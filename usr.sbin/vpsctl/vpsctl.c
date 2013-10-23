/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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
 * $Id: vpsctl.c 171 2013-06-11 16:35:58Z klaus $
 *
 */

/*
 * cc -g -Wall -Werror -o vpsctl vpsctl.c -I/usr/src/sys
 */

#include <machine/cputypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/ioccom.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/ttycom.h>
#include <sys/sockio.h>
#include <sys/priv.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <signal.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>
#include <assert.h>

#ifndef VIMAGE
#define VIMAGE	1
#endif
#ifndef VPS
#define VPS	1
#endif

#include <vps/vps_user.h>
#include <vps/vps_account.h>

#include <vps/vps_libdump.h>

#include "vpsctl.h"

#define s6_addr8  __u6_addr.__u6_addr8
#define s6_addr16 __u6_addr.__u6_addr16
#define s6_addr32 __u6_addr.__u6_addr32

#define LINE_MAXLEN	0x400

#define DEFAULT_VPSIF	"vps0"

#define RSYNC_MODE_CLIENT 1
#define RSYNC_MODE_SERVER 2

#ifdef DEBUG
#define DBGfprintf	fprintf
#else
#define	DBGfprintf	nofprintf	
int nofprintf(FILE __unused *fp, const char __unused *fmt, ...);
int nofprintf(FILE __unused *fp, const char __unused *fmt, ...)
{
	return (0);
}
#endif

/* Default TTY escape pattern. */
static const char def_escape_pattern[] = 
    { 0x0D, 0x23, 0x2E, 0x00, };
static const char def_escape_message[] =
    "Escape sequence: <enter>#.\n";

int vpsfd;
char **vc_envv;
int fscale;

int mig_did_suspend;
int mig_did_revoke;
char *mig_vps;
struct vps_conf mig_vc;

int vc_read_config(char *file_n, struct vps_conf *vc);
char *vc_trim_whitespace(char *s);
int vc_exec_cmd(char *cmd, int quiet);
int vc_usage(FILE *);
int vc_list(int, char **argv);
int vc_shell(int, char **argv);
int vc_show(int, char **);
int vc_start(int, char **);
int vc_stop(int, char **);
int vc_exec(int, char **, char **);
int vc_ifmove(int, char **);
int vc_suspend(int, char **);
int vc_snapshot(int, char **, int, int);
int vc_restore(int, char **);
int vc_abort(int, char **);
int vc_migrate(int argc, char **argv);
int vc_sshtest(int argc, char **argv);
int vc_get_ssh_transport(char *host, int *pid, int *rfd, int *wfd);
int vc_close_ssh_transport(int pid);
const char *vc_statusstr(int status);
int vc_net_publish(struct vps_conf *cf);
int vc_net_revoke(struct vps_conf *cf);
int vc_rsync(int, int, int, char *);
int vc_argtest(int, char **);
int vc_arg_show(int, char **);
int vc_allow_ip4net(const char *, in_addr_t, in_addr_t);
int vc_allow_ip6net(const char *, struct in6_addr *, u_int8_t *);
int vc_arg_ipnet(int, char **);
int vc_arg_priv(int, char **);
int vc_arg_limit(int, char **);
int vc_quota_recalc(int, char **);
int vc_showdump(int, char **);
int vc_console(int, char **);
int vc_savefile(int, char **);
static int vc_ttyloop(int ptsmfd, const char *esc_pattern);

int
main(int argc, char **argv, char **envv)
{
	size_t sysctl_len;
	int rc;

	if (argc < 2)
		return (vc_usage(stdout));

	vc_envv = envv;

	sysctl_len = sizeof(fscale);
	if ((rc = sysctlbyname("kern.fscale", &fscale, &sysctl_len, NULL, 0)) == -1) {
		fprintf(stderr, "sysctlbyname(\"kern.fscale\", ...): %s\n",
			strerror(errno));
		return (-1);
	}

	if ((vpsfd = open(_PATH_VPSDEV, O_RDWR)) == -1) {
		fprintf(stderr, "open(%s): %s\n",
				_PATH_VPSDEV, strerror(errno));
		return (-1);
	}

	if (strcmp(argv[1], "start") == 0) {
		rc = vc_start(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "stop") == 0) {
		rc = vc_stop(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "execin") == 0) {
		rc = vc_exec(argc-1, &argv[1], NULL);
	} else
	if (strcmp(argv[1], "execwt") == 0) {
		rc = vc_exec(argc-1, &argv[1], NULL);
	} else
	if (strcmp(argv[1], "shell") == 0) {
		rc = vc_shell(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "ifmove") == 0) {
		rc = vc_ifmove(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "list") == 0) {
		rc = vc_list(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "show") == 0) {
		rc = vc_show(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "suspend") == 0) {
		rc = vc_suspend(argc-1, &argv[1]);
	} else
	if (strcmp(argv[1], "resume") == 0) {
		rc = vc_suspend(argc-1, &argv[1]);
	} else
	if (strcmp(argv[1], "snapshot") == 0) {
		rc = vc_snapshot(argc-2, &argv[2], -1, 0);
	} else
	if (strcmp(argv[1], "restore") == 0) {
		rc = vc_restore(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "abort") == 0) {
		rc = vc_abort(argc-1, &argv[1]);
	} else
	if (strcmp(argv[1], "migrate") == 0) {
		rc = vc_migrate(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "rsyncserver") == 0) {
		if (argc < 2)
			return (-1);
		rc = vc_rsync(RSYNC_MODE_SERVER, 0, 1, argv[2]);
	} else
	if (strcmp(argv[1], "savefile") == 0) {
		rc = vc_savefile(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "argshow") == 0) {
		rc = vc_arg_show(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "ipnet") == 0) {
		rc = vc_arg_ipnet(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "priv") == 0) {
		rc = vc_arg_priv(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "limit") == 0) {
		rc = vc_arg_limit(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "quotarecalc") == 0) {
		rc = vc_quota_recalc(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "showdump") == 0) {
		rc = vc_showdump(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "console") == 0) {
		rc = vc_console(argc-2, &argv[2]);
	} else
#if 0
	// debug / test code, move or delete at some point
	if (strcmp(argv[1], "sshtest") == 0) {
		rc = vc_sshtest(argc-2, &argv[2]);
	} else
	if (strcmp(argv[1], "rsync") == 0) {
		int pid, rfd, wfd, len;
		char cmd[0x100];

		if (argc < 3)
			return (-1);
		if ((vc_get_ssh_transport(argv[2], &pid, &rfd, &wfd)))
			return (-1);

		snprintf(cmd, sizeof(cmd), "vpsctl rsyncserver %s\n",
			argv[3]);
		write(wfd, cmd, strlen(cmd));
		rc = vc_rsync(RSYNC_MODE_CLIENT, rfd, wfd, argv[3]);

		/* See if our remote shell still works. */
		snprintf(cmd, sizeof(cmd), "uptime\n");
		write(wfd, cmd, strlen(cmd));
		len = read(rfd, cmd, sizeof(cmd));
		if (len == -1)
			fprintf(stderr, "read error: %s\n", strerror(errno));
		cmd[len] = '\0';
		fprintf(stderr, "len=%d cmd=[%s]\n", len, cmd);

		vc_close_ssh_transport(pid);
	} else
	if (strcmp(argv[1], "argtest") == 0) {
		rc = vc_argtest(argc-2, &argv[2]);
	} else
#endif /* 0 */
	{
		rc = vc_usage(stdout);
	}

	if (close(vpsfd) == -1) {
		fprintf(stderr, "%s: close: %s\n",
			__func__, strerror(errno));
	}

	return (rc);
}

int
vc_usage(FILE *out)
{
	fprintf(out, 
		"usage: \n"
		"\n"
		"vpsctl \n"
		"      start    <id> <file>                 \n"
		"      stop     <id> <file>                 \n"
		"      list                                 \n"
		"      show     <id>                        \n"
		/*"      update   <id> <file>                 \n"*/
		"      execin   <id> <cmd> [args ...]       \n"
		"      execwt   <id> <cmd> [args ...]       \n"
		"      shell    <id>                        \n"
		"      console  <id>                        \n"
		"      ifmove   <id> <ifname> [ifnewname]   \n"
		"      suspend  <id>                        \n"
		"      resume   <id>                        \n"
		"      snapshot <id> <file>                 \n"
		"      abort    <id>                        \n"
		"      restore  <id> <file>                 \n"
		"      migrate  <id> <remote-host> [norsync|onersync|tworsync]\n"
		"                                           \n"
		"      argshow  <id>                        \n"
		"      ipnet    <id> add <address/network, ...> \n"
		"      ipnet    <id> rem <address/network, ...> \n"
		"      priv     <id> allow <privilege number, ...> \n"
		"      priv     <id> deny  <privilege number, ...> \n"
		"      priv     <id> nosys <privilege number, ...> \n"
		"      limit    <id> <resource:soft:hard,...> \n"
		"                                           \n"
		"      showdump <file> (attention: loads of output !)\n"
		"\n"
	);
	return (out == stdout ? 0 : -1);
}

const char *
vc_statusstr(int status)
{
    switch (status) {
		case VPS_ST_CREATING:
			return ("creating");
		case VPS_ST_RUNNING:
			return ("running");
		case VPS_ST_SUSPENDED:
			return ("suspended");
		case VPS_ST_SNAPSHOOTING:
			return ("snapshooting");
		case VPS_ST_RESTORING:
			return ("restoring");
		case VPS_ST_DYING:
			return ("dying");
		case VPS_ST_DEAD:
			return ("dead");
		default:
			return ("unknown");
	}
}

int
vc_shell(int argc, char **argv)
{
	char *argv2[4];
	char *envv2[4];
	char *s;
	char str1[] = "execwt";
	char str2[] = "/bin/sh";
	char str3[] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/games:"
		"/usr/local/sbin:/usr/local/bin:/root/bin";
	char str4[0x400];
	char str5[0x400];
	int rc;

	if (argc < 1)
		return (vc_usage(stderr));

	envv2[0] = str3;
	envv2[3] = NULL;

	if ((s = getenv("TERM")) != 0) {
		snprintf(str4, sizeof(str4), "TERM=%s", s);
		envv2[1] = str4;
	} else
		envv2[1] = NULL;

	if ((s = getenv("TERMCAP")) != 0) {
		snprintf(str5, sizeof(str5), "TERM=%s", s);
		envv2[2] = str5;
	} else
		envv2[2] = NULL;

	argv2[0] = str1;
	argv2[1] = argv[0];
	if ((argv2[2] = getenv("SHELL")) == NULL)
		argv2[2] = str2;
	argv2[3] = NULL;
	
	rc = vc_exec(3, argv2, envv2);

	return (rc);
}

int
vc_console(int argc, char **argv)
{
	struct vps_arg_getconsfd va;
	int consfd;
	int rc;

	if (argc < 1)
		return (vc_usage(stderr));

	memset(&va, 0, sizeof(va));
	snprintf(va.vps_name, sizeof(va.vps_name), "%s", argv[0]);
	if ((rc = ioctl(vpsfd, VPS_IOC_GETCONSFD, &va)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_GETCONSFD: %s\n",
			strerror(errno));
		return (-1);
	}
	consfd = va.consfd;

	fprintf(stdout, def_escape_message);
	vc_ttyloop(consfd, def_escape_pattern);

	return (rc);
}
	
int
vc_list(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
	struct vps_info *info, *info0;
	int rc, cnt, i;

	if ((rc = ioctl(vpsfd, VPS_IOC_LIST, &cnt)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_LIST: %s\n",
			strerror(errno));
		return (-1);
	}
	
	if (cnt <= 0)
		return (rc);

	if ((info0 = mmap(NULL, cnt * sizeof(struct vps_info), 
		PROT_READ, 0, vpsfd, 0)) == MAP_FAILED) {
		fprintf(stderr, "mmap: %s\n", strerror(errno));
		return (rc);
	}

	fprintf(stdout, "%-20s %-8s %-28s %4s %14s\n",
		"Name", "State", "VFS Root", "%CPU", "Virtual Size");

	for (i = 0; i < cnt; i++) {
		info = info0 + i;

		fprintf(stdout, "%-20s %-8s %-28s %4lu %14lu\n",
			info->name,
			vc_statusstr(info->status),
			info->fsroot,
			info->acc.pctcpu / (fscale / 100),
			info->acc.virt);
	}

	munmap(info0, cnt * sizeof(struct vps_info));

	return (rc);
}

int
vc_suspend(int argc, char **argv)
{
	struct vps_arg_flags va;
	u_long cmd;

	if (argc < 2)
		return (vc_usage(stderr));

	if (strcmp(argv[0], "suspend") == 0)
		cmd = VPS_IOC_SUSPND;
	else if (strcmp (argv[0], "resume") == 0)
		cmd = VPS_IOC_RESUME;
	else
		return (vc_usage(stderr));

	snprintf(va.vps_name, sizeof(va.vps_name), "%s", argv[1]);
	va.flags = 0;
	va.flags |= VPS_SUSPEND_RELINKFILES;
	if (argc > 2 && strcmp(argv[2], "norelinkfiles")==0)
		va.flags &= ~VPS_SUSPEND_RELINKFILES;

	if ((ioctl(vpsfd, cmd, &va)) == -1) {
		fprintf(stderr, "ioctl %s: %s\n",
			cmd == VPS_IOC_SUSPND ? "VPS_IOC_SUSPND" : "VPS_IOC_RESUME",
			strerror(errno));
		return (errno);
	}

	if (argc > 2 && strcmp(argv[2], "remote") == 0)
		fprintf(stdout, "SUCCESS\n");

	return (0);
}

int
vc_abort(int argc, char **argv)
{
	struct vps_arg_flags va;
	long cmd;

	if (argc < 2)
		return (vc_usage(stderr));

	cmd = VPS_IOC_ABORT;

	snprintf(va.vps_name, sizeof(va.vps_name), "%s", argv[1]);
	va.flags = 0;

	if ((ioctl(vpsfd, cmd, &va)) == -1) {
		fprintf(stderr, "ioctl %s: %s\n", "VPS_IOC_ABORT", strerror(errno));
		return (errno);
	}

	return (0);
}

int
vc_snapshot(int argc, char **argv, int outfd, int verbose)
{
	struct vps_arg_snapst va;
	int out, len, rv, wlen, one, i;
	int *base;
	char str2[0x100];
	//char tmpc, *tmpp;
	char *errbuf;

	if (argc < 2)
		return (vc_usage(stderr));

	if (outfd > -1)
		out = outfd;
	else if (strcmp(argv[1], "-") == 0)
		out = 1; /* stdout */
	else
		if ((out = open(argv[1], O_CREAT|O_WRONLY)) == -1) {
			fprintf(stderr, "open([%s]): %s\n",
				argv[1], strerror(errno));
			return (-1);
		}

	if (verbose) {
		fprintf(stderr, "Taking snapshot ... ");
		fflush(stdout);
	}


	memset(&va, 0, sizeof (va));
	va.msglen = 0x10000;
	errbuf = malloc(va.msglen);
	va.msgbase = errbuf;
	strncpy(va.vps_name, argv[0], sizeof(va.vps_name));

	if ((ioctl(vpsfd, VPS_IOC_SNAPST, &va)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_SNAPST: %s\n",
			strerror(errno));
		fprintf(stderr,
			"Kernel error messages:\n"
			"------------------------\n"
			"%s"
			"------------------------\n",
			errbuf);
		free(errbuf);
		close(out);
		return (errno);
	}
	base = (void*)va.database;
	len = va.datalen;
	DBGfprintf(stderr, "base = %p, len = %d\n", base, len);

	if (verbose) {
		fprintf(stderr, "done\n");
		fflush(stderr);
	}

	//fprintf(stderr, "errbuf: [%s]\n", errbuf);
	free(errbuf);

	/*
	 * XXX
	 * Since 9.0 on i386, writing out the mmaped area to a pipe
	 * to ssh, produces random data.
	 * A pipe to cat always reproduces the correct data.
	 * For now touch every single page as a workaround before write().
	 */
#if defined(CPU_386)
#if 0
	for (tmpp = (char *)base; tmpp < (char *)base + len; tmpp += PAGE_SIZE)
		tmpc = *tmpp;
#else
	{
	int *base2;
	base2 = malloc(len);
	memcpy(base2, base, len);
	base = base2;
	}
#endif
#endif /* CPU_386 */

	if (verbose) {
		fprintf(stderr, "Transferring dump (%d MB) ", len/1024/1024);
		fflush(stderr);
	}

	one = roundup(len / 100, 8);
	
	i = 0;
	wlen = 0;
	while (wlen < len) {
		if ((rv = write(out, ((char *)base)+wlen, MIN(len-wlen, one))) == -1) {
			fprintf(stderr, "write: %s\n", strerror(errno));
			return (-1);
		}
		wlen += rv;
		if (verbose) {
			if (i != 0 && i != 100 && (i % 10) == 0)
				snprintf(str2, sizeof(str2), "(%d%%)", i);
			else
				str2[0] = '\0';
			fprintf(stderr, ".%s", str2);
			fflush(stderr);
		}
		i++;
	}

	if (verbose) {
		fprintf(stderr, "(100%%) done\n");
		fflush(stderr);
	}

	if ( ! (out == 1 || outfd > -1) )
		close(out);

	return (0);
}

int
vc_restore(int argc, char **argv)
{
	struct vps_arg_snapst va;
	struct vps_dumpheader *dumphdr;
	//struct stat statbuf;
	int in, len, rv;
	int have_header;
	unsigned int rlen;
	/* int *base; */
	void *buf;
	struct vps_conf vc;
	char file_n[MAXPATHLEN];
	char *errbuf;

	if (argc < 2)
		return (vc_usage(stderr));

	snprintf(file_n, MAXPATHLEN, "%s/vps_%s.conf",
			_PATH_CONFDIR, argv[0]);
	memset(&vc, 0, sizeof(struct vps_conf));
	if (vc_read_config(file_n, &vc)) {
		fprintf(stderr, "config not found\n");
		return (1);
	}

	if (strcmp(argv[1], "-") == 0)
		in = 0; /* stdin */
	else
		if ((in = open(argv[1], O_RDONLY)) == -1) {
			fprintf(stderr, "open([%s]): %s\n",
				argv[1], strerror(errno));
			return (-1);
		}

	/*
	if ((fstat(in, &statbuf)) == -1) {
		fprintf(stderr, "fstat(): %s\n", strerror(errno));
		close(in);
		return (-1);
	}
	len = statbuf.st_size;
	*/

	len = 0x100;
	buf = malloc(len);
	rlen = 0;
	have_header = 0;
	while ((rv = read(in, ((char*)buf)+rlen, len-rlen)) > 0) {
		if (rv == -1) {
			fprintf(stderr, "read: %s\n", strerror(errno));
			return (-1);
		}
		rlen += rv;
		if (have_header == 0 && rlen >= sizeof (*dumphdr)) {
			dumphdr = (struct vps_dumpheader *)buf;
			len = dumphdr->size;
			DBGfprintf(stderr, "size=%lu\n", dumphdr->size);
			buf = realloc(buf, len);
			have_header = 1;
		}
	}

	DBGfprintf(stdout, "rlen = %d\n", rlen);

	memset(&va, 0, sizeof(va));
	va.msglen = 0x10000;
	errbuf = malloc(va.msglen);
	va.msgbase = errbuf;
	strncpy(va.vps_name, argv[0], sizeof(va.vps_name));
	va.database = buf;
	va.datalen = rlen;

	if ((ioctl(vpsfd, VPS_IOC_RESTOR, &va)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_RESTOR: %s [%s]\n",
			strerror(errno), va.errmsg);
		fprintf(stderr,
			"Kernel error messages:\n"
			"------------------------\n"
			"%s"
			"------------------------\n",
			errbuf);
		free(errbuf);
		close(in);
		return (errno);
	}

	//fprintf(stderr, "errbuf: [%s]\n", errbuf);
	free(errbuf);

	if (in != 0)
		close(in);

	vc_net_publish(&vc);

	if (argc > 2 && strcmp(argv[2], "remote") == 0)
		fprintf(stdout, "SUCCESS\n");

	return (0);
}

int
vc_start(int argc, char **argv)
{
	struct vps_param vp;
	struct vps_conf vc;
	char file_n[MAXPATHLEN];

	memset(&vp, 0, sizeof(struct vps_param));

	switch (argc) {
		case 1:
			/* <name> */
			snprintf(file_n, MAXPATHLEN, "%s/vps_%s.conf",
				_PATH_CONFDIR, argv[0]);
			/* FALLTHROUGH */
		case 2:
			/* <name> <conffile> */
			if (vc_read_config(argc == 2 ? argv[1] : file_n, &vc))
				return (-1);
			/* Override vps name. */
			strncpy(vp.name, argv[0][0] ? argv[0] : vc.name,
				MAXHOSTNAMELEN);
			break;
		default:
			return (vc_usage(stderr));
			break;
	}

	if (vc.epair) {
		struct epair_cf *epp;

		epp = vc.epair;
		do {
			/*
			printf("epp->idx = %d\nepp->ifidx = %d\nepp->ifconfig = [%s]\n",
					epp->idx, epp->ifidx, epp->ifconfig);
			*/
		} while ((epp = epp->next));
	}

	/* Fill in vps_param structure. */
	strncpy(vp.fsroot, vc.fsroot, MAXPATHLEN);

	/* If given, execute the command to mount the root filesystem. */
	if (vc.cmd_mountroot[0]) {
		if (vc_exec_cmd(vc.cmd_mountroot, 0))
			goto startfail;
	}

	/* 
	 * Actually create the vps instance.
	 */
	if (ioctl(vpsfd, VPS_IOC_CREAT, &vp) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_CREAT: %s\n",
			strerror(errno));
		goto startfail;
	}

	/*
	 * Set allowed privileges and ip4/ip6 networks.
	 */
	{
		struct ip_network *netp;
		char *argv2[4];
		int error;
		int i;

		for (i = 0; i < vc.ip_networks_cnt; i++) {
			netp = vc.ip_networks[i];

			switch (netp->af) {
			case AF_INET:
				vc_allow_ip4net(vp.name, netp->addr.in.s_addr,
				     netp->mask.in.s_addr);
				break;
			case AF_INET6:
				vc_allow_ip6net(vp.name, &netp->addr.in6, &netp->mask.in6);
				break;
			default:
				break;
			}
		}

		argv2[0] = vp.name;
		argv2[3] = NULL;

		if (vc.priv_allow != NULL) {
			argv2[1] = strdup("allow");
			argv2[2] = vc.priv_allow;
			error = vc_arg_priv(3, argv2);
			free(argv2[1]);
			if (error != 0)
				goto startfail;
		}

		if (vc.priv_nosys != NULL) {
			argv2[1] = strdup("nosys");
			argv2[2] = vc.priv_nosys;
			error = vc_arg_priv(3, argv2);
			free(argv2[1]);
			if (error != 0)
				goto startfail;
		}

		if (vc.limits != NULL) {
			argv2[1] = vc.limits;
			argv2[2] = NULL;
			error = vc_arg_limit(2, argv2);
			/*
			if (error != 0)
				goto startfail;
			Ignore error and only warn if module is not loaded.
			*/
			if (error != 0 && error != EOPNOTSUPP)
				goto startfail;
		}

	}

	/*
	 * Clone the epair network interfaces, and move the b-side(s)
	 * into the newly created vps instance.
	 * Also issue the epair_<idx>_ifconfig commands.
	 */
	if (vc.epair) {
		struct vps_arg_ifmove va;
		char cmd[0x100];
		struct epair_cf *epp;

		epp = vc.epair;
		do {
			/* XXX check if interface does not exist before creating ! */

			snprintf(cmd, 0x100, "ifconfig epair%d create\n", epp->ifidx);
			if (vc_exec_cmd(cmd, 0))
				goto startfail;

			snprintf(cmd, 0x100, "ifconfig epair%da %s\n",
					epp->ifidx, epp->ifconfig);
			if (vc_exec_cmd(cmd, 0))
				goto startfail;

			/* Move b-side into vps instance. */
			strncpy(va.vps_name, vp.name, sizeof(va.vps_name));
			snprintf(va.if_name, sizeof(va.if_name), "epair%db", epp->ifidx);
			if ((ioctl(vpsfd, VPS_IOC_IFMOVE, &va)) == -1) {
				fprintf(stderr, "ioctl VPS_IOC_IFMOVE: %s\n",
					strerror(errno));
				goto startfail;
			}

		} while ((epp = epp->next));
	}

	/*
	 * Clone the vps network interfaces, set adresses on interface and
	 * (XXX) set in vps instance for filtering and lookup;
	 * then move interface into vps.
	 */
	if (vc.netif) {
		struct vps_arg_ifmove va;
		struct netif_cf *netifp;
		struct netif_addr *ifaddrp;
		struct ifreq ifr;
		struct in6_addr addr6;
		u_int8_t plen;
		/* char cmd[0x100]; */
		int sockfd;
		int i;

		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			fprintf(stderr, "socket failed: %s\n", strerror(errno));
			goto startfail;
		}

		netifp = vc.netif;
		do {
			/*
			fprintf(stderr, "netifp=%p idx=%d addrcnt=%d\n",
					netifp, netifp->idx, netifp->ifaddr_cnt);
			snprintf(cmd, 0x100, "ifconfig vps%d create", netifp->ifidx);
			if (vc_exec_cmd(cmd, 0))
				goto startfail;
			*/
			memset(&ifr, 0, sizeof(ifr));
			strncpy(ifr.ifr_name, "vps", 4);
			if (ioctl(sockfd, SIOCIFCREATE2, &ifr) == -1) {
				fprintf(stderr, "ioctl SIOCIFCREATE2: %s\n", strerror(errno));
				goto startfail;
			}
			strncpy(va.if_name, ifr.ifr_name, sizeof(va.if_name));

			for (i = 0; i < netifp->ifaddr_cnt; i++) {
				ifaddrp = netifp->ifaddr[i];

				/*
				if (ifaddrp->af == AF_INET)
					snprintf(cmd, 0x100,
					    "ifconfig vps%d inet %s netmask 0xffffffff",
					    netifp->ifidx, ifaddrp->str);
				else if (ifaddrp->af == AF_INET6)
					snprintf(cmd, 0x100,
					    "ifconfig vps%d inet6 %s prefixlen 128",
					    netifp->ifidx, ifaddrp->str);
				else
					continue;

				if (vc_exec_cmd(cmd, 0))
					goto startfail;
				*/

				switch (ifaddrp->af) {
				case AF_INET:
					vc_allow_ip4net(vp.name, inet_addr(ifaddrp->str), 0xffffffff);
					break;

				case AF_INET6:
					if (inet_pton(AF_INET6, ifaddrp->str, &addr6) == -1) {
						fprintf(stderr, "couldn't parse [%s] as AF_INET6\n", ifaddrp->str);
						goto startfail;
					} else {
						plen = 128;
						vc_allow_ip6net(vp.name, &addr6, &plen);
					}
					break;

				default:
					break;
				}
			}

			strncpy(va.vps_name, vp.name, sizeof(va.vps_name));
			/* snprintf(va.if_name, sizeof(va.if_name), "vps%d", netifp->ifidx); */
			snprintf(va.if_newname, sizeof(va.if_newname), "vps%d", netifp->ifidx);
			if ((ioctl(vpsfd, VPS_IOC_IFMOVE, &va)) == -1) {
				fprintf(stderr, "ioctl VPS_IOC_IFMOVE: %s\n",
					strerror(errno));
				goto startfail;
			}

		} while ((netifp = netifp->next));

		close(sockfd);
	}

	vc_net_publish(&vc);

	/* Start the init process if given. 
	   XXX support parameters. */
	if (vc.initproc[0]) {
		int argc2 = 3;
		char str1[] = "execin";
		char *argv2[] = { str1, NULL, NULL, NULL };

		argv2[1] = vp.name;
		argv2[2] = vc.initproc;

		if (vc_exec(argc2, argv2, NULL)) {
			goto startfail;
		}
	}
	return (0);

  startfail:
	fprintf(stderr, "not started due to failure(s) !\n");

	/* XXX undo everything */

	return (-1);
}

int
vc_stop(int argc, char **argv)
{
	struct vps_conf vc;
	char file_n[MAXPATHLEN];
	char *vps_name;
	int vps_status;

	switch (argc) {
		case 1:
			/* <name> */
			snprintf(file_n, MAXPATHLEN, "%s/vps_%s.conf",
				_PATH_CONFDIR, argv[0]);
			/* FALLTHROUGH */
		case 2:
			/* <name> <conffile> */
			memset(&vc, 0, sizeof (struct vps_conf));
			if (vc_read_config(argc==2 ? argv[1] : file_n, &vc) 
				&& argv[0][0] == 0) {
				fprintf(stderr, "must specify at least <name> or <config>\n");
				return (-1);
			}
			/* Override vps name. */
			vps_name = argv[0][0] ? argv[0] : vc.name;
			break;
		default:
			return (vc_usage(stderr));
			break;
	}

	/* 
	 * If we do not have config values, just return because
	 * we do not know what to unmount etc..
	 */
	if (vps_name[0] == 0)
		return (0);

	/*
	 * First get info about the vps instance.
	 */
	{
		struct vps_getextinfo vgx;
		struct vps_extinfo *xinfo;

		memset(&vgx, 0, sizeof(vgx));
		vgx.datalen = sizeof(*xinfo);
		vgx.data = malloc(vgx.datalen);
		xinfo = (struct vps_extinfo *)vgx.data;

		snprintf(vgx.vps_name, sizeof(vgx.vps_name), "%s", vps_name);

		if ((ioctl(vpsfd, VPS_IOC_GETXINFO, &vgx)) == -1) {
			fprintf(stderr, "ioctl VPS_IOC_GETXINFO: %s\n",
				strerror(errno));
			free(xinfo);
			return (-1);
		}

		vps_status = xinfo->status;

		free(xinfo);
	}

	vc_net_revoke(&vc);

	/*
	 * Issue DESTROY IOCTL and specify 60 seconds grace time
	 * between SIGTERM and SIGKILL.
	 */
	if ((ioctl(vpsfd, VPS_IOC_DESTR, vps_name)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_DESTR: %s\n",
			strerror(errno));
		return (-1);
	}

	/* Take epair interfaces down and destroy them */
	if (vc.epair) {
		char cmd[0x100];
		struct epair_cf *epp;

		epp = vc.epair;
		do {
			snprintf(cmd, 0x100, "ifconfig epair%da down\n", epp->ifidx);
			vc_exec_cmd(cmd, 0);

			snprintf(cmd, 0x100, "ifconfig epair%db down\n", epp->ifidx);
			vc_exec_cmd(cmd, 0);

			/* disabled - epair destroy panics
			snprintf(cmd, 0x100, "ifconfig epair%da destroy\n", epp->ifidx);
			vc_exec_cmd(cmd, 0);
			*/

		} while ((epp = epp->next));
	}

	if (vc.cmd_unmountroot[0])
		vc_exec_cmd(vc.cmd_unmountroot, 0);

	return (0);
}

int
vc_exec(int argc, char **argv, char **envv)
{
	int rc, pid;
	int ptsmfd, ptssfd;
	int f_pts;
	int i;
	u_long cmd;

	if (argc < 3)
		return (vc_usage(stderr));
	
	if (strcmp (argv[0], "execin") == 0)
		cmd = VPS_IOC_SWITCH;
	else
	if (strcmp (argv[0], "execwt") == 0)
		cmd = VPS_IOC_SWITWT;
	else
		return (vc_usage(stderr));

	if (cmd == VPS_IOC_SWITWT) {
		f_pts = 1;
		if ((ptsmfd = posix_openpt(O_RDWR)) == -1) {
			fprintf(stderr, "posix_openpt(): %s\n", strerror(errno));
			return (-1);
		}
		cmd = VPS_IOC_SWITCH;
	} else {
		f_pts = 0;
		ptssfd = ptsmfd = -1;
	}

	if ((pid = fork()) == -1) {
		fprintf(stderr, "fork: %s\n", strerror(errno));
		return (-1);
	}

	if (pid == 0) {

		if (f_pts) {
			/* Get rid of any open file descriptors except vpsfd and ptsmfd. */
			closefrom(MAX(vpsfd, ptsmfd) + 1);
			for (i = 0; i < MAX(vpsfd, ptsmfd); i++)
				if (i != vpsfd && i != ptsmfd)
					close(i);

			if ((ptssfd = open(ptsname(ptsmfd), O_RDWR)) == -1) {
				fprintf(stderr, "open(%s): %s\n", ptsname(ptsmfd),
					strerror(errno));
				close(ptsmfd);
				exit(-1);
			}
			close(ptsmfd);
			dup2(ptssfd, 0);
			dup2(ptssfd, 1);
			dup2(ptssfd, 2);

		} else {
			/* Get rid of any open file descriptors except vpsfd. */
			closefrom(vpsfd + 1);
			for (i = 0; i < vpsfd; i++)
				close(i);

		}

		if ((rc = ioctl(vpsfd, cmd, argv[1])) == -1) {
			fprintf(stderr, "ioctl %lx: %s\n",
				cmd, strerror(errno));
			close(vpsfd);
			exit(-1);
		}
		if (close(vpsfd) == -1) {
			fprintf(stderr, "%s: close: %s\n",
				__func__, strerror(errno));
		}

		if (f_pts) {
			/*
			if (setpgid(0, 0) == -1) {
				fprintf(stderr, "setpgid(): %s\n", strerror(errno));
				exit(-1);
			}
			*/
			if (setsid() == -1) {
				fprintf(stderr, "setsid(): %s\n", strerror(errno));
				/*
				 * If this is the only process in the vps instance
				 * it's process group has the same number the process,
				 * which implies setsid() is not permitted.
				 * Report error on screen but continue anyway
				 * to allow opening an emergency shell or whatever.
				 *
				exit(-1);
				*/
			}
			if (ioctl(ptssfd, TIOCSCTTY, NULL) == -1) {
				fprintf(stderr, "ioctl(TIOCSCTTY): %s\n", strerror(errno));
				exit(-1);
			}
		}

		if (execve(argv[2], &argv[2], envv) == -1) {
			fprintf(stderr, "execve([%s], ...): %s\n",
				argv[2], strerror(errno));
			exit(-1);
		}
		/* NOTREACHED */

	} else {
		if (f_pts) {
			fprintf(stdout, def_escape_message);
			vc_ttyloop(ptsmfd, def_escape_pattern);
		}
		rc = 0;
	}

	return (rc);
}

int
vc_ifmove(int argc, char **argv)
{
	struct vps_arg_ifmove va;

	if (argc < 2)
		return (vc_usage(stderr));

	strncpy(va.vps_name, argv[0], sizeof(va.vps_name));
	strncpy(va.if_name, argv[1], sizeof(va.if_name));
	if (argc > 2)
		strncpy(va.if_newname, argv[2], sizeof(va.if_newname));

	if ((ioctl(vpsfd, VPS_IOC_IFMOVE, &va)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_IFMOVE: %s\n",
			strerror(errno));
		return (-1);
	}

	DBGfprintf(stdout, "%s@%s\n", va.if_name, va.vps_name);
	return (0);
}

static void
vc_migrate_sighandler(int sig)
{
	char str_resume[] = "resume";
	char *str[3];
	int error = 0;

	fprintf(stderr, "%s: signal=%d received\n", __func__, sig);

	if (sig == SIGSEGV || sig == SIGBUS || sig == SIGABRT) {
		signal(sig, SIG_DFL);
		return;
	}

	fprintf(stderr, "Aborted. \n");
	if (mig_did_suspend) {
		fprintf(stderr, "Trying to resume vps ... ");
		str[0] = str_resume;
		str[1] = mig_vps;
		str[2] = NULL;
		if ((error = vc_suspend(2, str))) {
			fprintf(stderr, "failed (%s)\n", strerror(errno));
		} else {
			fprintf(stderr, "done\n");
		}
		
	}
	if (error == 0 && mig_did_revoke) {
		fprintf(stderr, "Trying to re-announce vps on networks ... ");
		if ((error = vc_net_publish(&mig_vc))) {
			fprintf(stderr, "failed (%s)\n", strerror(errno));
		} else {
			fprintf(stderr, "done\n");
		}
	}

	signal(sig, SIG_DFL);
	kill(getpid(), sig);
	return;
}

int
vc_migrate(int argc, char **argv)
{
	struct vps_conf vc;
	struct statfs stf;
	struct stat st;
	char *argv2[5];
	char *host, *vps;
	char *fsroot;
	char cmd[0x100];
	char file_n[MAXPATHLEN];
	char cnt_rsync;
	char str_suspend[] = "suspend";
	char str_abort[] = "abort";
	int pid, rfd, wfd;
	int argc2;
	int error;
	int len;
	
	if (argc < 2)
		return (vc_usage(stderr));

	mig_did_suspend = 0;
	mig_did_revoke = 0;
	signal(SIGINT, vc_migrate_sighandler);
	signal(SIGQUIT, vc_migrate_sighandler);
	signal(SIGTERM, vc_migrate_sighandler);
	signal(SIGPIPE, vc_migrate_sighandler);

	snprintf(file_n, MAXPATHLEN, "%s/vps_%s.conf",
			_PATH_CONFDIR, argv[0]);
	memset(&vc, 0, sizeof(struct vps_conf));
	if (vc_read_config(file_n, &vc)) {
		fprintf(stderr, "config not found\n");
		return (1);
	}

	vps = argv[0];
	host = argv[1];

	mig_vps = vps;
	mig_vc = vc;

	if (vc.fsroot_priv[0] != '\0')
		fsroot = vc.fsroot_priv;
	else
		fsroot = vc.fsroot;

	if ((error = statfs(fsroot, &stf)) != 0) {
		fprintf(stderr, "statfs([%s]): error: %s\n",
		    fsroot, strerror(errno));
		return (1);
	}

	if (stf.f_flags & MNT_LOCAL)
		cnt_rsync = 2;
	else
		cnt_rsync = 0;

	if (argc > 2 && strcmp(argv[2], "norsync") == 0)
		cnt_rsync = 0;
	else if (argc > 2 && strcmp(argv[2], "onersync") == 0)
		cnt_rsync = 1;
	else if (argc > 2 && strcmp(argv[2], "tworsync") == 0)
		cnt_rsync = 2;

	fprintf(stderr, "Opening ssh transport ... ");

	if ((error = vc_get_ssh_transport(host, &pid, &rfd, &wfd)))
		return (error);

	fprintf(stderr, "done\n");

	/* Always syncing config file. */
#if 0
/* Don't use rsync here ... */
	fprintf(stderr, "Copying config file ... ");
	snprintf(cmd, sizeof(cmd), "vpsctl rsyncserver %s/\n", _PATH_CONFDIR); 
	write(wfd, cmd, strlen(cmd));
	if ((error = vc_rsync(RSYNC_MODE_CLIENT, rfd, wfd, file_n)))
		goto resume;
	
	fprintf(stderr, "done\n");
#else
	if ((error = stat(file_n, &st)) != 0) {
		fprintf(stderr, "stat([%s]): error: %s\n",
		    file_n, strerror(errno));
		return (1);
	}
	snprintf(cmd, sizeof(cmd), "vpsctl savefile %s %ld %d\n",
	    file_n, st.st_size, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	write(wfd, cmd, strlen(cmd));
	/* vc_savefile() sends '\n' when ready */
	len = read(rfd, cmd, 1); 
	{
		char file_buf[0x1000];
		int file_fd;

		/*
		 * Assume config file is no larger than buf and
		 * can be written in one go.
		 * XXX fix that
		 */
		
		if ((file_fd = open(file_n, O_RDONLY)) == -1) {
			fprintf(stderr, "stat([%s]): error: %s\n",
			    file_n, strerror(errno));
			return (1);
		}
		read(file_fd, file_buf, sizeof(file_buf));
		close(file_fd);

		write(wfd, file_buf, st.st_size);	
	}
#endif

	/* Always create directories. */
	if (vc.fsroot_priv[0] != '\0') {
		/* Create vps' private root directory. */
		snprintf(cmd, sizeof(cmd), "mkdir -p %s\n", vc.fsroot_priv);
		write(wfd, cmd, strlen(cmd));
		//len = read(rfd, cmd, sizeof(cmd));
	}
	/* Create vps' mountpoint directory. */
	snprintf(cmd, sizeof(cmd), "mkdir -p %s\n", vc.fsroot);
	write(wfd, cmd, strlen(cmd));
	//len = read(rfd, cmd, sizeof(cmd));

	if (cnt_rsync == 2) {

		/* Start a first filesystem sync while vps is still running. */

		fprintf(stderr, "Performing first filesystem sync ... ");

		snprintf(cmd, sizeof(cmd), "vpsctl rsyncserver %s/\n", fsroot);
		write(wfd, cmd, strlen(cmd));
		snprintf(cmd, sizeof(cmd), "%s/", fsroot);
		if ((error = vc_rsync(RSYNC_MODE_CLIENT, rfd, wfd, cmd)))
			goto resume;

		fprintf(stderr, "done\n");

	}

	fprintf(stderr, "Suspending vps ... ");

	argv2[0] = str_suspend;
	argv2[1] = vps;
	argv2[2] = NULL;
	argc2 = 2;
	if ((error = vc_suspend(argc2, argv2))) {
		return (error);
	}
	mig_did_suspend = 1;

	fprintf(stderr, "done\n");

	if (cnt_rsync > 0) {

		fprintf(stderr, "Performing final filesystem sync ... ");

		/* After suspending do the final filesystem sync. */
		snprintf(cmd, sizeof(cmd), "vpsctl rsyncserver %s/\n", fsroot);
		write(wfd, cmd, strlen(cmd));
		snprintf(cmd, sizeof(cmd), "%s/", fsroot);
		if ((error = vc_rsync(RSYNC_MODE_CLIENT, rfd, wfd, cmd)))
			goto resume;

		fprintf(stderr, "done\n");

	}

	/* Take offline. */
	vc_net_revoke(&vc);
	mig_did_revoke = 1;

	/* Start restore on other side ... */
	snprintf(cmd, sizeof(cmd), "vpsctl restore %s - remote\n", vps);
	write(wfd, cmd, strlen(cmd));

	/* ... and send snapshot data. */
	argv2[0] = vps;
	//argv2[1] = (char *)filename;
	argv2[1] = NULL;
	argv2[2] = NULL;
	argc2 = 2;
	if ((error = vc_snapshot(argc2, argv2, wfd, 1)))
		goto resume;

	fprintf(stderr, "Restoring on remote host ... ");

	/* Get status of remote restore. */
	len = read(rfd, cmd, sizeof(cmd));
	cmd[len] = '\0';
	DBGfprintf(stdout, "%s: remote restore: [%s]\n", __func__, cmd);
	if (strncmp(cmd, "SUCCESS", 7)) {
		fprintf(stderr, "%s: restore on remote host failed\n", __func__);
		goto resume;
	}

	fprintf(stderr, "done\n");

	/* remote vps_resume */
	snprintf(cmd, sizeof(cmd), "vpsctl resume %s remote\n", vps);
	write(wfd, cmd, strlen(cmd));
	len = read(rfd, cmd, sizeof(cmd));
	cmd[len] = '\0';
	DBGfprintf(stderr, "%s: remote resume: [%s]\n", __func__, cmd);
	if (strncmp(cmd, "SUCCESS", 7) != 0) {
		fprintf(stderr, "%s: resume on remote host failed: [%s]\n",
			__func__, cmd);
		goto resume;
	}

	fprintf(stderr, "Aborting local instance ... ");

	/* Migration was successful so abort local instance. */
	argv2[0] = str_abort;
	argv2[1] = vps;
	argv2[2] = NULL;
	argc2 = 2;
	vc_abort(argc2, argv2);

	fprintf(stderr, "done\n");

	fprintf(stderr, "Stopping local instance ... ");

	argv2[0] = vps;
	argv2[1] = NULL;
	argc2 = 1;
	vc_stop(argc2, argv2);

	fprintf(stderr, "done\n");

	vc_close_ssh_transport(pid);	

	return (0);

  resume:
	/*
	 * XXX does not work like this currently because snapshot context in
	 *     kernel exists until mappings are removed and (?) dev fd closed.
	 */
	/*
	argv2[0] = str_resume;
	argv2[1] = vps;
	argv2[2] = NULL;
	argc2 = 2;
	vc_suspend(argc2, argv2);

	//vc_net_publish(&vc);
	*/

	kill(getpid(), SIGTERM);

	return (error);
}

/* open ssh session to remote host and return a set of
   file descriptors */
int
vc_get_ssh_transport(char *host, int *pid, int *rfd, int *wfd)
{
	char str_ssh[] = "/usr/bin/ssh";
	char str_q[] = "-q";
	char str_l[] = "-l";
	char str_root[] = "root";
	char *argv[] = { str_ssh, str_q, str_l, str_root, NULL, NULL };
	int sshrfd, sshwfd;
	int fds[2];
	int error;

	argv[4] = host;

	if ((error = pipe(fds))) {
		fprintf(stderr, "%s: pipe: %s\n",
			__func__, strerror(errno));
	}
	*rfd = fds[0]; 
	sshwfd = fds[1];

	if ((error = pipe(fds))) {
		fprintf(stderr, "%s: pipe: %s\n",
			__func__, strerror(errno));
	}
	*wfd = fds[1]; 
	sshrfd = fds[0];

	if ((*pid = fork()) == -1) {
		fprintf(stderr, "%s: fork: %s\n",
			__func__, strerror(errno));
		return (-1);
	}

	if (*pid == 0) {

		close(1);
		close(0);
		dup2(sshrfd, 0);
		dup2(sshwfd, 1);

		if ((execve(argv[0], argv, vc_envv)) == -1) {
			fprintf(stderr, "%s: execve: %s\n",
				__func__, strerror(errno));
			exit(-1);
		}
		/* NOTREACHED */

	} else {
		char buf[0x100];
		int rlen = 0;

		/* Get rid of warning message. */
		/* XXX dirty */
		while (rlen < 80)
		   rlen += read(*rfd, buf, sizeof(buf));
	}

	return (0);
}

int
vc_close_ssh_transport(int pid)
{
	int status = 0;

	if (pid > 0) {
		kill(pid, SIGTERM);
		wait4(pid, &status, 0, NULL);
	}
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

int
vc_sshtest(int argc, char **argv)
{
	char buf[0x100];
	int pid, rfd, wfd;
	int len;

	if (argc < 2) {
		return (-1);
	}

	if ((vc_get_ssh_transport(argv[0], &pid, &rfd, &wfd)))
		return (-1);

	write(wfd, argv[1], strlen(argv[1]));
	write(wfd, "\n", 1);
	len = read(rfd, buf, sizeof(buf));
	buf[len] = 0;
	printf("buf=[%s]\n", buf);

	vc_close_ssh_transport(pid);

	return (0);
}

#define RSYNC_DELETE 1

/*
 * The rsync program that is used here is slightly adapted to allow
 * write/read from/to its stdin/stdout.
 */
/*
 * XXX Somehow rsync doesn't work properly with --numeric-ids specified.
 */
int
vc_rsync(int mode, int rfd, int wfd, char *path)
{
	char *argv[8];
	char str_rsync[] = "/usr/sbin/rsync_vps";
	char str_flagscl[] = "-xaHAXe";
	char str_dash[] = "-";
	char str_server[] = "--server";
	char str_flagssv[] = "-logDtprxHAXe.iLf";
	char str_delete[] = "--delete";
	//char str_numericids[] = "--numeric-ids";
	char str_dot[] = ".";
	int oflags_rfd;
	int oflags_wfd;
	int status;
	int pid;

	/* 
	 * XXX Make sure directory actually exists.
	 * On server side create it if necessary.
	 *
	 * --> happens in vc_migrate().
	 */

	oflags_rfd = fcntl(rfd, F_GETFL);
	oflags_wfd = fcntl(wfd, F_GETFL);

	pid = fork();

	if (pid == -1) {
		fprintf(stderr, "fork failed: %s\n", strerror(errno));
		return (-1);

	} else if (pid == 0) {
		
		if (rfd != 0 && wfd != 1) {
			close(1);
			close(0);
			dup2(rfd, 0);
			dup2(wfd, 1);
		}

		if (mode == RSYNC_MODE_CLIENT) {

			argv[0] = str_rsync;
			argv[1] = str_delete;
			//argv[2] = str_numericids;
			argv[2] = str_flagscl;
			argv[3] = str_dash;
			argv[4] = path;
			argv[5] = NULL;

		} else if (mode == RSYNC_MODE_SERVER) {

			argv[0] = str_rsync;
			argv[1] = str_server;
			argv[2] = str_flagssv;
			argv[3] = str_delete;
			//argv[4] = str_numericids;
			argv[4] = str_dot;
			argv[5] = path;
			argv[6] = NULL;

		} else {
			fprintf(stderr, "invalid rsync mode: %d\n", mode);
			exit(-1);
		}

		DBGfprintf(stderr, "%s: cmdline=[%s] [%s] [%s] [%s] [%s] [%s] [%s]\n",
			__func__, argv[0], argv[1], argv[2], argv[3], argv[4],
			argv[5], argv[6]);

		if (mode == RSYNC_MODE_CLIENT) {
			/* XXX protocol setup blocks if we don't wait ... */
			sleep(1);
		}

		if ((execve(argv[0], argv, NULL))) {
			fprintf(stderr, "execve(%s, ...) failed: %s\n",
				argv[0], strerror(errno));
			exit(-1);
		}
		/* Not reached. */
		
	} else {
		const char *str;
		char buf[0x100];
		char found;
		char *p;
		int slen;
		int rc;

		str = "RSYNC FINISHED\n";
		slen = strlen(str);
		found = 0;

		wait4(pid, &status, 0, NULL);

		/*
		 * There might be data left to read of the server side rsync,
		 * like debug messages, so read everything until we have the
		 * shell again.
		 *
		 * On the server side we send the message held in ''str''
		 * to inform the client side that rsync is done.
		 *
		 * I know this isn't what can be considered as good style.
		 */
		if (mode == RSYNC_MODE_CLIENT) {
			memset(buf, 0, sizeof(buf));
			while (found == 0) {
				/* I/O is still blocking here. */
				rc = read(rfd, buf, sizeof(buf));
				if (rc == -1 && errno != EAGAIN && errno != EINTR) {
					fprintf(stderr, "%s: read error: %s\n",
						__func__, strerror(errno));
					return (-1);
				}

				for (p = buf; p < buf + sizeof(buf) - slen; p++) {
					if (memcmp(p, str, slen) == 0) {
						found = 1;
						break;
					}
				}
				usleep(10);
			}

		} else {
			snprintf(buf, sizeof(buf), "%s", str);
			write(wfd, buf, slen);
		}

		/* 
		 * Rsync sets i/o channels to non blocking;
		 * so revert to blocking mode.
		 */
		fcntl(rfd, F_SETFL, oflags_rfd);
		fcntl(wfd, F_SETFL, oflags_wfd);

		return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
	}

	/* Not reached. */
	return (0);
}

/*
 * If using vps interface, set routes and arp entries
 * for each of clients addresses.
 * XXX Do custom net_publish command if specified.
 */
int
vc_net_publish(struct vps_conf *cf)
{
	struct netif_addr *ifaddrp;
	struct netif_cf *netifp;
	char cmd[0x100];
	int i;

	if (cf->netif == NULL)
		return (0);

	netifp = cf->netif;
	do {
		for (i = 0; i < netifp->ifaddr_cnt; i++) {
			ifaddrp = netifp->ifaddr[i];

			if (ifaddrp->af == AF_INET) {
				snprintf(cmd, 0x100, "arp -s %s auto pub",
						ifaddrp->str);
				vc_exec_cmd(cmd, 1);
				snprintf(cmd, 0x100, "route add -inet %s -iface %s",
						ifaddrp->str, DEFAULT_VPSIF);
				vc_exec_cmd(cmd, 1);
			} else if (ifaddrp->af == AF_INET6) {
				snprintf(cmd, 0x100, "route add -inet6 %s -iface %s -proto1",
						ifaddrp->str, DEFAULT_VPSIF);
				vc_exec_cmd(cmd, 1);
			}
		}
	} while ((netifp = netifp->next));

	return (0);
}

int
vc_net_revoke(struct vps_conf *cf)
{
	struct netif_addr *ifaddrp;
	struct netif_cf *netifp;
	char cmd[0x100];
	int i;

	if (cf->netif == NULL)
		return (0);

	netifp = cf->netif;
	do {
		for (i = 0; i < netifp->ifaddr_cnt; i++) {
			ifaddrp = netifp->ifaddr[i];

			if (ifaddrp->af == AF_INET) {
				snprintf(cmd, 0x100, "route del -inet %s -iface %s",
						ifaddrp->str, DEFAULT_VPSIF);
				vc_exec_cmd(cmd, 1);
				snprintf(cmd, 0x100, "arp -d %s pub",
						ifaddrp->str);
				vc_exec_cmd(cmd, 1);
			} else if (ifaddrp->af == AF_INET6) {
				snprintf(cmd, 0x100, "route del -inet6 %s -iface %s -proto1",
						ifaddrp->str, DEFAULT_VPSIF);
				vc_exec_cmd(cmd, 1);
			}
		}
	} while ((netifp = netifp->next));

	return (0);
}

int
vc_exec_cmd(char *cmd, int quiet)
{
	char str_sh[] = "/bin/sh";
	char str_opt[] = "-c";
	char *argv[] = { str_sh, str_opt, NULL, NULL };
	int status = 0;
	int pid;

	if ( !quiet)
		DBGfprintf(stdout, "%s: cmd: [%s]\n", __func__, cmd);

	argv[2] = cmd;

	if ((pid = fork()) == -1) {
		fprintf(stderr, "%s: fork: %s\n",
			__func__, strerror(errno));
		return (-1);
	}

	if (pid == 0) {
		if (quiet) {
			int fd;

			close(1);
			close(2);
			fd = open("/dev/null", O_RDWR);
			if (fd != 1)
			 	dup2(fd, 1);
			if (fd != 2)
				dup2(fd, 2);
		}
		if ((execve("/bin/sh", argv, vc_envv)) == -1) {
			fprintf(stderr, "%s: execve: %s\n",
				__func__, strerror(errno));
			exit(-1);
		}
		/* NOTREACHED */
	} else {
		wait4(pid, &status, 0, NULL);
	}
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

int
vc_read_config(char *file_n, struct vps_conf *vc)
{
	char linebuf[LINE_MAXLEN];
	FILE *file_s;

	memset(vc, 0, sizeof(struct vps_conf));
	/* Set defaults where available. */
	//strncpy(vc->initproc, "/sbin/init", MAXPATHLEN);

	if ((file_s = fopen(file_n, "r")) == NULL) {
		fprintf(stderr, "open file [%s]: %s\n",
			file_n, strerror(errno));
		return (-1);
	}
	while (fgets(linebuf, LINE_MAXLEN, file_s)) {
		char *key, *val, *pos;

		/* [whitespace]KEY[whitespace]=[whitespace]VALUE[whitespace]\n */
		pos = linebuf;
		key = strsep(&pos, "=");
		val = strsep(&pos, "\n");
		if ( ! (key && val && strlen(key) > 0 && strlen(val) > 0) )
			continue;
		key = vc_trim_whitespace(key);
		val = vc_trim_whitespace(val);

		if (key[0] == '#')
			continue;

		/* Fill in structure. */
		if      (strcasecmp (key, "name") == 0)
			strncpy(vc->name, val, MAXHOSTNAMELEN);

		else if (strcasecmp(key, "fsroot") == 0)
			strncpy(vc->fsroot, val, MAXPATHLEN);

		else if (strcasecmp(key, "fsroot_priv") == 0)
			strncpy(vc->fsroot_priv, val, MAXPATHLEN);

		else if (strcasecmp(key, "init") == 0)
			strncpy(vc->initproc, val, MAXPATHLEN);

		else if (strcasecmp(key, "root_mount") == 0)
			strncpy(vc->cmd_mountroot, val, MAXPATHLEN);

		else if (strcasecmp(key, "root_unmount") == 0)
			strncpy(vc->cmd_unmountroot, val, MAXPATHLEN);

		else if (strcasecmp(key, "devfs_ruleset") == 0)
			strncpy(vc->devfs_ruleset, val, DEFAULTLEN);

		else if (strcasecmp(key, "network_announce") == 0)
			strncpy(vc->network_announce, val, DEFAULTLEN);

		else if (strcasecmp(key, "network_revoke") == 0)
			strncpy(vc->network_revoke, val, DEFAULTLEN);

		else if (strncasecmp(key, "netif_", 6) == 0) {
			struct netif_cf *netifp, *netifp2;
			char cmd[11];
			char *c;
			int idx;

			c = key;
			while (*c) {
				*c = tolower(*c);
				++c;
			}

			netifp = NULL;
			if (sscanf(key, "netif_%d_%10s", &idx, cmd) == 2) {
			
				if ((netifp2 = vc->netif)) {
					do {
						if (netifp2->idx == idx)
							netifp = netifp2;
					} while ((netifp2 = netifp2->next));
				}
			} else
				continue;

			if (netifp == NULL) {
				if ((netifp = malloc(sizeof(struct netif_cf))) == NULL) {
					fprintf(stderr, "no memory !\n");
					return (-1);
				}
				memset(netifp, 0, sizeof(*netifp));
				netifp2 = vc->netif;
				if (netifp2) {
					while (netifp2->next)
						netifp2 = netifp2->next;
					netifp2->next = netifp;
				} else
					vc->netif = netifp;	
				netifp->idx = idx;
				netifp->ifidx = idx;

			}

			if (strcmp(cmd, "ifidx") == 0)
				netifp->ifidx = atoi(val);

			else if (strcmp(cmd, "address") == 0) {
				char **ap, *argv[10], *addrstr, *input;
				struct netif_addr *addrp;

				addrstr = strdup(val);
				input = addrstr;
				for (ap = argv; (*ap = strsep(&input, " ,")) != NULL;) {

					if (**ap == '\0')
						continue;

					//fprintf(stderr, "address=[%s]\n", *ap);

					addrp = calloc(1, sizeof(*addrp));

					addrp->str = strdup(*ap);
					netifp->ifaddr[netifp->ifaddr_cnt] = addrp;
					netifp->ifaddr_cnt++;

					if ((inet_pton(AF_INET, *ap, &addrp->addr.in)) == 1) {
						/*
						fprintf(stderr, "in.s_addr=0x%08x\n",
						    addrp->addr.in.s_addr);
						*/
						addrp->af = AF_INET;

					} else if ((inet_pton(AF_INET6, *ap, &addrp->addr.in6))
					    == 1) {
						/*
						fprintf(stderr, "in6.s6_addr: "
						    "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
						    "%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
						    in6.s6_addr[0], in6.s6_addr[1], in6.s6_addr[2],
						    in6.s6_addr[3], in6.s6_addr[4], in6.s6_addr[5],
						    in6.s6_addr[6], in6.s6_addr[7], in6.s6_addr[8],
						    in6.s6_addr[9], in6.s6_addr[10],
						    in6.s6_addr[11], in6.s6_addr[12],
						    in6.s6_addr[13], in6.s6_addr[14],
						    in6.s6_addr[15]);
						*/
						addrp->af = AF_INET6;

					} else {
						fprintf(stderr, "could not parse address [%s]\n",
						    *ap);
						netifp->ifaddr[netifp->ifaddr_cnt] = NULL;
						netifp->ifaddr_cnt--;
						free(addrp);
					}

					if (++ap >= &argv[10])
						break;
				}
				
				free(addrstr);
			}

		}

		else if (strncasecmp(key, "epair_", 6) == 0) {
			struct epair_cf *epp, *epp2;
			char cmd[11];
			char *c;
			int idx;

			c = key;
			while (*c) {
				*c = tolower(*c);
				++c;
			}

			epp = NULL;
			if (sscanf(key, "epair_%d_%10s", &idx, cmd) == 2) {
				if ((epp2 = vc->epair)) {
					do {
						if (epp2->idx == idx)
							epp = epp2;
					} while ((epp2 = epp2->next));
				}
			} else
				continue;

			if (epp == NULL) {
				if ((epp = malloc(sizeof(struct epair_cf))) == NULL) {
					fprintf(stderr, "no memory !\n");
					return (-1);
				}
				memset(epp, 0, sizeof(struct epair_cf));
				epp2 = vc->epair;
				if (epp2) {
					while (epp2->next)
						epp2 = epp2->next;
					epp2->next = epp;
				} else
					vc->epair = epp;
				epp->idx = idx;
			}
			if (strcmp(cmd, "ifidx") == 0)
				epp->ifidx = atoi(val);
			else if (strcmp(cmd, "ifconfig") == 0)
				epp->ifconfig = strdup(val);
		}

		else if (strncasecmp(key, "ip_networks", 13) == 0) {
			/*
			 * XXX redundant, share with vc_arg_ipnet()
			 */
			char **ap, *argv[10], *addrstr, *input, *straddr, *strmask, *strtmp, *sep;
			struct ip_network *netp;

			addrstr = strdup(val);
			input = addrstr;
			for (ap = argv; (*ap = strsep(&input, " ,")) != NULL;) {

				if (**ap == '\0')
					continue;

				strtmp = strdup(*ap);
				sep = strstr(strtmp, "/");
				if (sep == NULL) {
					free(strtmp);
					break;
				}
				straddr = strtmp;
				*sep = '\0';
				strmask = sep+1;

				netp = calloc(1, sizeof(*netp));

				netp->str = strdup(*ap);
				vc->ip_networks[vc->ip_networks_cnt++] = netp;

				if ( (inet_pton(AF_INET, straddr, &netp->addr.in) == 1) &&
				     (inet_pton(AF_INET, strmask, &netp->mask.in) == 1) ) {
					/*
					fprintf(stderr, "in.s_addr=0x%08x\n",
					    netp->addr.in.s_addr);
					*/
					netp->af = AF_INET;

				} else if ( (inet_pton(AF_INET6, straddr, &netp->addr.in6) == 1) &&
		 		    (sscanf(strmask, "%hhu", &netp->mask.in6) == 1) ) {
					/*
					fprintf(stderr, "in6.s6_addr: %02x%02x:%02x%02x:%02x%02x:"
						"%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
						in6.s6_addr[0], in6.s6_addr[1], in6.s6_addr[2],
						in6.s6_addr[3], in6.s6_addr[4], in6.s6_addr[5],
						in6.s6_addr[6], in6.s6_addr[7], in6.s6_addr[8],
						in6.s6_addr[9], in6.s6_addr[10], in6.s6_addr[11],
						in6.s6_addr[12], in6.s6_addr[13], in6.s6_addr[14],
						in6.s6_addr[15]);
					*/
					netp->af = AF_INET6;

				} else {
					fprintf(stderr, "could not parse address [%s]\n", *ap);
					vc->ip_networks[--vc->ip_networks_cnt] = NULL;
					free(netp);
				}

				free(strtmp);

				if (++ap >= &argv[10])
					break;
			}
				
			free(addrstr);
		}

		else if (strncasecmp(key, "priv_allow", 11) == 0) {
			if (vc->priv_allow != NULL) {
				fprintf(stderr, "redundant PRIV_ALLOW line, ignoring.\n");
				break;
			}
			vc->priv_allow = strdup(val);
		}

		else if (strncasecmp(key, "priv_nosys", 11) == 0) {
			if (vc->priv_nosys != NULL) {
				fprintf(stderr, "redundant PRIV_NOSYS line, ignoring.\n");
				break;
			}
			vc->priv_nosys = strdup(val);
		}

		else if (strncasecmp(key, "limits", 7) == 0) {
			if (vc->limits != NULL) {
				fprintf(stderr, "redundant LIMITS line, ignoring.\n");
				break;
			}
			vc->limits = strdup(val);
		}
		else {
			fprintf(stderr, "unknown key [%s], ignoring.\n", key);
		}

		/* fprintf(stdout, "conf: key=[%s] val=[%s]\n", key, val); */
	}
	fclose(file_s);

	/* Check if config is complete. */
	if (vc->name[0] == '\0') {
		fprintf(stderr, "missing parameter NAME in config\n");
		return (-1);
	}
	if (vc->fsroot[0] == '\0') {
		fprintf(stderr, "missing parameter FSROOT in config\n");
		return (-1);
	}

	return (0);
}

char *
vc_trim_whitespace(char *s)
{
	char *r, *p;

	p = s - 1;
	r = s;
	if (s == NULL)
		return (NULL);
	while (*++p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		r = p + 1;
	if (*r == '\'' && *r++)
		while (*++p && *p != '\'' && *p != '\n' && *p != '\r');
	if (*r == '"' && *r++)
		while (*++p && *p != '"' && *p != '\n' && *p != '\r');
	if (*p == '\'' || *p == '"')
		*p = 0;
	while (*++p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r');
	*p = 0;
	return (r);
}

int
vc_allow_ip4net(const char *vpsname, in_addr_t addr, in_addr_t mask)
{
	struct vps_arg_set vas;
	struct vps_arg_item item;

	memset(&vas, 0, sizeof(vas));
	memset(&item, 0, sizeof(item));
	snprintf(vas.vps_name, sizeof(vas.vps_name), "%s", vpsname);
	vas.data = (void*)&item;
	vas.datalen = sizeof(item);
	item.type = VPS_ARG_ITEM_IP4;
	item.u.ip4.addr.s_addr = addr;
	item.u.ip4.mask.s_addr = mask;

	DBGfprintf(stdout, "%s: addr=%08x mask=%08x\n", __func__, addr, mask);

	if ((ioctl(vpsfd, VPS_IOC_ARGSET, &vas)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_ARGSET: %s\n", strerror(errno));
		return (-1);
	}

	return (0);
}

int
vc_allow_ip6net(const char *vpsname, struct in6_addr *addr, u_int8_t *plen)
{
	struct vps_arg_set vas;
	struct vps_arg_item item;

	memset(&vas, 0, sizeof(vas));
	memset(&item, 0, sizeof(item));
	snprintf(vas.vps_name, sizeof(vas.vps_name), "%s", vpsname);
	vas.data = (void*)&item;
	vas.datalen = sizeof (item);
	item.type = VPS_ARG_ITEM_IP6;
	memcpy(&item.u.ip6.addr, addr, sizeof(*addr));
	memcpy(&item.u.ip6.plen, plen, sizeof(*plen));

	if ((ioctl(vpsfd, VPS_IOC_ARGSET, &vas)) == -1) {
		fprintf (stderr, "ioctl VPS_IOC_ARGSET: %s\n", strerror(errno));
		return (-1);
	}

	return (0);
}

int
vc_show(int argc, char **argv)
{
	struct vps_getextinfo vgx;
	struct vps_extinfo *xinfo;
	caddr_t data;
	int datalen;

	if (argc < 1)
		return (vc_usage(stderr));

	datalen = sizeof(*xinfo);
	data = malloc(datalen);
	memset(&vgx, 0, sizeof(vgx));
	memset(data, 0, datalen);
	snprintf(vgx.vps_name, sizeof(vgx.vps_name), "%s", argv[0]);
	vgx.data = data;
	vgx.datalen = datalen;

	if ((ioctl(vpsfd, VPS_IOC_GETXINFO, &vgx)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_GETXINFO: %s\n",
			strerror(errno));
		free(data);
		return (-1);
	}

	xinfo = (struct vps_extinfo *)vgx.data;

	fprintf(stdout, "Name:          %-20s\n", xinfo->name);
	fprintf(stdout, "Status:        %-20s\n", vc_statusstr(xinfo->status));
	fprintf(stdout, "VFS root:      %-20s\n", xinfo->fsroot);
	fprintf(stdout, "Processes:     %8d\n", xinfo->nprocs);
	fprintf(stdout, "Sockets:       %8d\n", xinfo->nsocks);
	fprintf(stdout, "Interfaces:    %8d\n", xinfo->nifaces);
	fprintf(stdout, "Restore count: %8d\n", xinfo->restore_count);

	free(data);

	return (0);
}

int
vc_arg_show(int argc, char **argv)
{
	struct vps_arg_get vag;
	struct vps_arg_item *item;
	caddr_t data;
	char *start_allow, *pos_allow;
	char *start_nosys, *pos_nosys;
	const char *resstr;
	char event;
	int datalen;

	if (argc < 1)
		return (vc_usage(stderr));

	datalen = 0x100000;
	data = malloc(datalen);
	memset(&vag, 0, sizeof(vag));
	memset(data, 0, datalen);
	snprintf(vag.vps_name, sizeof(vag.vps_name), "%s", argv[0]);
	vag.data = data;
	vag.datalen = datalen;

	if ((ioctl(vpsfd, VPS_IOC_ARGGET, &vag)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_ARGGET: %s\n",
			strerror(errno));
		free(data);
		return (-1);
	}

	for (item = (struct vps_arg_item *)vag.data;
		(caddr_t)item < ((caddr_t)vag.data) + vag.datalen;
		item++) {

		char addr[0x20], mask[0x20];

		if (item->type != VPS_ARG_ITEM_IP4)
			continue;

		inet_ntop(AF_INET, &item->u.ip4.addr, addr, 0x20);
		inet_ntop(AF_INET, &item->u.ip4.mask, mask, 0x20);

		fprintf(stdout, "IPv4 Networks: \n%s/%s\n", addr, mask);
	}
	for (item = (struct vps_arg_item *)vag.data;
		(caddr_t)item < ((caddr_t)vag.data) + vag.datalen;
		item++) {

		char addr[0x80];

		if (item->type != VPS_ARG_ITEM_IP6)
			continue;

		inet_ntop(AF_INET6, &item->u.ip6.addr, addr, 0x80);

		fprintf(stdout, "\nIPv6 Networks: \n%s/%hhu\n", addr, item->u.ip6.plen);
	}

	pos_allow = start_allow = malloc (0x1000);
	pos_nosys = start_nosys = malloc (0x1000);

	for (item = (struct vps_arg_item *)vag.data;
		(caddr_t)item < ((caddr_t)vag.data) + vag.datalen;
		item++) {
		if (item->type != VPS_ARG_ITEM_PRIV)
			continue;

		switch (item->u.priv.value) {
		case VPS_ARG_PRIV_ALLOW:
			pos_allow += snprintf(pos_allow, 0x1000 - (pos_allow - start_allow),
						"%s, ", priv_ntos(item->u.priv.priv));
			break;
		/*
		case VPS_ARG_PRIV_DENY:
			break;
		*/
		case VPS_ARG_PRIV_NOSYS:
			pos_nosys += snprintf(pos_nosys, 0x1000 - (pos_nosys - start_nosys),
						"%s, ", priv_ntos(item->u.priv.priv));
			break;
		default:
			break;
		}
	}

	start_allow[pos_allow - start_allow - 2] = '\0';
	start_nosys[pos_nosys - start_nosys - 2] = '\0';
	fprintf(stdout, "\nALLOW privileges: \n%s\n", start_allow);
	fprintf(stdout, "\nNOSYS privileges: \n%s\n", start_nosys);
	free(start_allow);
	free(start_nosys);

	fprintf(stdout, "\n%-10s %1s %12s %12s %12s %12s %12s\n",
		"Resource", "S", "Current", "Soft Limit", "Hard Limit", "Soft Hits", "Hard Hits"); 

	for (item = (struct vps_arg_item *)vag.data;
		(caddr_t)item < ((caddr_t)vag.data) + vag.datalen;
		item++) {
		if (item->type != VPS_ARG_ITEM_LIMIT)
			continue;

		switch (item->u.limit.resource) {
		case VPS_ACC_VIRT:
			resstr = "virt";
			break;
		case VPS_ACC_PHYS:
			resstr = "phys";
			break;
		case VPS_ACC_KMEM:
			resstr = "kmem";
			break;
		case VPS_ACC_KERNEL:
			resstr = "kernel";
			break;
		case VPS_ACC_BUFFER:
			resstr = "buffer";
			break;
		case VPS_ACC_PCTCPU:
			resstr = "pctcpu";
			break;
		case VPS_ACC_BLOCKIO:
			resstr = "blockio";
			break;
		case VPS_ACC_THREADS:
			resstr = "threads";
			break;
		case VPS_ACC_PROCS:
			resstr = "procs";
			break;
		default:
			resstr = "unknown";
			break;
		}

		if (item->u.limit.cur > item->u.limit.soft)
			event = 'C';
		else
		if (item->u.limit.hits_soft != 0 && item->u.limit.hits_hard == 0)
			event = 'x';
		else
		if (item->u.limit.hits_hard != 0)
			event = 'X';
		else
			event = '_';
		if (item->u.limit.soft == 0 && item->u.limit.hard == 0)
			event = '_';

		fprintf(stdout, "%-10s %c %12zu %12zu %12zu %12u %12u\n",
			resstr, event, item->u.limit.cur, item->u.limit.soft, item->u.limit.hard,
			item->u.limit.hits_soft, item->u.limit.hits_hard);
	}

	free(data);
	return (0);
}

int
vc_arg_ipnet(int argc, char **argv)
{
	struct vps_arg_set vas;
	struct vps_arg_item *item;
	char **ap, *argv2[0x100], *input, *sep;
	char *strtmp, *straddr;
	const char *strmask;
	int cmd;

	if (argc < 3)
		return (vc_usage(stderr));

	if (strcmp(argv[1], "add") == 0)
		cmd = 0;
	else
	if (strcmp(argv[1], "rem") == 0)
		cmd = 1;
	else
		return (vc_usage(stderr));

	vas.datalen = sizeof(*item) * 0x100;
	vas.data = malloc(vas.datalen);
	memset(vas.data, 0, vas.datalen);
	item = (struct vps_arg_item *)vas.data;
	if (vas.data == NULL) {
		fprintf(stderr, "%s: malloc: %s\n", __func__, strerror(errno));
		return (-1);
	}
	input = strdup(argv[2]);
	for (ap = argv2; (*ap = strsep(&input, " ,")) != NULL;)
		if (**ap != '\0') {

			strtmp = straddr = strdup(*ap);
			sep = strstr(strtmp, "/");
			if (sep == NULL) {
				strmask = "255.255.255.255";
			} else {
				*sep = '\0';
				strmask = sep+1;
			}

			if ( (inet_pton(AF_INET, straddr, &item->u.ip4.addr) == 1) &&
				 (inet_pton(AF_INET, strmask, &item->u.ip4.mask) == 1) )
					item->type = VPS_ARG_ITEM_IP4;
			else
			if ( (inet_pton(AF_INET6, straddr, &item->u.ip6.addr) == 1) &&
				 /*(inet_pton(AF_INET6, strmask, &item->u.ip6.mask) == 1) )*/
				 (sscanf(strmask, "%hhu", &item->u.ip6.plen) == 1) )
					item->type = VPS_ARG_ITEM_IP6;
			else {
				fprintf(stderr, "could not parse address [%s]\n", *ap);
				free(strtmp);
				free(input);
				free(vas.data);
				return (-1);
			}
			item->revoke = cmd;

			/*
			fprintf(stderr, "%s: %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x / %u\n",
			    __func__,
			    htons(item->u.ip6.addr.s6_addr16[0]),
			    htons(item->u.ip6.addr.s6_addr16[1]), 
			    htons(item->u.ip6.addr.s6_addr16[2]),
			    htons(item->u.ip6.addr.s6_addr16[3]), 
			    htons(item->u.ip6.addr.s6_addr16[4]),
			    htons(item->u.ip6.addr.s6_addr16[5]), 
			    htons(item->u.ip6.addr.s6_addr16[6]),
			    htons(item->u.ip6.addr.s6_addr16[7]), 
			    item->u.ip6.plen);
			*/

			++item;
			if (++ap >= &argv2[0x100])
				break;
		}
	free(input);

	strncpy(vas.vps_name, argv[0], sizeof(vas.vps_name));
	vas.datalen = ((caddr_t)item) - (caddr_t)vas.data;
	if ((ioctl(vpsfd, VPS_IOC_ARGSET, &vas)) == -1) {
		fprintf (stderr, "ioctl VPS_IOC_ARGSET: %s\n",
			strerror(errno));
		return (-1);
	}

	return (0);
}

int
vc_arg_priv(int argc, char **argv)
{
	struct vps_arg_set vas;
	struct vps_arg_item *item;
	char **ap, *argv2[0x100], *input;
	int cmd;

	if (argc < 3)
		return (vc_usage(stderr));

	if (strcmp(argv[1], "allow") == 0)
		cmd = VPS_ARG_PRIV_ALLOW;
	else
	if (strcmp(argv[1], "deny") == 0)
		cmd = VPS_ARG_PRIV_DENY;
	else
	if (strcmp(argv[1], "nosys") == 0)
		cmd = VPS_ARG_PRIV_NOSYS;
	else
		return (vc_usage(stderr));

	vas.datalen = sizeof(*item) * 0x100;
	vas.data = malloc(vas.datalen);
	memset(vas.data, 0, vas.datalen);
	item = (struct vps_arg_item *)vas.data;
	if (vas.data == NULL) {
		fprintf(stderr, "%s: malloc: %s\n", __func__, strerror(errno));
		return (-1);
	}
	input = strdup(argv[2]);
	for (ap = argv2; (*ap = strsep(&input, " ,")) != NULL;)
		if (**ap != '\0') {

			item->type = VPS_ARG_ITEM_PRIV;
			item->u.priv.value = cmd;
			if ((item->u.priv.priv = priv_ston(*ap)) == 0)
				item->u.priv.priv = atoi(*ap);
			if (item->u.priv.priv < _PRIV_LOWEST || item->u.priv.priv > _PRIV_HIGHEST) {
				fprintf(stderr, "%s: invalid privilege %d [%s]\n",
					__func__, item->u.priv.priv, *ap);
				free(vas.data);
				free(input);
				return (-1);
			}

			++item;
			if (++ap >= &argv2[0x100])
				break;
		}
	free(input);

	strncpy(vas.vps_name, argv[0], sizeof(vas.vps_name));
	vas.datalen = ((caddr_t)item) - (caddr_t)vas.data;
	if ((ioctl(vpsfd, VPS_IOC_ARGSET, &vas)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_ARGSET: %s\n",
			strerror(errno));
		return (-1);
	}

	return (0);
}

int
vc_arg_limit(int argc, char **argv)
{
	struct vps_arg_set vas;
	struct vps_arg_item *item;
	char **ap, *argv2[0x100], *input, resstr[0x100];

	/* "vpsctl limit vps192 virt:100:200,phys:50:100" */

	if (argc < 2)
		return (vc_usage(stderr));

	vas.datalen = sizeof(*item) * 0x100;
	vas.data = malloc(vas.datalen);
	memset(vas.data, 0, vas.datalen);
	item = (struct vps_arg_item *)vas.data;
	if (vas.data == NULL) {
		fprintf(stderr, "%s: malloc: %s\n", __func__, strerror(errno));
		return (-1);
	}
	input = strdup(argv[1]);
	for (ap = argv2; (*ap = strsep(&input, " ,")) != NULL;)
		if (**ap != '\0') {

			item->type = VPS_ARG_ITEM_LIMIT;

			if ((strlen(*ap) > 0x100) ||
			    (sscanf(*ap, "%[^:]:%zu:%zu", resstr,
			     &item->u.limit.soft, &item->u.limit.hard) != 3)) {
				fprintf(stderr, "%s: invalid argument [%s]\n",
					__func__, *ap);
				free(vas.data);
				free(input);
				return (-1);
			}

			if      (strcmp(resstr, "virt") == 0)
				item->u.limit.resource = VPS_ACC_VIRT;
			else if (strcmp(resstr, "phys") == 0)
				item->u.limit.resource = VPS_ACC_PHYS;
			/*
			else if (strcmp(resstr, "kmem") == 0)
				item->u.limit.resource = VPS_ACC_KMEM;
			else if (strcmp(resstr, "kernel") == 0)
				item->u.limit.resource = VPS_ACC_KERNEL;
			else if (strcmp(resstr, "buffer") == 0)
				item->u.limit.resource = VPS_ACC_BUFFER;
			*/
			else if (strcmp(resstr, "pctcpu") == 0)
				item->u.limit.resource = VPS_ACC_PCTCPU;
			else if (strcmp(resstr, "blockio") == 0)
				item->u.limit.resource = VPS_ACC_BLOCKIO;
			else if (strcmp(resstr, "threads") == 0)
				item->u.limit.resource = VPS_ACC_THREADS;
			else if (strcmp(resstr, "procs") == 0)
				item->u.limit.resource = VPS_ACC_PROCS;
			else {
				fprintf(stderr, "%s: invalid resource [%s]\n",
					__func__, resstr);
				free(vas.data);
				free(input);
				return (-1);
			}

			++item;
			if (++ap >= &argv2[0x100])
				break;
		}
	free(input);

	strncpy(vas.vps_name, argv[0], sizeof(vas.vps_name));
	vas.datalen = ((caddr_t)item) - (caddr_t)vas.data;
	if ((ioctl(vpsfd, VPS_IOC_ARGSET, &vas)) == -1) {
		fprintf (stderr, "ioctl VPS_IOC_ARGSET: %s\n",
			strerror(errno));
		return (errno);
	}

	return (0);
}

static int
vc_argtest_getall(int argc, char **argv)
{
	struct vps_arg_get vag;
	struct vps_arg_item *item;
	caddr_t data;
	int datalen;

	if (argc < 1)
		return (vc_usage(stderr));

	datalen = 0x100000;
	data = malloc(datalen);
	memset(&vag, 0, sizeof(vag));
	memset(data, 0, datalen);

	vag.data = data;
	vag.datalen = datalen;
	snprintf(vag.vps_name, sizeof(vag.vps_name), "%s", argv[0]);

	printf("vag.data=%p vag.datalen=%zu\n", vag.data, vag.datalen);
	if ((ioctl(vpsfd, VPS_IOC_ARGGET, &vag)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_ARGGET: %s\n",
			strerror(errno));
		return (errno);
	}

	fprintf(stdout, "ioctl VPS_IOC_ARGGET: got %zu bytes\n", vag.datalen);

	for (item = (struct vps_arg_item *)vag.data;
		(caddr_t)item < ((caddr_t)vag.data) + vag.datalen;
		item++) {

		switch (item->type) {
		case VPS_ARG_ITEM_PRIV:
			fprintf(stdout, "item: type=priv priv=%d value=%d\n",
				item->u.priv.priv, item->u.priv.value);
			break;
		case VPS_ARG_ITEM_IP4:
			fprintf(stdout, "item: type=ip4 addr=%08x mask=%08x\n",
				item->u.ip4.addr.s_addr, item->u.ip4.mask.s_addr);
			break;
		/*
		case VPS_ARG_ITEM_IP6:
			fprintf(stdout, "item: type=priv priv=%d value=%d\n",
				item->u.priv.priv, item->u.priv.value);
			break;
		*/
		case VPS_ARG_ITEM_LIMIT:
			fprintf(stdout, "item: type=limit resource=%u soft=%zu hard=%zu\n",
				item->u.limit.resource, item->u.limit.soft, item->u.limit.hard);
			break;
		default:
			fprintf(stdout, "unknown item type %d\n", item->type);
			break;
		}
	}
	fprintf(stdout, "---------------------------");

	return (0);
}

int
vc_argtest(int argc, char **argv)
{
	struct vps_arg_set vas;
	struct vps_arg_item *item;
	void *data;
	int datalen;

	if (argc < 1)
		return (vc_usage(stderr));

	vc_argtest_getall (argc, argv);

	datalen = 0x10000;
	data = malloc(datalen);
	memset(&vas, 0, sizeof(vas));
	memset(data, 0, datalen);

	vas.data = data;
	vas.datalen = datalen;
	snprintf(vas.vps_name, sizeof(vas.vps_name), "%s", argv[0]);

	item = (struct vps_arg_item *)data;
	item->type = VPS_ARG_ITEM_IP4;
	item->u.ip4.addr.s_addr = inet_addr("78.142.178.192");
	item->u.ip4.mask.s_addr = inet_addr("255.255.255.255");
	item++;
	item->type = VPS_ARG_ITEM_IP4;
	item->u.ip4.addr.s_addr = inet_addr("192.168.0.0");
	item->u.ip4.mask.s_addr = inet_addr("255.255.0.0");
	item++;
	item->type = VPS_ARG_ITEM_PRIV;
	item->u.priv.priv = 123;
	item->u.priv.value = VPS_ARG_PRIV_ALLOW;
	item++;
	vas.datalen = ((caddr_t)item) - (caddr_t)data;
	printf("vas.data=%p vas.datalen=%zu\n", vas.data, vas.datalen);
	if ((ioctl(vpsfd, VPS_IOC_ARGSET, &vas)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_ARGSET: %s\n",
			strerror(errno));
		return (-1);
	}
	vc_argtest_getall(argc, argv);

	item = (struct vps_arg_item *)data;
	item->type = VPS_ARG_ITEM_IP4;
	item->revoke = 1;
	item->u.ip4.addr.s_addr = inet_addr ("78.142.178.192");
	item->u.ip4.mask.s_addr = inet_addr ("255.255.255.255");
	item++;
	vas.datalen = ((caddr_t)item) - (caddr_t)data;
	printf("vas.data=%p vas.datalen=%zu\n", vas.data, vas.datalen);
	if ((ioctl(vpsfd, VPS_IOC_ARGSET, &vas)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_ARGSET: %s\n",
			strerror(errno));
		return (-1);
	}
	vc_argtest_getall(argc, argv);

	return (0);
}

int
vc_quota_recalc(int argc, char **argv)
{
	struct vps_arg_item *items;
	struct vps_arg_get va;
	size_t datalen;
	caddr_t data;

	if (argc < 1)
		return (vc_usage(stderr));

	datalen = 0x10000;
	data = malloc(datalen);
	memset(&va, 0, sizeof(va));
	memset(data, 0, datalen);

	va.data = data;
	va.datalen = datalen;
	snprintf(va.vps_name, sizeof(va.vps_name), "%s", argv[0]);

	if ((ioctl(vpsfd, VPS_IOC_FSCALCPATH, &va)) == -1) {
		fprintf(stderr, "ioctl VPS_IOC_FSCALCPATH: %s\n",
			strerror(errno));
		return (errno);
	}

	items = (struct vps_arg_item *)va.data;
	printf("usage for vpsfs on [%s]: blocks_used=%zu nodes_used=%zu\n",
		va.vps_name, items[0].u.limit.cur, items[1].u.limit.cur);

	free(data);

	return (0);
}

int
vc_showdump(int argc, char **argv)
{
	struct vps_snapst_ctx *ctx;
	struct stat sb;
	int size, fd;
	void *p;

	if (argc < 1) {
		fprintf(stderr, "no path given\n");
		return (vc_usage(stderr));
	}

	if ((fd = open(argv[0], O_RDONLY)) == -1) {
		fprintf(stderr, "open [%s]: %s\n",
			argv[0], strerror(errno));
		return (errno);
	}
	if ((fstat(fd, &sb)) == -1) {
		fprintf(stderr, "fstat: %s\n",
			strerror(errno));
		return (errno);
	}
	size = sb.st_size;

	if ((p = mmap(NULL, size, PROT_READ|PROT_WRITE,
		      MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		fprintf(stderr, "mmap: %s\n",
			strerror(errno));
		return (errno);
	}

	ctx = malloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));
	ctx->data = p;
	ctx->dsize = size;
	ctx->rootobj = (struct vps_dumpobj *)(void *)(((caddr_t)ctx->data) +
			sizeof(struct vps_dumpheader));
	ctx->relative = 1;
	ctx->elements = -1;

	vps_libdump_printheader(p);
	if (vps_dumpobj_printtree(ctx))
		printf("%s: tree is invalid !\n", __func__);
	else
		printf("%s: tree is good !\n", __func__);

	free(ctx);
	munmap(p, size);
	close(fd);

	return (0);
}

int
vc_savefile(int argc, char **argv)
{
	char buf[0x1000];
	char *path;
	long size;
	long done;
	int mode;
	int fd;
	int rc;

	if (argc < 3)
		return (1);

	size = atoi(argv[1]);
	if (size < 1)
		return (1);

	path = argv[0];
	if (strlen(path) == 0)
		return (1);

	mode = atoi(argv[2]);

	write(1, "\n", 1);

	if ((fd = open(path, O_CREAT|O_TRUNC|O_WRONLY)) == -1) {
		fprintf(stderr, "open([%s], O_CREAT|O_TRUNC): error: %s\n",
		    path, strerror(errno));
		return (1);
	}

	if (fchmod(fd, mode) == -1) {
		fprintf(stderr, "fchmod(%d, %d): error: %s\n",
		    fd, mode, strerror(errno));
		close(fd);
		return (1);
	}

	done = 0;
	while (done < size) {
		rc = size-done;
		if (rc > sizeof(buf))
			rc = sizeof(buf);
		rc = read(0, buf, rc);
		if (rc == -1) {
			fprintf(stderr, "read(0, ...): error: %s\n",
			    strerror(errno));
			close(fd);
			return (-1);
		}
		done += rc;
		rc = write(fd, buf, rc);
		if (rc == -1) {
			fprintf(stderr, "write(%d, ...): error: %s\n",
			    fd, strerror(errno));
			close(fd);
			return (-1);
		}
	}
	close(fd);

	return (0);
}

static int
vc_ttyloop(int ptsmfd, const char *esc_pattern)
{
	struct termios tios;
	char buf_last[0x100];
	char buf[0x10000];
	int buf_last_len;
	fd_set rfds;
	int rlen;
	int wlen;
	int flags;
	int rc;

	tcgetattr(0, &tios);
	cfmakeraw(&tios);
	tcsetattr(0, TCSADRAIN, &tios);

	flags = fcntl(ptsmfd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(ptsmfd, F_SETFL, flags);

	flags = fcntl(0, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(0, F_SETFL, flags);

	memset(buf_last, 0, sizeof(buf_last));
	buf_last_len = 0;

	for (;;) {

		FD_ZERO(&rfds);
		FD_SET(ptsmfd, &rfds);
		FD_SET(0, &rfds);
		if ((select(ptsmfd+1, &rfds, NULL, NULL, NULL)) == -1) {
			fprintf(stderr, "select(): %s\n", strerror(errno));
			exit(-1);
		}

		if (FD_ISSET(ptsmfd, &rfds)) {

			rlen = read(ptsmfd, buf, sizeof(buf));
			if (rlen == 0) {
				return (0);
			} else if (rlen == -1) {
				if (errno == EINTR || errno == EAGAIN) {
					continue;
				} else {
					fprintf(stderr, "read(): %s\n", strerror(errno));
					return (-1);
				}
			}

			wlen = 0;
			while (wlen < rlen) {
				rc = write(1, buf+wlen, rlen-wlen);
				if (rc == -1) {
					if (errno == EINTR || errno == EAGAIN) {
						rc = 0;
					} else {
						fprintf(stderr, "write(): %s\n", strerror(errno));
						return (-1);
					}
				}
				wlen += rc;
			}
		}

		if (FD_ISSET(0, &rfds)) {

			rlen = read(0, buf, sizeof(buf));
			if (rlen == 0) {
				return (0);
			} else if (rlen == -1) {
				if (errno == EINTR || errno == EAGAIN) {
					continue;
				} else {
					fprintf(stderr, "read(): %s\n", strerror(errno));
					return (-1);
				}
			}

			if (esc_pattern != NULL) {
				int llen;

				assert(strlen(esc_pattern) < sizeof(buf_last));

				if (rlen > (int)sizeof(buf_last) - 1)
					llen = sizeof(buf_last) - 1;
				else
					llen = rlen;

				if (llen > (int)strlen(esc_pattern))
					buf_last_len = 0;

				if (buf_last_len > 0x10) {
					memmove(buf_last, buf_last + llen, buf_last_len);
					buf_last_len -= llen;
				}
				memcpy(buf_last + buf_last_len, buf + rlen - llen, llen);
				buf_last_len += llen;
				buf_last[buf_last_len] = 0;

				/*
				fprintf(stderr,
				    "esc_pattern=[%02x %02x %02x %02x %02x %02x %02x %02x]\n",
				    esc_pattern[0], esc_pattern[1], esc_pattern[2], esc_pattern[3],
				    esc_pattern[4], esc_pattern[5], esc_pattern[6], esc_pattern[7]);
				fprintf(stderr,
				    "buf_last=[%02x %02x %02x %02x %02x %02x %02x %02x]\n",
				    buf_last[0], buf_last[1], buf_last[2], buf_last[3],
				    buf_last[4], buf_last[5], buf_last[6], buf_last[7]);
				*/

				if (strstr(buf_last, esc_pattern) != NULL) {
					/* fprintf(stderr, "matched escape pattern!\n"); */
					return (0);
				}
			}

			wlen = 0;
			while (wlen < rlen) {
				rc = write(ptsmfd, buf+wlen, rlen-wlen);
				if (rc == -1) {
					if (errno == EINTR || errno == EAGAIN) {
						rc = 0;
					} else {
						fprintf(stderr, "write(): %s\n", strerror(errno));
						return (-1);
					}
				}
				wlen += rc;
			}
		}

	} /* for() */

	return (rc);
}

/* EOF */
