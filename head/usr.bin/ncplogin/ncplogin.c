/*
 * Copyright (c) 1999, Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <netncp/ncp_lib.h>
#include <netncp/ncp_rcfile.h>

extern char *__progname;

static void
login_usage(void) {
	printf("usage: %s [-Dh] [-A host] [-BCN] [-I level] [-M mode] \n"
	       "       [-R retrycount] [-W timeout] /server:user\n", __progname);
	exit(1);
}

static void
logout_usage(void) {
	printf("usage: %s [-c handle] [-h] [/server:user]\n", __progname);
	exit(1);
}

static void
login(int argc, char *argv[], struct ncp_conn_loginfo *li) {
	int error = 0, connid, opt, setprimary = 0;

	while ((opt = getopt(argc, argv, STDPARAM_OPT"D")) != -1) {
		switch(opt){
		    case STDPARAM_ARGS:
			if (ncp_li_arg(li, opt, optarg))	
				exit(1);
			break;
		    case 'D':
			setprimary = 1;
			break;
		    default:
			login_usage();
			/*NOTREACHED*/
		}
	}
	if (li->access_mode == 0)
		li->access_mode = S_IRWXU;
	if (ncp_li_check(li))
		exit(1);
	li->opt |= NCP_OPT_WDOG | NCP_OPT_PERMANENT;
	/* now we can try to login, or use already established connection */
	error = ncp_li_login(li, &connid);
	if (error) {
		ncp_error("Could not login to server %s", error, li->server);
		exit(1);
	}
	error = ncp_setpermanent(connid, 1);
	if (error && errno != EACCES){
		ncp_error("Can't make connection permanent", error);
		exit(1);
	}
	if (setprimary && ncp_setprimary(connid, 1) != 0)
		ncp_error("Warning: can't make connection primary", errno);
	printf("Logged in with conn handle:%d\n", connid);
	return;
}

static void
logout(int argc, char *argv[], struct ncp_conn_loginfo *li) {
	int error = 0, connid, opt;

	connid = -1;
	while ((opt = getopt(argc, argv, STDPARAM_OPT"c:")) != -1){
		switch (opt) {
		    case 'c':
			connid = atoi(optarg);
			break;
		    case STDPARAM_ARGS:
			if (ncp_li_arg(li, opt, optarg))
				exit(1);
			break;
		    default:
			logout_usage();
			/*NOTREACHED*/
		}
	}
	if (connid == -1) {
		if (li->server[0] == 0)
			errx(EX_USAGE, "no server name specified");
		if (li->user == 0) 
			errx(EX_USAGE, "no user name specified");
		if (ncp_conn_scan(li, &connid))
			errx(EX_OSERR, "You are not attached to server %s",
			     li->server);
	}
	if (ncp_setpermanent(connid, 0) < 0 && errno != EACCES) {
		ncp_error("Connection isn't valid", errno);
		exit(EX_OSERR);
	}
        error = ncp_disconnect(connid);
	if (error) {
		if (errno == EACCES) {
			warnx("you logged out, but connection belongs"
			      "to other user and not closed");
		} else {
			ncp_error("Can't logout with connid %d", error, connid);
			error = 1;
		}
	}
	exit(error ? 1 : 0);
}

int
main(int argc, char *argv[]) {
	int islogin, error;
	char *p, *p1;
	struct ncp_conn_loginfo li;

	islogin = strcmp(__progname, "ncplogin") == 0;

	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0) {
			if (islogin)
				login_usage();
			else
				logout_usage();
		}
	}

	if (ncp_initlib())
		exit(1);
	if (ncp_li_init(&li, argc, argv))
		return 1;

	if (argc >= 2 && argv[argc - 1][0] == '/') {
		p = argv[argc - 1];
		error = 1;
		do {
			if (*p++ != '/')
				break;
			p1 = strchr(p, ':');
			if (p1 == NULL)
				break;
			*p1++ = 0;
			if (ncp_li_setserver(&li, p))
				break;
			if (*p1 == 0)
				break;
			if (ncp_li_setuser(&li, p1)) break;
			error = 0;
		} while(0);
		if (error)
			errx(EX_DATAERR, 
			    "an error occured while parsing '%s'",
			    argv[argc - 1]);
	}

	if (ncp_li_readrc(&li))
		return 1;
	if (ncp_rc)
		rc_close(ncp_rc);
	if (islogin)
		login(argc, argv, &li);
	else
		logout(argc, argv, &li);
	return 0;
}
