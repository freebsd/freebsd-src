/*
 * Copyright (c) 1993 Terrence R. Lambert.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: modload.c,v 1.11 1996/04/26 18:39:48 erich Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <err.h>
#include <a.out.h>

#include <sys/param.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/lkm.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include "pathnames.h"

#define	min(a, b)	((a) < (b) ? (a) : (b))

int debug = 0;
int verbose = 0;
int quiet = 0;
int dounlink = 0;

extern char *sys_siglist[];

/*
 * Expected linker options:
 *
 * -A		executable to link against
 * -e		entry point
 * -o		output file
 * -T		address to link to in hex (assumes it's a page boundry)
 * <target>	object file
 */

void
linkcmd(kernel, entry, outfile, address, object)
	char *kernel, *entry, *outfile;
	u_int address;	/* XXX */
	char *object;
{
	char addrbuf[32], entrybuf[_POSIX2_LINE_MAX];
	pid_t pid;
	int status;

	snprintf(entrybuf, sizeof entrybuf, "_%s", entry);
	snprintf(addrbuf, sizeof addrbuf, "%x", address);

	if (debug)
		printf("%s -A %s -e %s -o %s -T %s %s\n",
		       _PATH_LD, kernel, entrybuf, outfile,
		       addrbuf, object);

	pid = fork();
	if(pid < 0) {
		err(18, "fork");
	}

	if(pid == 0) {
		execl(_PATH_LD, "ld", "-A", kernel, "-e", entrybuf, "-o",
		      outfile, "-T", addrbuf, object, (char *)0);
		exit(128 + errno);
	}

	waitpid(pid, &status, 0);

	if(WIFSIGNALED(status)) {
		errx(1, "%s got signal: %s", _PATH_LD,
		     sys_siglist[WTERMSIG(status)]);
	}

	if(WEXITSTATUS(status) > 128) {
		errno = WEXITSTATUS(status) - 128;
		err(1, "exec(%s)", _PATH_LD);
	}

	if(WEXITSTATUS(status) != 0) {
		errx(1, "%s: return code %d", _PATH_LD, WEXITSTATUS(status));
	}

}

void
usage()
{

	fprintf(stderr, "usage:\n");
	fprintf(stderr, "modload [-d] [-v] [-q] [-u] [-A <kernel>] [-e <entry]\n");
	fprintf(stderr,
	    "[-p <postinstall>] [-o <output file>] <input file>\n");
	exit(1);
}

int fileopen = 0;
#define	DEV_OPEN	0x01
#define	MOD_OPEN	0x02
#define	PART_RESRV	0x04
int devfd, modfd;
struct lmc_resrv resrv;

void
cleanup()
{
	if (fileopen & PART_RESRV) {
		/*
		 * Free up kernel memory
		 */
		if (ioctl(devfd, LMUNRESRV, 0) == -1)
			warn("can't release slot 0x%08x memory", resrv.slot);
	}

	if (fileopen & DEV_OPEN)
		close(devfd);

	if (fileopen & MOD_OPEN)
		close(modfd);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	char *kname = (char *)getbootfile();
	char *entry = NULL;
	char *post = NULL;
	char *out = NULL;
	char *modobj;
	char modout[80], *p;
	struct exec info_buf;
	u_int modsize;	/* XXX */
	u_int modentry;	/* XXX */

	struct lmc_loadbuf ldbuf;
	int sz, bytesleft;
	char buf[MODIOBUF];

	while ((c = getopt(argc, argv, "dvquA:e:p:o:")) != EOF) {
		switch (c) {
		case 'd':
			debug = 1;
			break;	/* debug */
		case 'v':
			verbose = 1;
			break;	/* verbose */
		case 'u':
			dounlink = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'A':
			kname = optarg;
			break;	/* kernel */
		case 'e':
			entry = optarg;
			break;	/* entry point */
		case 'p':
			post = optarg;
			break;	/* postinstall */
		case 'o':
			out = optarg;
			break;	/* output file */
		case '?':
			usage();
		default:
			printf("default!\n");
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	modobj = argv[0];

	atexit(cleanup);

	/*
	 * Open the virtual device device driver for exclusive use (needed
	 * to write the new module to it as our means of getting it in the
	 * kernel).
	 */
	if ((devfd = open(_PATH_LKM, O_RDWR, 0)) == -1)
		err(3, _PATH_LKM);
	fileopen |= DEV_OPEN;

	p = strrchr(modobj, '.');
	if (!p || strcmp(p, ".o"))
		errx(2, "module object must end in .o");

	if (!out) {
		p = strrchr(modobj, '/');
		if (p)
			p++;			/* skip over '/' */
		else
			p = modobj;
		snprintf(modout, sizeof modout, "%s%sut", _PATH_TMP, p);
		out = modout;
		/*
		 * reverse meaning of -u - if we've generated a /tmp
		 * file, remove it automatically...
		 */
		dounlink = !dounlink;
	}

	if (!entry) {	/* calculate default entry point */
		entry = strrchr(modobj, '/');
		if (entry)
			entry++;		/* skip over '/' */
		else
			entry = modobj;
		entry = strdup(entry);		/* so we can modify it */
		if (!entry)
			errx(1, "Could not allocate memory");
		entry[strlen(entry) - 2] = '\0'; /* chop off .o */
	}

	modfd = open(out, O_RDWR | O_CREAT, 0666);
	if(modfd < 0) {
		err(1, "creating %s", out);
	}
	close(modfd);

	/*
	 * Prelink to get file size
	 */
	linkcmd(kname, entry, out, 0, modobj);

	/*
	 * Pre-open the 0-linked module to get the size information
	 */
	if ((modfd = open(out, O_RDONLY, 0)) == -1)
		err(4, out);
	fileopen |= MOD_OPEN;

	/*
	 * Get the load module post load size... do this by reading the
	 * header and doing page counts.
	 */
	if (read(modfd, &info_buf, sizeof(struct exec)) == -1)
		err(3, "read `%s'", out);

	/*
	 * Close the dummy module -- we have our sizing information.
	 */
	close(modfd);
	fileopen &= ~MOD_OPEN;

	/*
	 * Magic number...
	 */
	if (N_BADMAG(info_buf))
		errx(4, "not an a.out format file");

	/*
	 * Calculate the size of the module
	 */
 	modsize = info_buf.a_text + info_buf.a_data + info_buf.a_bss;

	/*
	 * Reserve the required amount of kernel memory -- this may fail
	 * to be successful.
	 */
	resrv.size = modsize;	/* size in bytes */
	resrv.name = modout;	/* objname w/o ".o" */
	resrv.slot = -1;	/* returned */
	resrv.addr = 0;		/* returned */
	if (ioctl(devfd, LMRESERV, &resrv) == -1)
		err(9, "can't reserve memory");
	fileopen |= PART_RESRV;

	/*
	 * Relink at kernel load address
	 */
	linkcmd(kname, entry, out, resrv.addr, modobj);

	/*
	 * Open the relinked module to load it...
	 */
	if ((modfd = open(out, O_RDONLY, 0)) == -1)
		err(4, out);
	fileopen |= MOD_OPEN;

	/*
	 * Reread the header to get the actual entry point *after* the
	 * relink.
	 */
	if (read(modfd, &info_buf, sizeof(struct exec)) == -1)
		err(3, "read `%s'", out);

	/*
	 * Get the entry point (for initialization)
	 */
	modentry = info_buf.a_entry;			/* place to call */

	/*
	 * Seek to the text offset to start loading...
	 */
	if (lseek(modfd, N_TXTOFF(info_buf), 0) == -1)
		err(12, "lseek");

	/*
	 * Transfer the relinked module to kernel memory in chunks of
	 * MODIOBUF size at a time.
	 */
	for (bytesleft = info_buf.a_text + info_buf.a_data;
	    bytesleft > 0;
	    bytesleft -= sz) {
		sz = min(bytesleft, MODIOBUF);
		read(modfd, buf, sz);
		ldbuf.cnt = sz;
		ldbuf.data = buf;
		if (ioctl(devfd, LMLOADBUF, &ldbuf) == -1)
			err(11, "error transferring buffer");
	}

	/*
	 * Save ourselves before disaster (potentitally) strikes...
	 */
	sync();

	/*
	 * Trigger the module as loaded by calling the entry procedure;
	 * this will do all necessary table fixup to ensure that state
	 * is maintained on success, or blow everything back to ground
	 * zero on failure.
	 */
	if (ioctl(devfd, LMREADY, &modentry) == -1)
		err(14, "error initializing module");

	/*
	 * Success!
	 */
	fileopen &= ~PART_RESRV;	/* loaded */
	if(!quiet) printf("Module loaded as ID %d\n", resrv.slot);

	if (post) {
	    struct lmc_stat sbuf;
	    char id[16], type[16], offset[16];

	    sbuf.id = resrv.slot;
	    if (ioctl(devfd, LMSTAT, &sbuf) == -1)
		err(15, "error fetching module stats for post-install");
	    sprintf(id, "%d", sbuf.id);
	    sprintf(type, "0x%x", sbuf.type);
	    sprintf(offset, "%d", sbuf.offset);
	    /* XXX the modload docs say that drivers can install bdevsw &
	       cdevsw, but the interface only supports one at a time.  sigh. */
	    execl(post, post, id, type, offset, 0);
	    err(16, "can't exec '%s'", post);
	}

	if(dounlink) {
		if(unlink(out)) {
			err(17, "unlink(%s)", out);
		}
	}

	return 0;
}
