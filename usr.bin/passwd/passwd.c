/*
 * Copyright (c) 1988 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)passwd.c	5.5 (Berkeley) 7/6/91";*/
static char rcsid[] = "$Id: passwd.c,v 1.3 1994/05/14 19:40:58 ache Exp $";
#endif /* not lint */

#include <stdio.h>
#include <unistd.h>

/*
 * Note on configuration:
 *      Generally one would not use both Kerberos and YP
 *      to maintain passwords.
 *
 */
#ifdef KERBEROS
int use_kerberos = 1;
#else
int use_kerberos = 0;
#endif
#ifdef YP
int force_yp = 0;
int use_yp;
#else
int use_yp = 0;
#endif

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	register int ch;
	char *uname;
        int status;
        
#ifdef	YP
        use_yp = _yp_check(NULL);
#endif
        
	while ((ch = getopt(argc, argv, "lky")) != EOF)
		switch (ch) {
		case 'l':		/* change local password file */
			use_kerberos = 0;
			use_yp = 0;
			break;
		case 'k':		/* change Kerberos password */
#ifdef	KERBEROS
			use_kerberos = 1;
			use_yp = 0;
			break;
#else
                        usage();
                        exit(1);
#endif
		case 'y':		/* change YP password */
#ifdef	YP
                        if (!use_yp) {
                                fprintf(stderr, "passwd: YP not in use.\n");
                                usage();
                                exit(1);
                        }

                        /* XXX Maybe just exec yppasswd ?? */
			use_kerberos = 0;
			use_yp = 1;
                        force_yp = 1;
			break;
#else
                        usage();
                        exit(1);
#endif
		default:
		case '?':
			usage();
			exit(1);
		}

	argc -= optind;
	argv += optind;

	uname = getlogin();
        if (uname == NULL) {
                fprintf(stderr, "passwd: who are you ??\n");
                exit(1);
        }
        
	switch(argc) {
	case 0:
		break;
	case 1:
#ifdef	KERBEROS
		if (use_kerberos && strcmp(argv[0], uname)) {
			(void)fprintf(stderr, "passwd: %s\n\t%s\n%s\n",
"to change another user's Kerberos password, do",
"\"kinit user; passwd; kdestroy\";",
"to change a user's local passwd, use \"passwd -l user\"");
			exit(1);
		}
#endif
		uname = argv[0];
		break;
	default:
		usage();
		exit(1);
	}

#ifdef	KERBEROS
	if (use_kerberos)
		exit(krb_passwd());
#endif
#ifdef	YP
        if (force_yp || ((status = local_passwd(uname)) && use_yp))
		exit(yp_passwd(uname));
        exit(status);
#endif
	exit(local_passwd(uname));
}

usage()
{
#if	defined(KERBEROS) && defined(YP)
	(void)fprintf(stderr, "usage: passwd [-l] [-k] [-y] user\n");
#else	/* !(KERBEROS && YP) */
#if	defined(KERBEROS) || defined(YP)
	(void)fprintf(stderr, "usage: passwd [-l] user\n");
#else
	(void)fprintf(stderr, "usage: passwd user\n");
#endif
#endif	/* KERBEROS && YP */
}
