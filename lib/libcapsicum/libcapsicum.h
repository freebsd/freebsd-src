/*-
 * Copyright (c) 2009 Robert N. M. Watson
 * All rights reserved.
 *
 * WARNING: THIS IS EXPERIMENTAL SECURITY SOFTWARE THAT MUST NOT BE RELIED
 * ON IN PRODUCTION SYSTEMS.  IT WILL BREAK YOUR SOFTWARE IN NEW AND
 * UNEXPECTED WAYS.
 * 
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc. 
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
 *
 * $P4: //depot/projects/trustedbsd/capabilities/src/lib/libcapsicum/libcapsicum.h#1 $
 */

#ifndef _LIBCAPABILITY_H_
#define	_LIBCAPABILITY_H_

#include <sys/cdefs.h>
#include <sys/capability.h>

__BEGIN_DECLS

struct lc_sandbox;
struct lc_host;

/*
 * Description of a library passed to lch_start_libs().
 */
struct lc_library {
	const char	*lcl_libpath;
	const char	*lcl_libname;
	int		 lcl_fd;
};


/* A list of file descriptors, which can be passed around in shared memory */
struct lc_fdlist;


struct lc_fdlist*	lc_fdlist_new(void);
struct lc_fdlist*	lc_fdlist_dup(struct lc_fdlist *orig);
void			lc_fdlist_free(struct lc_fdlist *l);

/* Size of an FD list in bytes, including all associated string data */
int	lc_fdlist_size(struct lc_fdlist *l);


/*
 * Add a file descriptor to the list.
 *
 * l		the list to add to
 * subsystem	a software component name, e.g. "org.freebsd.rtld-elf"
 * classname	a class name, e.g. "libdir" or "library"
 * name		an instance name, e.g. "system library dir" or "libc.so.6"
 * fd		the file descriptor
 */
int	lc_fdlist_add(struct lc_fdlist **l,
	              const char *subsystem, const char *classname,
	              const char *name, int fd);

/*
 * Like lc_fdlist_add(), but allows capability rights to be specified. The file
 * descriptor will be wrapped in a capability with the given rights (so if the
 * descriptor *is* a capability, its rights will be constrained according to this
 * rights mask)
 */
int	lc_fdlist_addcap(struct lc_fdlist **l,
	                 const char *subsystem, const char *classname,
	                 const char *name, int fd, cap_rights_t rights);

/*
 * Look up a file descriptor.
 *
 * Multiple entries with the same classname are allowed, so iterating through
 * all instances of a class is done by supplying an integer 'pos' which is used
 * internally to skip entries which have already been seen. If 'pos' is 0 or NULL,
 * the first matching entry will be returned.
 */
int	lc_fdlist_lookup(struct lc_fdlist *l,
	                 const char *subsystem, const char *classname,
	                 char **name, int *fdp, int *pos);

/*
 * Capability interfaces.
 */
int	lc_limitfd(int fd, cap_rights_t rights);

/*
 * Global policy interface to ask whether we should, in fact, sandbox a
 * particular optionally sandboxed service, by name.
 */
int	lch_autosandbox_isenabled(const char *servicename);

/*
 * Interfaces to start and stop capability mode sandboxs.
 */
int	lch_start(const char *sandbox, char *const argv[], u_int flags,
	    struct lc_sandbox **lcspp);
int	lch_start_libs(const char *sandbox, char *const argv[], u_int flags,
	    struct lc_library *lclp, u_int lcl_count,
	    struct lc_sandbox **lcspp);
int	lch_startfd(int fd_sandbox, const char *binname, char *const argv[],
	    u_int flags, struct lc_sandbox **lcspp);
int	lch_startfd_libs(int fd_sandbox, const char *binname,
	    char *const argv[], u_int flags, struct lc_library *lclp,
	    u_int lcl_count, struct lc_sandbox **lcspp);
void	lch_stop(struct lc_sandbox *lcsp);

/*
 * Flags to lch_start_flags:
 */
#define	LCH_PERMIT_STDERR	0x00000001
#define	LCH_PERMIT_STDOUT	0x00000002

/*
 * Interfaces to query state about capability mode sandboxs.
 */
int	lch_getsock(struct lc_sandbox *lcsp, int *fdp);
int	lch_getpid(struct lc_sandbox *lcsp, pid_t *pidp);
int	lch_getprocdesc(struct lc_sandbox *lcsp, int *fdp);

/*
 * Message-passing APIs for the host environment.
 */
struct iovec;
ssize_t	lch_recv(struct lc_sandbox *lcsp, void *buf, size_t len, int flags);
ssize_t	lch_recv_rights(struct lc_sandbox *lcsp, void *buf, size_t len,
	    int flags, int *fdp, int *fdcountp);
ssize_t	lch_send(struct lc_sandbox *lcsp, const void *msg, size_t len,
	    int flags);
ssize_t	lch_send_rights(struct lc_sandbox *lcsp, const void *msg, size_t len,
	    int flags, int *fdp, int fdcount);

/*
 * RPC APIs for the host environment.
 */
int	lch_rpc(struct lc_sandbox *lcsp, u_int32_t opno, struct iovec *req,
	    int reqcount, struct iovec *rep, int repcount, size_t *replenp);
int	lch_rpc_rights(struct lc_sandbox *lcsp, u_int32_t opno,
	    struct iovec *req, int reqcount, int *req_fdp, int req_fdcount,
	    struct iovec *rep, int repcount, size_t *replenp, int *rep_fdp,
	    int *rep_fdcountp);

/*
 * Interfaces to query state from within capability mode sandboxes.
 */
int	lcs_get(struct lc_host **lchpp);
int	lcs_getsock(struct lc_host *lchp, int *fdp);

/*
 * Message-passing APIs for the sandbox environment.
 */
ssize_t	lcs_recv(struct lc_host *lchp, void *buf, size_t len, int flags);
ssize_t	lcs_recv_rights(struct lc_host *lchp, void *buf, size_t len,
	    int flags, int *fdp, int *fdcountp);
ssize_t	lcs_send(struct lc_host *lchp, const void *msg, size_t len,
	    int flags);
ssize_t	lcs_send_rights(struct lc_host *lchp, const void *msg, size_t len,
	    int flags, int *fdp, int fdcount);

/*
 * RPC APIs for the sandbox environment.
 */
int	lcs_recvrpc(struct lc_host *lchp, u_int32_t *opnop,
	    u_int32_t *seqnop, u_char **bufferp, size_t *lenp);
int	lcs_recvrpc_rights(struct lc_host *lchp, u_int32_t *opnop,
	    u_int32_t *seqnop, u_char **bufferp, size_t *lenp, int *fdp,
	    int *fdcountp);
int	lcs_sendrpc(struct lc_host *lchp, u_int32_t opno, u_int32_t seqno,
	    struct iovec *rep, int repcount);
int	lcs_sendrpc_rights(struct lc_host *lchp, u_int32_t opno,
	    u_int32_t seqno, struct iovec *rep, int repcount, int *fdp,
	    int fdcount);

/*
 * Actually an rtld-elf-cap symbol, but declared here so it is available to
 * applications.
 */
int	ld_libcache_lookup(const char *libname, int *fdp);
int	ld_insandbox(void);

/*
 * Applications may declare an alternative entry point to the default ELF
 * entry point for their binary, which will be used in preference to 'main'
 * in the sandbox environment.
 */
int	cap_main(int argc, char *argv[]);

__END_DECLS

#endif /* !_LIBCAPABILITY_H_ */
