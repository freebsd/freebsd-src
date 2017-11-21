/*
 * Copyright (c) 2006 "David Kirchner" <dpk@dpk.net>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define L2CAP_SOCKET_CHECKED

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/capsicum.h>
#include <sys/extattr.h>
#include <sys/linker.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/procctl.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/rtprio.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/thr.h>
#include <sys/umtx.h>
#include <machine/sysarch.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/udplite.h>
#include <nfsserver/nfs.h>
#include <ufs/ufs/quota.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <aio.h>
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sysdecode.h>
#include <unistd.h>
#include <sys/bitstring.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_l2cap.h>
#include <netgraph/bluetooth/include/ng_btsocket.h>

/*
 * This is taken from the xlat tables originally in truss which were
 * in turn taken from strace.
 */
struct name_table {
	uintmax_t val;
	const char *str;
};

#define	X(a)	{ a, #a },
#define	XEND	{ 0, NULL }

#define	TABLE_START(n)	static struct name_table n[] = {
#define	TABLE_ENTRY	X
#define	TABLE_END	XEND };

#include "tables.h"

#undef TABLE_START
#undef TABLE_ENTRY
#undef TABLE_END

/*
 * These are simple support macros. print_or utilizes a variable
 * defined in the calling function to track whether or not it should
 * print a logical-OR character ('|') before a string. if_print_or
 * simply handles the necessary "if" statement used in many lines
 * of this file.
 */
#define print_or(fp,str,orflag) do {                     \
	if (orflag) fputc(fp, '|'); else orflag = true;  \
	fprintf(fp, str); }                              \
	while (0)
#define if_print_or(fp,i,flag,orflag) do {         \
	if ((i & flag) == flag)                    \
	print_or(fp,#flag,orflag); }               \
	while (0)

static const char *
lookup_value(struct name_table *table, uintmax_t val)
{

	for (; table->str != NULL; table++)
		if (table->val == val)
			return (table->str);
	return (NULL);
}

/*
 * Used when the value maps to a bitmask of #definition values in the
 * table.  This is a helper routine which outputs a symbolic mask of
 * matched masks.  Multiple masks are separated by a pipe ('|').
 * The value is modified on return to only hold unmatched bits.
 */
static void
print_mask_part(FILE *fp, struct name_table *table, uintmax_t *valp,
    bool *printed)
{
	uintmax_t rem;

	rem = *valp;
	for (; table->str != NULL; table++) {
		if ((table->val & rem) == table->val) {
			/*
			 * Only print a zero mask if the raw value is
			 * zero.
			 */
			if (table->val == 0 && *valp != 0)
				continue;
			fprintf(fp, "%s%s", *printed ? "|" : "", table->str);
			*printed = true;
			rem &= ~table->val;
		}
	}

	*valp = rem;
}

/*
 * Used when the value maps to a bitmask of #definition values in the
 * table.  The return value is true if something was printed.  If
 * rem is not NULL, *rem holds any bits not decoded if something was
 * printed.  If nothing was printed and rem is not NULL, *rem holds
 * the original value.
 */
static bool
print_mask_int(FILE *fp, struct name_table *table, int ival, int *rem)
{
	uintmax_t val;
	bool printed;

	printed = false;
	val = (unsigned)ival;
	print_mask_part(fp, table, &val, &printed);
	if (rem != NULL)
		*rem = val;
	return (printed);
}

/*
 * Used for a mask of optional flags where a value of 0 is valid.
 */
static bool
print_mask_0(FILE *fp, struct name_table *table, int val, int *rem)
{

	if (val == 0) {
		fputs("0", fp);
		if (rem != NULL)
			*rem = 0;
		return (true);
	}
	return (print_mask_int(fp, table, val, rem));
}

/*
 * Like print_mask_0 but for a unsigned long instead of an int.
 */
static bool
print_mask_0ul(FILE *fp, struct name_table *table, u_long lval, u_long *rem)
{
	uintmax_t val;
	bool printed;

	if (lval == 0) {
		fputs("0", fp);
		if (rem != NULL)
			*rem = 0;
		return (true);
	}

	printed = false;
	val = lval;
	print_mask_part(fp, table, &val, &printed);
	if (rem != NULL)
		*rem = val;
	return (printed);
}

static void
print_integer(FILE *fp, int val, int base)
{

	switch (base) {
	case 8:
		fprintf(fp, "0%o", val);
		break;
	case 10:
		fprintf(fp, "%d", val);
		break;
	case 16:
		fprintf(fp, "0x%x", val);
		break;
	default:
		abort2("bad base", 0, NULL);
		break;
	}
}

static bool
print_value(FILE *fp, struct name_table *table, uintmax_t val)
{
	const char *str;

	str = lookup_value(table, val);
	if (str != NULL) {
		fputs(str, fp);
		return (true);
	}
	return (false);
}

const char *
sysdecode_atfd(int fd)
{

	if (fd == AT_FDCWD)
		return ("AT_FDCWD");
	return (NULL);
}

bool
sysdecode_atflags(FILE *fp, int flag, int *rem)
{

	return (print_mask_int(fp, atflags, flag, rem));
}

static struct name_table semctlops[] = {
	X(GETNCNT) X(GETPID) X(GETVAL) X(GETALL) X(GETZCNT) X(SETVAL) X(SETALL)
	X(IPC_RMID) X(IPC_SET) X(IPC_STAT) XEND
};

const char *
sysdecode_semctl_cmd(int cmd)
{

	return (lookup_value(semctlops, cmd));
}

static struct name_table shmctlops[] = {
	X(IPC_RMID) X(IPC_SET) X(IPC_STAT) XEND
};

const char *
sysdecode_shmctl_cmd(int cmd)
{

	return (lookup_value(shmctlops, cmd));
}

const char *
sysdecode_msgctl_cmd(int cmd)
{

	return (sysdecode_shmctl_cmd(cmd));
}

static struct name_table semgetflags[] = {
	X(IPC_CREAT) X(IPC_EXCL) X(SEM_R) X(SEM_A) X((SEM_R>>3)) X((SEM_A>>3))
	X((SEM_R>>6)) X((SEM_A>>6)) XEND
};

bool
sysdecode_semget_flags(FILE *fp, int flag, int *rem)
{

	return (print_mask_int(fp, semgetflags, flag, rem));
}

static struct name_table idtypes[] = {
	X(P_PID) X(P_PPID) X(P_PGID) X(P_SID) X(P_CID) X(P_UID) X(P_GID)
	X(P_ALL) X(P_LWPID) X(P_TASKID) X(P_PROJID) X(P_POOLID) X(P_JAILID)
	X(P_CTID) X(P_CPUID) X(P_PSETID) XEND
};

/* XXX: idtype is really an idtype_t */
const char *
sysdecode_idtype(int idtype)
{

	return (lookup_value(idtypes, idtype));
}

/*
 * [g|s]etsockopt's level argument can either be SOL_SOCKET or a
 * protocol-specific value.
 */
const char *
sysdecode_sockopt_level(int level)
{
	const char *str;

	if (level == SOL_SOCKET)
		return ("SOL_SOCKET");

	/* SOL_* constants for Bluetooth sockets. */
	str = lookup_value(ngbtsolevel, level);
	if (str != NULL)
		return (str);

	/*
	 * IP and Infiniband sockets use IP protocols as levels.  Not all
	 * protocols are valid but it is simpler to just allow all of them.
	 *
	 * XXX: IPPROTO_IP == 0, but UNIX domain sockets use a level of 0
	 * for private options.
	 */
	str = sysdecode_ipproto(level);
	if (str != NULL)
		return (str);

	return (NULL);
}

bool
sysdecode_vmprot(FILE *fp, int type, int *rem)
{

	return (print_mask_int(fp, vmprot, type, rem));
}

static struct name_table sockflags[] = {
	X(SOCK_CLOEXEC) X(SOCK_NONBLOCK) XEND
};

bool
sysdecode_socket_type(FILE *fp, int type, int *rem)
{
	const char *str;
	uintmax_t val;
	bool printed;

	str = lookup_value(socktype, type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK));
	if (str != NULL) {
		fputs(str, fp);
		*rem = 0;
		printed = true;
	} else {
		*rem = type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK);
		printed = false;
	}
	val = type & (SOCK_CLOEXEC | SOCK_NONBLOCK);
	print_mask_part(fp, sockflags, &val, &printed);
	return (printed);
}

bool
sysdecode_access_mode(FILE *fp, int mode, int *rem)
{

	return (print_mask_int(fp, accessmode, mode, rem));
}

/* XXX: 'type' is really an acl_type_t. */
const char *
sysdecode_acltype(int type)
{

	return (lookup_value(acltype, type));
}

bool
sysdecode_cap_fcntlrights(FILE *fp, uint32_t rights, uint32_t *rem)
{

	return (print_mask_int(fp, capfcntl, rights, rem));
}

const char *
sysdecode_extattrnamespace(int namespace)
{

	return (lookup_value(extattrns, namespace));
}

const char *
sysdecode_fadvice(int advice)
{

	return (lookup_value(fadvisebehav, advice));
}

bool
sysdecode_open_flags(FILE *fp, int flags, int *rem)
{
	bool printed;
	int mode;
	uintmax_t val;

	mode = flags & O_ACCMODE;
	flags &= ~O_ACCMODE;
	switch (mode) {
	case O_RDONLY:
		if (flags & O_EXEC) {
			flags &= ~O_EXEC;
			fputs("O_EXEC", fp);
		} else
			fputs("O_RDONLY", fp);
		printed = true;
		mode = 0;
		break;
	case O_WRONLY:
		fputs("O_WRONLY", fp);
		printed = true;
		mode = 0;
		break;
	case O_RDWR:
		fputs("O_RDWR", fp);
		printed = true;
		mode = 0;
		break;
	default:
		printed = false;
	}
	val = (unsigned)flags;
	print_mask_part(fp, openflags, &val, &printed);
	if (rem != NULL)
		*rem = val | mode;
	return (printed);
}

bool
sysdecode_fcntl_fileflags(FILE *fp, int flags, int *rem)
{
	bool printed;
	int oflags;

	/*
	 * The file flags used with F_GETFL/F_SETFL mostly match the
	 * flags passed to open(2).  However, a few open-only flag
	 * bits have been repurposed for fcntl-only flags.
	 */
	oflags = flags & ~(O_NOFOLLOW | FRDAHEAD);
	printed = sysdecode_open_flags(fp, oflags, rem);
	if (flags & O_NOFOLLOW) {
		fprintf(fp, "%sFPOIXSHM", printed ? "|" : "");
		printed = true;
	}
	if (flags & FRDAHEAD) {
		fprintf(fp, "%sFRDAHEAD", printed ? "|" : "");
		printed = true;
	}
	return (printed);
}

bool
sysdecode_flock_operation(FILE *fp, int operation, int *rem)
{

	return (print_mask_int(fp, flockops, operation, rem));
}

static struct name_table getfsstatmode[] = {
	X(MNT_WAIT) X(MNT_NOWAIT) XEND
};

const char *
sysdecode_getfsstat_mode(int mode)
{

	return (lookup_value(getfsstatmode, mode));
}

const char *
sysdecode_getrusage_who(int who)
{

	return (lookup_value(rusage, who));
}

const char *
sysdecode_kldsym_cmd(int cmd)
{

	return (lookup_value(kldsymcmd, cmd));
}

const char *
sysdecode_kldunload_flags(int flags)
{

	return (lookup_value(kldunloadfflags, flags));
}

const char *
sysdecode_lio_listio_mode(int mode)
{

	return (lookup_value(lio_listiomodes, mode));
}

const char *
sysdecode_madvice(int advice)
{

	return (lookup_value(madvisebehav, advice));
}

const char *
sysdecode_minherit_inherit(int inherit)
{

	return (lookup_value(minheritflags, inherit));
}

bool
sysdecode_mlockall_flags(FILE *fp, int flags, int *rem)
{

	return (print_mask_int(fp, mlockallflags, flags, rem));
}

bool
sysdecode_mmap_prot(FILE *fp, int prot, int *rem)
{

	return (print_mask_int(fp, mmapprot, prot, rem));
}

bool
sysdecode_fileflags(FILE *fp, fflags_t flags, fflags_t *rem)
{

	return (print_mask_0(fp, fileflags, flags, rem));
}

bool
sysdecode_filemode(FILE *fp, int mode, int *rem)
{

	return (print_mask_0(fp, filemode, mode, rem));
}

bool
sysdecode_mount_flags(FILE *fp, int flags, int *rem)
{

	return (print_mask_int(fp, mountflags, flags, rem));
}

bool
sysdecode_msync_flags(FILE *fp, int flags, int *rem)
{

	return (print_mask_int(fp, msyncflags, flags, rem));
}

const char *
sysdecode_nfssvc_flags(int flags)
{

	return (lookup_value(nfssvcflags, flags));
}

static struct name_table pipe2flags[] = {
	X(O_CLOEXEC) X(O_NONBLOCK) XEND
};

bool
sysdecode_pipe2_flags(FILE *fp, int flags, int *rem)
{

	return (print_mask_0(fp, pipe2flags, flags, rem));
}

const char *
sysdecode_prio_which(int which)
{

	return (lookup_value(prio, which));
}

const char *
sysdecode_procctl_cmd(int cmd)
{

	return (lookup_value(procctlcmd, cmd));
}

const char *
sysdecode_ptrace_request(int request)
{

	return (lookup_value(ptraceop, request));
}

static struct name_table quotatypes[] = {
	X(GRPQUOTA) X(USRQUOTA) XEND
};

bool
sysdecode_quotactl_cmd(FILE *fp, int cmd)
{
	const char *primary, *type;

	primary = lookup_value(quotactlcmds, cmd >> SUBCMDSHIFT);
	if (primary == NULL)
		return (false);
	fprintf(fp, "QCMD(%s,", primary);
	type = lookup_value(quotatypes, cmd & SUBCMDMASK);
	if (type != NULL)
		fprintf(fp, "%s", type);
	else
		fprintf(fp, "%#x", cmd & SUBCMDMASK);
	fprintf(fp, ")");
	return (true);
}

bool
sysdecode_reboot_howto(FILE *fp, int howto, int *rem)
{
	bool printed;

	/*
	 * RB_AUTOBOOT is special in that its value is zero, but it is
	 * also an implied argument if a different operation is not
	 * requested via RB_HALT, RB_POWEROFF, or RB_REROOT.
	 */
	if (howto != 0 && (howto & (RB_HALT | RB_POWEROFF | RB_REROOT)) == 0) {
		fputs("RB_AUTOBOOT|", fp);
		printed = true;
	} else
		printed = false;
	return (print_mask_int(fp, rebootopt, howto, rem) || printed);
}

bool
sysdecode_rfork_flags(FILE *fp, int flags, int *rem)
{

	return (print_mask_int(fp, rforkflags, flags, rem));
}

const char *
sysdecode_rlimit(int resource)
{

	return (lookup_value(rlimit, resource));
}

const char *
sysdecode_scheduler_policy(int policy)
{

	return (lookup_value(schedpolicy, policy));
}

bool
sysdecode_sendfile_flags(FILE *fp, int flags, int *rem)
{

	return (print_mask_int(fp, sendfileflags, flags, rem));
}

bool
sysdecode_shmat_flags(FILE *fp, int flags, int *rem)
{

	return (print_mask_int(fp, shmatflags, flags, rem));
}

const char *
sysdecode_shutdown_how(int how)
{

	return (lookup_value(shutdownhow, how));
}

const char *
sysdecode_sigbus_code(int si_code)
{

	return (lookup_value(sigbuscode, si_code));
}

const char *
sysdecode_sigchld_code(int si_code)
{

	return (lookup_value(sigchldcode, si_code));
}

const char *
sysdecode_sigfpe_code(int si_code)
{

	return (lookup_value(sigfpecode, si_code));
}

const char *
sysdecode_sigill_code(int si_code)
{

	return (lookup_value(sigillcode, si_code));
}

const char *
sysdecode_sigsegv_code(int si_code)
{

	return (lookup_value(sigsegvcode, si_code));
}

const char *
sysdecode_sigtrap_code(int si_code)
{

	return (lookup_value(sigtrapcode, si_code));
}

const char *
sysdecode_sigprocmask_how(int how)
{

	return (lookup_value(sigprocmaskhow, how));
}

const char *
sysdecode_socketdomain(int domain)
{

	return (lookup_value(sockdomain, domain));
}

const char *
sysdecode_socket_protocol(int domain, int protocol)
{

	switch (domain) {
	case PF_INET:
	case PF_INET6:
		return (lookup_value(sockipproto, protocol));
	default:
		return (NULL);
	}
}

const char *
sysdecode_sockaddr_family(int sa_family)
{

	return (lookup_value(sockfamily, sa_family));
}

const char *
sysdecode_ipproto(int protocol)
{

	return (lookup_value(sockipproto, protocol));
}

const char *
sysdecode_sockopt_name(int level, int optname)
{

	if (level == SOL_SOCKET)
		return (lookup_value(sockopt, optname));
	if (level == IPPROTO_IP)
		/* XXX: UNIX domain socket options use a level of 0 also. */
		return (lookup_value(sockoptip, optname));
	if (level == IPPROTO_IPV6)
		return (lookup_value(sockoptipv6, optname));
	if (level == IPPROTO_SCTP)
		return (lookup_value(sockoptsctp, optname));
	if (level == IPPROTO_TCP)
		return (lookup_value(sockopttcp, optname));
	if (level == IPPROTO_UDP)
		return (lookup_value(sockoptudp, optname));
	if (level == IPPROTO_UDPLITE)
		return (lookup_value(sockoptudplite, optname));
	return (NULL);
}

bool
sysdecode_thr_create_flags(FILE *fp, int flags, int *rem)
{

	return (print_mask_int(fp, thrcreateflags, flags, rem));
}

const char *
sysdecode_umtx_op(int op)
{

	return (lookup_value(umtxop, op));
}

const char *
sysdecode_vmresult(int result)
{

	return (lookup_value(vmresult, result));
}

bool
sysdecode_wait4_options(FILE *fp, int options, int *rem)
{
	bool printed;
	int opt6;

	/* A flags value of 0 is normal. */
	if (options == 0) {
		fputs("0", fp);
		if (rem != NULL)
			*rem = 0;
		return (true);
	}

	/*
	 * These flags are implicit and aren't valid flags for wait4()
	 * directly (though they don't fail with EINVAL).
	 */
	opt6 = options & (WEXITED | WTRAPPED);
	options &= ~opt6;
	printed = print_mask_int(fp, wait6opt, options, rem);
	if (rem != NULL)
		*rem |= opt6;
	return (printed);
}

bool
sysdecode_wait6_options(FILE *fp, int options, int *rem)
{

	return (print_mask_int(fp, wait6opt, options, rem));
}

const char *
sysdecode_whence(int whence)
{

	return (lookup_value(seekwhence, whence));
}

const char *
sysdecode_fcntl_cmd(int cmd)
{

	return (lookup_value(fcntlcmd, cmd));
}

static struct name_table fcntl_fd_arg[] = {
	X(FD_CLOEXEC) X(0) XEND
};

bool
sysdecode_fcntl_arg_p(int cmd)
{

	switch (cmd) {
	case F_GETFD:
	case F_GETFL:
	case F_GETOWN:
		return (false);
	default:
		return (true);
	}
}

void
sysdecode_fcntl_arg(FILE *fp, int cmd, uintptr_t arg, int base)
{
	int rem;

	switch (cmd) {
	case F_SETFD:
		if (!print_value(fp, fcntl_fd_arg, arg))
		    print_integer(fp, arg, base);
		break;
	case F_SETFL:
		if (!sysdecode_fcntl_fileflags(fp, arg, &rem))
			fprintf(fp, "%#x", rem);
		else if (rem != 0)
			fprintf(fp, "|%#x", rem);
		break;
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		fprintf(fp, "%p", (void *)arg);
		break;
	default:
		print_integer(fp, arg, base);
		break;
	}
}

bool
sysdecode_mmap_flags(FILE *fp, int flags, int *rem)
{
	uintmax_t val;
	bool printed;
	int align;

	/*
	 * MAP_ALIGNED can't be handled directly by print_mask_int().
	 * MAP_32BIT is also problematic since it isn't defined for
	 * all platforms.
	 */
	printed = false;
	align = flags & MAP_ALIGNMENT_MASK;
	val = (unsigned)flags & ~MAP_ALIGNMENT_MASK;
	print_mask_part(fp, mmapflags, &val, &printed);
#ifdef MAP_32BIT
	if (val & MAP_32BIT) {
		fprintf(fp, "%sMAP_32BIT", printed ? "|" : "");
		printed = true;
		val &= ~MAP_32BIT;
	}
#endif
	if (align != 0) {
		if (printed)
			fputc('|', fp);
		if (align == MAP_ALIGNED_SUPER)
			fputs("MAP_ALIGNED_SUPER", fp);
		else
			fprintf(fp, "MAP_ALIGNED(%d)",
			    align >> MAP_ALIGNMENT_SHIFT);
		printed = true;
	}
	if (rem != NULL)
		*rem = val;
	return (printed);
}

const char *
sysdecode_pathconf_name(int name)
{

	return (lookup_value(pathconfname, name));
}

const char *
sysdecode_rtprio_function(int function)
{

	return (lookup_value(rtpriofuncs, function));
}

bool
sysdecode_msg_flags(FILE *fp, int flags, int *rem)
{

	return (print_mask_0(fp, msgflags, flags, rem));
}

const char *
sysdecode_sigcode(int sig, int si_code)
{
	const char *str;

	str = lookup_value(sigcode, si_code);
	if (str != NULL)
		return (str);
	
	switch (sig) {
	case SIGILL:
		return (sysdecode_sigill_code(si_code));
	case SIGBUS:
		return (sysdecode_sigbus_code(si_code));
	case SIGSEGV:
		return (sysdecode_sigsegv_code(si_code));
	case SIGFPE:
		return (sysdecode_sigfpe_code(si_code));
	case SIGTRAP:
		return (sysdecode_sigtrap_code(si_code));
	case SIGCHLD:
		return (sysdecode_sigchld_code(si_code));
	default:
		return (NULL);
	}
}

const char *
sysdecode_sysarch_number(int number)
{

	return (lookup_value(sysarchnum, number));
}

bool
sysdecode_umtx_cvwait_flags(FILE *fp, u_long flags, u_long *rem)
{

	return (print_mask_0ul(fp, umtxcvwaitflags, flags, rem));
}

bool
sysdecode_umtx_rwlock_flags(FILE *fp, u_long flags, u_long *rem)
{

	return (print_mask_0ul(fp, umtxrwlockflags, flags, rem));
}

void
sysdecode_cap_rights(FILE *fp, cap_rights_t *rightsp)
{
	struct name_table *t;
	int i;
	bool comma;

	for (i = 0; i < CAPARSIZE(rightsp); i++) {
		if (CAPIDXBIT(rightsp->cr_rights[i]) != 1 << i) {
			fprintf(fp, "invalid cap_rights_t");
			return;
		}
	}
	comma = false;
	for (t = caprights; t->str != NULL; t++) {
		if (cap_rights_is_set(rightsp, t->val)) {
			fprintf(fp, "%s%s", comma ? "," : "", t->str);
			comma = true;
		}
	}
}
