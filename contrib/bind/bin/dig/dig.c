#ifndef lint
static const char rcsid[] = "$Id: dig.c,v 8.46 2001/04/01 17:35:01 vixie Exp $";
#endif

/*
 * Copyright (c) 1989
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*********************** Notes for the BIND 4.9 release (Paul Vixie, DEC)
 *	dig 2.0 was written by copying sections of libresolv.a and nslookup
 *	and modifying them to be more useful for a general lookup utility.
 *	as of BIND 4.9, the changes needed to support dig have mostly been
 *	incorporated into libresolv.a and nslookup; dig now links against
 *	some of nslookup's .o files rather than #including them or maintaining
 *	local copies of them.
 *
 *	while merging dig back into the BIND release, i made a number of
 *	structural changes.  for one thing, i put all of dig's private
 *	library routines into this file rather than maintaining them in
 *	separate, #included, files.  i don't like to #include ".c" files.
 *	i removed all calls to "bcopy", replacing them with structure
 *	assignments.  i removed all "extern"'s of standard functions,
 *	replacing them with #include's of standard header files.  this
 *	version of dig is probably as portable as the rest of BIND.
 *
 *	i had to remove the query-time and packet-count statistics since
 *	the current libresolv.a is a lot harder to modify to maintain these
 *	than the 4.8 one (used in the original dig) was.  for consolation,
 *	i added a "usage" message with extensive help text.
 *
 *	to save my (limited, albeit) sanity, i ran "indent" over the source.
 *	i also added the standard berkeley/DEC copyrights, since this file now
 *	contains a fair amount of non-USC code.  note that the berkeley and
 *	DEC copyrights do not prohibit redistribution, with or without fee;
 *	we add them only to protect ourselves (you have to claim copyright
 *	in order to disclaim liability and warranty).
 *
 *	Paul Vixie, Palo Alto, CA, April 1993
 ****************************************************************************

 ******************************************************************
 *      DiG -- Domain Information Groper                          *
 *                                                                *
 *        dig.c - Version 2.1 (7/12/94) ("BIND takeover")         *
 *                                                                *
 *        Developed by: Steve Hotz & Paul Mockapetris             *
 *        USC Information Sciences Institute (USC-ISI)            *
 *        Marina del Rey, California                              *
 *        1989                                                    *
 *                                                                *
 *        dig.c -                                                 *
 *           Version 2.0 (9/1/90)                                 *
 *               o renamed difftime() difftv() to avoid           *
 *                 clash with ANSI C                              *
 *               o fixed incorrect # args to strcmp,gettimeofday  *
 *               o incorrect length specified to strncmp          *
 *               o fixed broken -sticky -envsa -envset functions  *
 *               o print options/flags redefined & modified       *
 *                                                                *
 *           Version 2.0.beta (5/9/90)                            *
 *               o output format - helpful to `doc`               *
 *               o minor cleanup                                  *
 *               o release to beta testers                        *
 *                                                                *
 *           Version 1.1.beta (10/26/89)                          *
 *               o hanging zone transer (when REFUSED) fixed      *
 *               o trailing dot added to domain names in RDATA    *
 *               o ISI internal                                   *
 *                                                                *
 *           Version 1.0.tmp  (8/27/89)                           *
 *               o Error in prnttime() fixed                      *
 *               o no longer dumps core on large pkts             *
 *               o zone transfer (axfr) added                     *
 *               o -x added for inverse queries                   *
 *                               (i.e. "dig -x 128.9.0.32")       *
 *               o give address of default server                 *
 *               o accept broadcast to server @255.255.255.255    *
 *                                                                *
 *           Version 1.0  (3/27/89)                               *
 *               o original release                               *
 *                                                                *
 *     DiG is Public Domain, and may be used for any purpose as   *
 *     long as this notice is not removed.                        *
 ******************************************************************/

/* Import. */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <isc/dst.h>

#include <assert.h>
#include <ctype.h> 
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <resolv.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "port_after.h"

#include "../nslookup/res.h"

/* Global. */

#define VERSION 83
#define VSTRING "8.3"

#define PRF_DEF		0x2ff9
#define PRF_MIN		0xA930
#define PRF_ZONE        0x24f9

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

#define SAVEENV "DiG.env"
#define DIG_MAXARGS 30

static int		eecode = 0;
static FILE *		qfp;
static char		*defsrv, *srvmsg;
static char		defbuf[40] = "default -- ";
static char		srvbuf[60];
static char		myhostname[MAXHOSTNAMELEN];
static struct sockaddr_in myaddress;
static u_int32_t	ixfr_serial;

/* stuff for nslookup modules */
struct __res_state  res;
FILE		*filePtr;
jmp_buf		env;
HostInfo	*defaultPtr = NULL;
HostInfo	curHostInfo, defaultRec;
int		curHostValid = FALSE;
int		queryType, queryClass;
extern int	StringToClass(), StringToType();	/* subr.c */
#if defined(BSD) && BSD >= 199006 && !defined(RISCOS_BSD)
FILE		*yyin = NULL;
void		yyrestart(FILE *f) { }
#endif
char		*pager = NULL;
/* end of nslookup stuff */

/* Forward. */

static void		Usage(void);
static int		setopt(const char *);
static void		res_re_init(void);
static int		xstrtonum(char *);
static int		printZone(ns_type, const char *,
				  const struct sockaddr_in *, ns_tsig_key *);
static int		print_axfr(FILE *output, const u_char *msg,
				   size_t msglen);
static struct timeval	difftv(struct timeval, struct timeval);
static void		prnttime(struct timeval);
static void		stackarg(char *, char **);

/* Public. */

int
main(int argc, char **argv) {
	struct hostent *hp;
	short port = htons(NAMESERVER_PORT);
	/* Wierd stuff for SPARC alignment, hurts nothing else. */
	union {
		HEADER header_;
		u_char packet_[PACKETSZ];
	} packet_;
#define header (packet_.header_)
#define	packet (packet_.packet_)
	u_char answer[64*1024];
	int n;
	char doping[90];
	char pingstr[50];
	char *afile;
	char *addrc, *addrend, *addrbegin;

	time_t exectime;
	struct timeval tv1, tv2, start_time, end_time, query_time;

	char *srv;
	int anyflag = 0;
	int sticky = 0;
	int tmp; 
	int qtypeSet;
	int addrflag = 0;
	ns_type xfr = ns_t_invalid;
        int bytes_out, bytes_in;

	char cmd[512];
	char domain[MAXDNAME];
        char msg[120], **vtmp;
	char *args[DIG_MAXARGS];
	char **ax;
	int once = 1, dofile = 0; /* batch -vs- interactive control */
	char fileq[384];
	int  fp;
	int wait=0, delay;
	int envset=0, envsave=0;
	struct __res_state res_x, res_t;

	ns_tsig_key key;
	char *keyfile = NULL, *keyname = NULL;

	res_ninit(&res);
	res.pfcode = PRF_DEF;
	qtypeSet = 0;
	memset(domain, 0, sizeof domain);
	gethostname(myhostname, (sizeof myhostname));
#ifdef HAVE_SA_LEN
	myaddress.sin_len = sizeof(struct sockaddr_in);
#endif
	myaddress.sin_family = AF_INET;
	myaddress.sin_addr.s_addr = INADDR_ANY;
	myaddress.sin_port = 0; /*INPORT_ANY*/;
	defsrv = strcat(defbuf, inet_ntoa(res.nsaddr.sin_addr));
	res_x = res;

/*
 * If LOCALDEF in environment, should point to file
 * containing local favourite defaults.  Also look for file
 * DiG.env (i.e. SAVEENV) in local directory.
 */

	if ((((afile = (char *) getenv("LOCALDEF")) != (char *) NULL) &&
	     ((fp = open(afile, O_RDONLY)) > 0)) ||
	    ((fp = open(SAVEENV, O_RDONLY)) > 0)) {
		read(fp, (char *)&res_x, (sizeof res_x));
		close(fp);
		res = res_x;
	}
/*
 * Check for batch-mode DiG; also pre-scan for 'help'.
 */
	vtmp = argv;
	ax = args;
	while (*vtmp != NULL) {
		if (strcmp(*vtmp, "-h") == 0 ||
		    strcmp(*vtmp, "-help") == 0 ||
		    strcmp(*vtmp, "-usage") == 0 ||
		    strcmp(*vtmp, "help") == 0) {
			Usage();
			exit(0);
		}

		if (strcmp(*vtmp, "-f") == 0) {
			dofile++; once=0;
			if ((qfp = fopen(*++vtmp, "r")) == NULL) {
				fflush(stdout);
				perror("file open");
				fflush(stderr);
				exit(10);
			}
		} else {
			if (ax - args == DIG_MAXARGS) {
				fprintf(stderr, "dig: too many arguments\n");
				exit(10);
			}
			*ax++ = *vtmp;
		}
		vtmp++;
	}

	res.id = 1;
	gettimeofday(&tv1, NULL);
	assert(tv1.tv_usec >= 0 && tv1.tv_usec < 1000000);

/*
 * Main section: once if cmd-line query
 *               while !EOF if batch mode
 */
	*fileq = '\0';
	while ((dofile && fgets(fileq, sizeof fileq, qfp) != NULL) || 
	       (!dofile && once--)) 
	{
		if (*fileq == '\n' || *fileq == '#' || *fileq==';') {
			printf("%s", fileq);	/* echo but otherwise ignore */
			continue;		/* blank lines and comments  */
		}

/*
 * "Sticky" requests that before current parsing args
 * return to current "working" environment (X******).
 */
		if (sticky) {
			printf(";; (using sticky settings)\n");
			res = res_x;
		}

/*
 * Concat cmd-line and file args.
 */
		stackarg(fileq, ax);

		/* defaults */
		queryType = ns_t_ns;
		queryClass = ns_c_in;
		xfr = ns_t_invalid;
		*pingstr = 0;
		srv = NULL;

		sprintf(cmd, "\n; <<>> DiG %s <<>> ", VSTRING);
		argv = args;
		argc = ax - args;
/*
 * More cmd-line options than anyone should ever have to
 * deal with ....
 */
		while (*(++argv) != NULL && **argv != '\0') { 
			strcat(cmd, *argv);
			strcat(cmd, " ");
			if (**argv == '@') {
				srv = (*argv+1);
				continue;
			}
			if (**argv == '%')
				continue;
			if (**argv == '+') {
				setopt(*argv+1);
				continue;
			}
			if (**argv == '=') {
				ixfr_serial = strtoul(*argv+1, NULL, 0);
				continue;
			}
			if (strncmp(*argv, "-nost", 5) == 0) {
				sticky = 0;
				continue;
			} else if (strncmp(*argv, "-st", 3) == 0) {
				sticky++;
				continue;
			} else if (strncmp(*argv, "-envsa", 6) == 0) {
				envsave++;
				continue;
			} else if (strncmp(*argv, "-envse", 6) == 0) {
				envset++;
				continue;
			}

			if (**argv == '-') {
				switch (argv[0][1]) { 
				case 'T':
					if (*++argv == NULL)
						printf("; no arg for -T?\n");
					else
						wait = atoi(*argv);
					break;
				case 'c': 
					if(*++argv == NULL) 
						printf("; no arg for -c?\n");
					else if ((tmp = atoi(*argv))
						  || *argv[0] == '0') {
						queryClass = tmp;
					} else if ((tmp = StringToClass(*argv,
								       0, NULL)
						   ) != 0) {
						queryClass = tmp;
					} else {
						printf(
						  "; invalid class specified\n"
						       );
					}
					break;
				case 't': 
					if (*++argv == NULL)
						printf("; no arg for -t?\n");
					else if ((tmp = atoi(*argv))
					    || *argv[0]=='0') {
						queryType = tmp;
						qtypeSet++;
					} else if ((tmp = StringToType(*argv,
								      0, NULL)
						   ) != 0) {
						queryType = tmp;
						qtypeSet++;
					} else {
						printf(
						   "; invalid type specified\n"
						       );
					}
					break;
				case 'x':
					if (!qtypeSet) {
						queryType = T_ANY;
						qtypeSet++;
					}
					if ((addrc = *++argv) == NULL) {
						printf("; no arg for -x?\n");
						break;
					}
					addrend = addrc + strlen(addrc);
					if (*addrend == '.')
						*addrend = '\0';
					*domain = '\0';
					while ((addrbegin = strrchr(addrc,'.'))) {
						strcat(domain, addrbegin+1);
						strcat(domain, ".");
						*addrbegin = '\0';
					}
					strcat(domain, addrc);
					strcat(domain, ".in-addr.arpa.");
					break;
				case 'p':
					if (argv[0][2] != '\0')
						port = ntohs(atoi(argv[0]+2));
					else if (*++argv == NULL)
						printf("; no arg for -p?\n");
					else
						port = htons(atoi(*argv));
					break;
				case 'P':
					if (argv[0][2] != '\0')
						strcpy(pingstr, argv[0]+2);
					else
						strcpy(pingstr, "ping -s");
					break;
				case 'n':
					if (argv[0][2] != '\0')
						res.ndots = atoi(argv[0]+2);
					else if (*++argv == NULL)
						printf("; no arg for -n?\n");
					else
						res.ndots = atoi(*argv);
					break;
				case 'b': {
					char *a, *p;

					if (argv[0][2] != '\0')
						a = argv[0]+2;
					else if (*++argv == NULL) {
						printf("; no arg for -b?\n");
						break;
					} else
						a = *argv;
					if ((p = strchr(a, ':')) != NULL) {
						*p++ = '\0';
						myaddress.sin_port =
							ntohs(atoi(p));
					}
					if (!inet_aton(a,&myaddress.sin_addr)){
						fprintf(stderr,
							";; bad -b addr\n");
						exit(1);
					}
				    }
				    break;
				case 'k':
					/* -k keydir:keyname */
					
					if (argv[0][2] != '\0')
						keyfile = argv[0]+2;
					else if (*++argv == NULL) {
						printf("; no arg for -k?\n");
						break;
					} else
						keyfile = *argv;

					keyname = strchr(keyfile, ':');
					if (keyname == NULL) {
						fprintf(stderr,
			     "key option argument should be keydir:keyname\n");
						exit(1);
					}
					*keyname++='\0';
					break;
				} /* switch - */
				continue;
			} /* if '-'   */

			if ((tmp = StringToType(*argv, -1, NULL)) != -1) { 
				if ((T_ANY == tmp) && anyflag++) {  
					queryClass = C_ANY; 	
					continue; 
				}
				if (ns_t_xfr_p(tmp) &&
				    (tmp == ns_t_axfr ||
				     (res.options & RES_USEVC) != 0)
				     ) {
					res.pfcode = PRF_ZONE;
					xfr = (ns_type)tmp;
				} else {
					queryType = tmp; 
					qtypeSet++;
				}
			} else if ((tmp = StringToClass(*argv, -1, NULL))
				   != -1) { 
				queryClass = tmp; 
			} else {
				memset(domain, 0, sizeof domain);
				sprintf(domain,"%s",*argv);
			}
		} /* while argv remains */

		/* process key options */
		if (keyfile) {
#ifdef PARSE_KEYFILE
			int i, n1;
			char buf[BUFSIZ], *p;
			FILE *fp = NULL;
			int file_major, file_minor, alg;

			fp = fopen(keyfile, "r");
			if (fp == NULL) {
				perror(keyfile);
				exit(1);
			}
			/* Now read the header info from the file. */
			i = fread(buf, 1, BUFSIZ, fp);
			if (i < 5) {
				fclose(fp);
	                	exit(1);
	        	}
			fclose(fp);
	
			p = buf;
	
			n=strlen(p);		/* get length of strings */
			n1=strlen("Private-key-format: v");
			if (n1 > n ||
			    strncmp(buf, "Private-key-format: v", n1)) {
				fprintf(stderr, "Invalid key file format\n");
				exit(1);	/* not a match */
			}
			p+=n1;		/* advance pointer */
			sscanf((char *)p, "%d.%d", &file_major, &file_minor);
			/* should do some error checking with these someday */
			while (*p++!='\n');	/* skip to end of line */
	
	        	n=strlen(p);		/* get length of strings */
	        	n1=strlen("Algorithm: ");
	        	if (n1 > n || strncmp(p, "Algorithm: ", n1)) {
				fprintf(stderr, "Invalid key file format\n");
	                	exit(1);	/* not a match */
			}
			p+=n1;		/* advance pointer */
			if (sscanf((char *)p, "%d", &alg)!=1) {
				fprintf(stderr, "Invalid key file format\n");
				exit(1);
			}
			while (*p++!='\n');	/* skip to end of line */
	
	        	n=strlen(p);		/* get length of strings */
	        	n1=strlen("Key: ");
	        	if (n1 > n || strncmp(p, "Key: ", n1)) {
				fprintf(stderr, "Invalid key file format\n");
				exit(1);	/* not a match */
			}
			p+=n1;		/* advance pointer */
			pp=p;
			while (*pp++!='\n');	/* skip to end of line,
						 * terminate it */
			*--pp='\0';
	
			key.data=malloc(1024*sizeof(char));
			key.len=b64_pton(p, key.data, 1024);
	
			strcpy(key.name, keyname);
			strcpy(key.alg, "HMAC-MD5.SIG-ALG.REG.INT");
#else
			/* use the dst* routines to parse the key files
			 * 
			 * This requires that both the .key and the .private
			 * files exist in your cwd, so the keyfile parmeter
			 * here is assumed to be a path in which the
			 * K*.{key,private} files exist.
			 */
			DST_KEY *dst_key;
			char cwd[PATH_MAX+1];
	
			if (getcwd(cwd, PATH_MAX)==NULL) {
				perror("unable to get current directory");
				exit(1);
			}
			if (chdir(keyfile)<0) {
				fprintf(stderr,
					"unable to chdir to %s: %s\n", keyfile,
					strerror(errno));
				exit(1);
			}
	
			dst_init();
			dst_key = dst_read_key(keyname,
					       0 /* not used for priv keys */,
					       KEY_HMAC_MD5, DST_PRIVATE);
			if (!dst_key) {
				fprintf(stderr,
					"dst_read_key: error reading key\n");
				exit(1);
			}
			key.data=malloc(1024*sizeof(char));
			dst_key_to_buffer(dst_key, key.data, 1024);
			key.len=dst_key->dk_key_size;
	
			strcpy(key.name, keyname);
			strcpy(key.alg, "HMAC-MD5.SIG-ALG.REG.INT");
	
			if (chdir(cwd)<0) {
				fprintf(stderr, "unable to chdir to %s: %s\n",
					cwd, strerror(errno));
				exit(1);
			}
#endif
		}

		if (res.pfcode & 0x80000)
			printf("; pfcode: %08lx, options: %08lx\n",
			       res.pfcode, res.options);
	  
/*
 * Current env. (after this parse) is to become the
 * new "working" environmnet. Used in conj. with sticky.
 */
		if (envset) {
			res_x = res;
			envset = 0;
		}

/*
 * Current env. (after this parse) is to become the
 * new default saved environmnet. Save in user specified
 * file if exists else is SAVEENV (== "DiG.env").
 */
		if (envsave) {
			afile = (char *) getenv("LOCALDEF");
			if ((afile &&
			     ((fp = open(afile,
					 O_WRONLY|O_CREAT|O_TRUNC,
					 S_IREAD|S_IWRITE)) > 0))
			    ||
			    ((fp = open(SAVEENV,
					O_WRONLY|O_CREAT|O_TRUNC,
					S_IREAD|S_IWRITE)) > 0)) {
				write(fp, (char *)&res, (sizeof res));
				close(fp);
			}
			envsave = 0;
		}

		if (res.pfcode & RES_PRF_CMD)
			printf("%s\n", cmd);

		addrflag = anyflag = 0;

/*
 * Find address of server to query. If not dot-notation, then
 * try to resolve domain-name (if so, save and turn off print 
 * options, this domain-query is not the one we want. Restore
 * user options when done.
 * Things get a bit wierd since we need to use resolver to be
 * able to "put the resolver to work".
 */

		srvbuf[0] = 0;
		srvmsg = defsrv;
		if (srv != NULL) {
			struct in_addr addr;

			if (inet_aton(srv, &addr)) {
				res.nscount = 1;
				res.nsaddr.sin_addr = addr;
				srvmsg = strcat(srvbuf, srv);
			} else {
				res_t = res;
				res_ninit(&res);
				res.pfcode = 0;
				res.options = RES_DEFAULT;
				hp = gethostbyname(srv);
				res = res_t;
				if (hp == NULL
				    || hp->h_addr_list == NULL
				    || *hp->h_addr_list == NULL) {
					fflush(stdout);
					fprintf(stderr,
		"; Bad server: %s -- using default server and timer opts\n",
						srv);
					fflush(stderr);
					srvmsg = defsrv;
					srv = NULL;
				} else {
					u_int32_t **addr;

					res.nscount = 0;
					for (addr = (u_int32_t**)hp->h_addr_list;
					     *addr && (res.nscount < MAXNS);
					     addr++) {
						res.nsaddr_list[
							res.nscount++
						].sin_addr.s_addr = **addr;
					}

					srvmsg = strcat(srvbuf,srv);
					strcat(srvbuf, "  ");
					strcat(srvmsg,
					       inet_ntoa(res.nsaddr.sin_addr));
				}
			}
			printf("; (%d server%s found)\n",
			       res.nscount, (res.nscount==1)?"":"s");
			res.id += res.retry;
		}

		{
			int i;

			for (i = 0;  i < res.nscount;  i++) {
				res.nsaddr_list[i].sin_family = AF_INET;
				res.nsaddr_list[i].sin_port = port;
			}
			res.id += res.retry;
		}

		if (ns_t_xfr_p(xfr)) {
			int i;

			for (i = 0; i < res.nscount; i++) {
				int x;

				if (keyfile)
					x = printZone(xfr, domain,
						      &res.nsaddr_list[i],
						      &key);
				else
					x = printZone(xfr, domain,
						      &res.nsaddr_list[i],
						      NULL);
				if (res.pfcode & RES_PRF_STATS) {
					exectime = time(NULL);
					printf(";; FROM: %s to SERVER: %s\n",
					       myhostname,
					       inet_ntoa(res.nsaddr_list[i]
							 .sin_addr));
					printf(";; WHEN: %s", ctime(&exectime));
				}
				if (!x)
					break;	/* success */
			}
			fflush(stdout);
			continue;
		}

		if (*domain && !qtypeSet) {
			queryType = T_A;
			qtypeSet++;
		}
		
		bytes_out = n = res_nmkquery(&res, QUERY, domain,
					     queryClass, queryType,
					     NULL, 0, NULL,
					     packet, sizeof packet);
		if (n < 0) {
			fflush(stderr);
			printf(";; res_nmkquery: buffer too small\n\n");
			continue;
		}
		if (queryType == T_IXFR) {
			HEADER *hp = (HEADER *) packet;
			u_char *cpp = packet + bytes_out;

			hp->nscount = htons(1+ntohs(hp->nscount));
			n = dn_comp(domain, cpp,
				    (sizeof packet) - (cpp - packet),
				    NULL, NULL);
			cpp += n;
			PUTSHORT(T_SOA, cpp); /* type */
			PUTSHORT(C_IN, cpp);  /* class */
			PUTLONG(0, cpp);      /* ttl */
			PUTSHORT(22, cpp);    /* dlen */
			*cpp++ = 0;           /* mname */
			*cpp++ = 0;           /* rname */
			PUTLONG(ixfr_serial, cpp);
			PUTLONG(0xDEAD, cpp); /* Refresh */
			PUTLONG(0xBEEF, cpp); /* Retry */
			PUTLONG(0xABCD, cpp); /* Expire */
			PUTLONG(0x1776, cpp); /* Min TTL */
			bytes_out = n = cpp - packet;
		};	

		eecode = 0;
		if (res.pfcode & RES_PRF_HEAD1)
			fp_resstat(&res, stdout);
		(void) gettimeofday(&start_time, NULL);
		assert(start_time.tv_usec >= 0 && start_time.tv_usec < 1000000);
		if (keyfile)
			n = res_nsendsigned(&res, packet, n, &key, answer, sizeof answer);
		else
			n = res_nsend(&res, packet, n, answer, sizeof answer);
		if ((bytes_in = n) < 0) {
			fflush(stdout);
			n = 0 - n;
			msg[0]=0;
			if (keyfile)
				strcat(msg,";; res_nsendsigned to server ");
			else
				strcat(msg,";; res_nsend to server ");
			strcat(msg,srvmsg);
			perror(msg);
			fflush(stderr);

			if (!dofile) {
				if (eecode)
					exit(eecode);
				else
					exit(9);
			}
		}
		(void) gettimeofday(&end_time, NULL);
		assert(end_time.tv_usec >= 0 && end_time.tv_usec < 1000000);

		if (res.pfcode & RES_PRF_STATS) {
			query_time = difftv(start_time, end_time);
			printf(";; Total query time: ");
			prnttime(query_time);
			putchar('\n');
			exectime = time(NULL);
			printf(";; FROM: %s to SERVER: %s\n",
			       myhostname, srvmsg);
			printf(";; WHEN: %s", ctime(&exectime));
			printf(";; MSG SIZE  sent: %d  rcvd: %d\n",
			       bytes_out, bytes_in);
		}
	  
		fflush(stdout);
/*
 *   Argh ... not particularly elegant. Should put in *real* ping code.
 *   Would necessitate root priviledges for icmp port though!
 */
		if (*pingstr) {
			sprintf(doping,"%s %s 56 3 | tail -3",pingstr,
				(srv==NULL)?(defsrv+10):srv);
			system(doping);
		}
		putchar('\n');

/*
 * Fairly crude method and low overhead method of keeping two
 * batches started at different sites somewhat synchronized.
 */
		gettimeofday(&tv2, NULL);
		assert(tv2.tv_usec >= 0 && tv2.tv_usec < 1000000);
		delay = (int)(tv2.tv_sec - tv1.tv_sec);
		if (delay < wait) {
			sleep(wait - delay);
		}
		tv1 = tv2;
	}
	return (eecode);
}

/* Private. */

static void
Usage() {
	fputs("\
usage:  dig [@server] [domain] [q-type] [q-class] {q-opt} {d-opt} [%comment]\n\
where:	server,\n\
	domain	are names in the Domain Name System\n\
	q-class	is one of (in,any,...) [default: in]\n\
	q-type	is one of (a,any,mx,ns,soa,hinfo,axfr,txt,...) [default: a]\n\
", stderr);
	fputs("\
	q-opt	is one of:\n\
		-x dot-notation-address	(shortcut to in-addr.arpa lookups)\n\
		-f file			(batch mode input file name)\n\
		-T time			(batch mode time delay, per query)\n\
		-p port			(nameserver is on this port) [53]\n\
		-b addr[:port]		(bind to this tcp address) [*]\n\
		-P[ping-string]		(see man page)\n\
		-t query-type		(synonym for q-type)\n\
		-c query-class		(synonym for q-class)\n\
		-k keydir:keyname	(sign the query with this TSIG key)\n\
		-envsav,-envset		(see man page)\n\
		-[no]stick		(see man page)\n\
", stderr);
	fputs("\
	d-opt	is of the form ``+keyword=value'' where keyword is one of:\n\
		[no]debug [no]d2 [no]recurse retry=# time=# [no]ko [no]vc\n\
		[no]defname [no]search domain=NAME [no]ignore [no]primary\n\
		[no]aaonly [no]cmd [no]stats [no]Header [no]header\n\
		[no]ttlid [no]cl [no]qr [no]reply [no]ques [no]answer\n\
		[no]author [no]addit pfdef pfmin pfset=# pfand=# pfor=#\n\
", stderr);
	fputs("\
notes:	defname and search don't work; use fully-qualified names.\n\
	this is DiG version " VSTRING "\n\
	$Id: dig.c,v 8.46 2001/04/01 17:35:01 vixie Exp $\n\
", stderr);
}

static int
setopt(const char *string) {
	char option[NAME_LEN], *ptr;
	int i;

	i = pickString(string, option, sizeof option);
	if (i == 0) {
		fprintf(stderr, ";*** Invalid option: %s\n",  string);

		/* this is ugly, but fixing the caller to behave
		   properly with an error return value would require a major
		   cleanup. */
		exit(9);
	} 
   
	if (strncmp(option, "aa", 2) == 0) {	/* aaonly */
		res.options |= RES_AAONLY;
	} else if (strncmp(option, "noaa", 4) == 0) {
		res.options &= ~RES_AAONLY;
	} else if (strncmp(option, "deb", 3) == 0) {	/* debug */
		res.options |= RES_DEBUG;
	} else if (strncmp(option, "nodeb", 5) == 0) {
		res.options &= ~(RES_DEBUG | RES_DEBUG2);
	} else if (strncmp(option, "ko", 2) == 0) {	/* keepopen */
		res.options |= (RES_STAYOPEN | RES_USEVC);
	} else if (strncmp(option, "noko", 4) == 0) {
		res.options &= ~RES_STAYOPEN;
	} else if (strncmp(option, "d2", 2) == 0) {	/* d2 (more debug) */
		res.options |= (RES_DEBUG | RES_DEBUG2);
	} else if (strncmp(option, "nod2", 4) == 0) {
		res.options &= ~RES_DEBUG2;
	} else if (strncmp(option, "def", 3) == 0) {	/* defname */
		res.options |= RES_DEFNAMES;
	} else if (strncmp(option, "nodef", 5) == 0) {
		res.options &= ~RES_DEFNAMES;
	} else if (strncmp(option, "sea", 3) == 0) {	/* search list */
		res.options |= RES_DNSRCH;
	} else if (strncmp(option, "nosea", 5) == 0) {
		res.options &= ~RES_DNSRCH;
	} else if (strncmp(option, "do", 2) == 0) {	/* domain */
		ptr = strchr(option, '=');
		if (ptr != NULL) {
			i = pickString(++ptr, res.defdname, sizeof res.defdname);
			if (i == 0) { /* value's too long or non-existant. This actually
					 shouldn't happen due to pickString()
					 above */
				fprintf(stderr, "*** Invalid domain: %s\n", ptr) ;
				exit(9); /* see comment at previous call to exit()*/
			}
		}
	} else if (strncmp(option, "ti", 2) == 0) {      /* timeout */
		ptr = strchr(option, '=');
		if (ptr != NULL)
			sscanf(++ptr, "%d", &res.retrans);
	} else if (strncmp(option, "ret", 3) == 0) {    /* retry */
		ptr = strchr(option, '=');
		if (ptr != NULL)
			sscanf(++ptr, "%d", &res.retry);
	} else if (strncmp(option, "i", 1) == 0) {	/* ignore */
		res.options |= RES_IGNTC;
	} else if (strncmp(option, "noi", 3) == 0) {
		res.options &= ~RES_IGNTC;
	} else if (strncmp(option, "pr", 2) == 0) {	/* primary */
		res.options |= RES_PRIMARY;
	} else if (strncmp(option, "nop", 3) == 0) {
		res.options &= ~RES_PRIMARY;
	} else if (strncmp(option, "rec", 3) == 0) {	/* recurse */
		res.options |= RES_RECURSE;
	} else if (strncmp(option, "norec", 5) == 0) {
		res.options &= ~RES_RECURSE;
	} else if (strncmp(option, "v", 1) == 0) {	/* vc */
		res.options |= RES_USEVC;
	} else if (strncmp(option, "nov", 3) == 0) {
		res.options &= ~RES_USEVC;
	} else if (strncmp(option, "pfset", 5) == 0) {
		ptr = strchr(option, '=');
		if (ptr != NULL)
			res.pfcode = xstrtonum(++ptr);
	} else if (strncmp(option, "pfand", 5) == 0) {
		ptr = strchr(option, '=');
		if (ptr != NULL)
			res.pfcode = res.pfcode & xstrtonum(++ptr);
	} else if (strncmp(option, "pfor", 4) == 0) {
		ptr = strchr(option, '=');
		if (ptr != NULL)
			res.pfcode |= xstrtonum(++ptr);
	} else if (strncmp(option, "pfmin", 5) == 0) {
		res.pfcode = PRF_MIN;
	} else if (strncmp(option, "pfdef", 5) == 0) {
		res.pfcode = PRF_DEF;
	} else if (strncmp(option, "an", 2) == 0) {  /* answer section */
		res.pfcode |= RES_PRF_ANS;
	} else if (strncmp(option, "noan", 4) == 0) {
		res.pfcode &= ~RES_PRF_ANS;
	} else if (strncmp(option, "qu", 2) == 0) {  /* question section */
		res.pfcode |= RES_PRF_QUES;
	} else if (strncmp(option, "noqu", 4) == 0) {  
		res.pfcode &= ~RES_PRF_QUES;
	} else if (strncmp(option, "au", 2) == 0) {  /* authority section */
		res.pfcode |= RES_PRF_AUTH;
	} else if (strncmp(option, "noau", 4) == 0) {  
		res.pfcode &= ~RES_PRF_AUTH;
	} else if (strncmp(option, "ad", 2) == 0) {  /* addition section */
		res.pfcode |= RES_PRF_ADD;
	} else if (strncmp(option, "noad", 4) == 0) {  
		res.pfcode &= ~RES_PRF_ADD;
	} else if (strncmp(option, "tt", 2) == 0) {  /* TTL & ID */
		res.pfcode |= RES_PRF_TTLID;
	} else if (strncmp(option, "nott", 4) == 0) {  
		res.pfcode &= ~RES_PRF_TTLID;
	} else if (strncmp(option, "he", 2) == 0) {  /* head flags stats */
		res.pfcode |= RES_PRF_HEAD2;
	} else if (strncmp(option, "nohe", 4) == 0) {  
		res.pfcode &= ~RES_PRF_HEAD2;
	} else if (strncmp(option, "H", 1) == 0) {  /* header all */
		res.pfcode |= RES_PRF_HEADX;
	} else if (strncmp(option, "noH", 3) == 0) {  
		res.pfcode &= ~(RES_PRF_HEADX);
	} else if (strncmp(option, "qr", 2) == 0) {  /* query */
		res.pfcode |= RES_PRF_QUERY;
	} else if (strncmp(option, "noqr", 4) == 0) {  
		res.pfcode &= ~RES_PRF_QUERY;
	} else if (strncmp(option, "rep", 3) == 0) {  /* reply */
		res.pfcode |= RES_PRF_REPLY;
	} else if (strncmp(option, "norep", 5) == 0) {  
		res.pfcode &= ~RES_PRF_REPLY;
	} else if (strncmp(option, "cm", 2) == 0) {  /* command line */
		res.pfcode |= RES_PRF_CMD;
	} else if (strncmp(option, "nocm", 4) == 0) {  
		res.pfcode &= ~RES_PRF_CMD;
	} else if (strncmp(option, "cl", 2) == 0) {  /* class mnemonic */
		res.pfcode |= RES_PRF_CLASS;
	} else if (strncmp(option, "nocl", 4) == 0) {  
		res.pfcode &= ~RES_PRF_CLASS;
	} else if (strncmp(option, "st", 2) == 0) {  /* stats*/
		res.pfcode |= RES_PRF_STATS;
	} else if (strncmp(option, "nost", 4) == 0) {  
		res.pfcode &= ~RES_PRF_STATS;
	} else {
		fprintf(stderr, "; *** Invalid option: %s\n",  option);
		return (ERROR);
	}
	res_re_init();
	return (SUCCESS);
}

/*
 * Force a reinitialization when the domain is changed.
 */
static void
res_re_init() {
	static char localdomain[] = "LOCALDOMAIN";
	u_long pfcode = res.pfcode, options = res.options;
	unsigned ndots = res.ndots;
	int retrans = res.retrans, retry = res.retry;
	char *buf;

	/*
	 * This is ugly but putenv() is more portable than setenv().
	 */
	buf = malloc((sizeof localdomain) + strlen(res.defdname) +10/*fuzz*/);
	sprintf(buf, "%s=%s", localdomain, res.defdname);
	putenv(buf);	/* keeps the argument, so we won't free it */
	res_ninit(&res);
	res.pfcode = pfcode;
	res.options = options;
	res.ndots = ndots;
	res.retrans = retrans;
	res.retry = retry;
}

/*
 * convert char string (decimal, octal, or hex) to integer
 */
static int
xstrtonum(char *p) {
	int v = 0;
	int i;
	int b = 10;
	int flag = 0;
	while (*p != 0) {
		if (!flag++)
			if (*p == '0') {
				b = 8; p++;
				continue;
			}
		if (isupper(*p))
			*p = tolower(*p);
		if (*p == 'x') {
			b = 16; p++;
			continue;
		}
		if (isdigit(*p)) {
			i = *p - '0';
		} else if (isxdigit(*p)) {
			i = *p - 'a' + 10;
		} else {
			fprintf(stderr,
				"; *** Bad char in numeric string..ignored\n");
			i = -1;
		}
		if (i >= b) {
			fprintf(stderr,
				"; *** Bad char in numeric string..ignored\n");
			i = -1;
		}
		if (i >= 0)
			v = v * b + i;
		p++;
	}
	return (v);
}

typedef union {
	HEADER qb1;
	u_char qb2[PACKETSZ];
} querybuf;

static int
printZone(ns_type xfr, const char *zone, const struct sockaddr_in *sin,
	  ns_tsig_key *key)
{
	static u_char *answer = NULL;
	static int answerLen = 0;

	querybuf buf;
	int msglen, amtToRead, numRead, result = 0, sockFD, len;
	int count, type, class, rlen, done, n;
	int numAnswers = 0, numRecords = 0, soacnt = 0;
	u_char *cp, tmp[NS_INT16SZ];
	char dname[2][NS_MAXDNAME];
	enum { NO_ERRORS, ERR_READING_LEN, ERR_READING_MSG, ERR_PRINTING }
		error = NO_ERRORS;
	pid_t zpid;
	u_char *newmsg;
	int newmsglen;
	ns_tcp_tsig_state tsig_state;
	int tsig_ret, tsig_required, tsig_present;

	switch (xfr) {
	case ns_t_axfr:
	case ns_t_zxfr:
		break;
	default:
		fprintf(stderr, ";; %s - transfer type not supported\n",
			p_type(xfr));
		return (ERROR);
	}

	/*
	 *  Create a query packet for the requested zone name.
	 */
	msglen = res_nmkquery(&res, ns_o_query, zone,
			      queryClass, ns_t_axfr, NULL,
			      0, 0, buf.qb2, sizeof buf);
	if (msglen < 0) {
		if (res.options & RES_DEBUG)
			fprintf(stderr, ";; res_nmkquery failed\n");
		return (ERROR);
	}

	/*
	 * Sign the message if a key was sent
	 */
	if (key == NULL) {
		newmsg = (u_char *)&buf;
		newmsglen = msglen;
	} else {
		DST_KEY *dstkey;
		int bufsize, siglen;
		u_char sig[64];
		int ret;
		
		/* ns_sign() also calls dst_init(), but there is no harm
		 * doing it twice
		 */
		dst_init();
		
		bufsize = msglen + 1024;
		newmsg = (u_char *) malloc(bufsize);
		if (newmsg == NULL) {
			errno = ENOMEM;
			return (-1);
		}
		memcpy(newmsg, (u_char *)&buf, msglen);
		newmsglen = msglen;
		
		if (strcmp(key->alg, NS_TSIG_ALG_HMAC_MD5) != 0)
			dstkey = NULL;
		else
			dstkey = dst_buffer_to_key(key->name, KEY_HMAC_MD5,
							NS_KEY_TYPE_AUTH_ONLY,
							NS_KEY_PROT_ANY,
							key->data, key->len);
		if (dstkey == NULL) {
			errno = EINVAL;
			if (key)
				free(newmsg);
			return (-1);
		}
		
		siglen = sizeof(sig);
/* newmsglen++; */
		ret = ns_sign(newmsg, &newmsglen, bufsize, NOERROR, dstkey, NULL, 0,
		      sig, &siglen, 0);
		if (ret < 0) {
			if (key)
				free (newmsg);
			if (ret == NS_TSIG_ERROR_NO_SPACE)
				errno  = EMSGSIZE;
			else if (ret == -1)
				errno  = EINVAL;
			return (ret);
		}
		ns_verify_tcp_init(dstkey, sig, siglen, &tsig_state);
	}

	/*
	 *  Set up a virtual circuit to the server.
	 */
	if ((sockFD = socket(sin->sin_family, SOCK_STREAM, 0)) < 0) {
		int e = errno;

		perror(";; socket");
		return (e);
	}
	if (bind(sockFD, (struct sockaddr *)&myaddress, sizeof myaddress) < 0){
		int e = errno;

		fprintf(stderr, ";; bind(%s:%u): %s\n",
			inet_ntoa(myaddress.sin_addr),
			ntohs(myaddress.sin_port),
			strerror(e));
		(void) close(sockFD);
		sockFD = -1;
		return (e);
	}
	if (connect(sockFD, (struct sockaddr *)sin, sizeof *sin) < 0) {
		int e = errno;

		perror(";; connect");
		(void) close(sockFD);
		sockFD = -1;
		return (e);
	}

	/*
	 * Send length & message for zone transfer
	 */

	ns_put16(newmsglen, tmp);
        if (write(sockFD, (char *)tmp, NS_INT16SZ) != NS_INT16SZ ||
            write(sockFD, (char *)newmsg, newmsglen) != newmsglen) {
		int e = errno;
		if (key)
			free (newmsg);
		perror(";; write");
		(void) close(sockFD);
		sockFD = -1;
		return (e);
	}

	/*
	 * If we're compressing, push a gzip into the pipeline.
	 */
	if (xfr == ns_t_zxfr) {
		enum { rd = 0, wr = 1 };
		int z[2];

		if (pipe(z) < 0) {
			int e = errno;
			if (key)
				free (newmsg);

			perror(";; pipe");
			(void) close(sockFD);
			sockFD = -1;
			return (e);
		}
		zpid = vfork();
		if (zpid < 0) {
			int e = errno;
			if (key)
				free (newmsg);

			perror(";; fork");
			(void) close(sockFD);
			sockFD = -1;
			return (e);
		} else if (zpid == 0) {
			/* Child. */
			(void) close(z[rd]);
			(void) dup2(sockFD, STDIN_FILENO);
			(void) close(sockFD);
			(void) dup2(z[wr], STDOUT_FILENO);
			(void) close(z[wr]);
			execlp("gzip", "gzip", "-d", "-v", NULL);
			perror(";; child: execlp(gunzip)");
			_exit(1);
		}
		/* Parent. */
		(void) close(z[wr]);
		(void) dup2(z[rd], sockFD);
		(void) close(z[rd]);
	}

	dname[0][0] = '\0';
	for (done = 0; !done; (void)NULL) {
		/*
		 * Read the length of the response.
		 */

		cp = tmp;
		amtToRead = INT16SZ;
		while (amtToRead > 0 &&
		   (numRead = read(sockFD, cp, amtToRead)) > 0) {
			cp += numRead;
			amtToRead -= numRead;
		}
		if (numRead <= 0) {
			error = ERR_READING_LEN;
			break;
		}

		len = ns_get16(tmp);
		if (len == 0)
			break;	/* nothing left to read */

		/*
		 * The server sent too much data to fit the existing buffer --
		 * allocate a new one.
		 */
		if (len > answerLen) {
			if (answerLen != 0)
				free(answer);
			answerLen = len;
			answer = (u_char *)Malloc(answerLen);
		}

		/*
		 * Read the response.
		 */

		amtToRead = len;
		cp = answer;
		while (amtToRead > 0 &&
		       (numRead = read(sockFD, cp, amtToRead)) > 0) {
			cp += numRead;
			amtToRead -= numRead;
		}
		if (numRead <= 0) {
			error = ERR_READING_MSG;
			break;
		}

		result = print_axfr(stdout, answer, len);
		if (result != 0) {
			error = ERR_PRINTING;
			break;
		}
		numRecords += htons(((HEADER *)answer)->ancount);
		numAnswers++;

		/* Header. */
		cp = answer + HFIXEDSZ;
		/* Question. */
		for (count = ntohs(((HEADER *)answer)->qdcount);	
		     count > 0;
		     count--) {
			n = dn_skipname(cp, answer + len);
			if (n < 0) {
				error = ERR_PRINTING;
				done++;
				break;
			}
			cp += n + QFIXEDSZ;
			if (cp > answer + len) {
				error = ERR_PRINTING;
				done++;
				break;
			}
		}
		/* Answer. */
		for (count = ntohs(((HEADER *)answer)->ancount);
		     count > 0 && !done;
		     count--) {
			n = dn_expand(answer, answer + len, cp,
				      dname[soacnt], sizeof dname[0]);
			if (n < 0) {
				error = ERR_PRINTING;
				done++;
				break;
			}
			cp += n;
			if (cp + 3 * INT16SZ + INT32SZ > answer + len) {
				error = ERR_PRINTING;
				done++;
				break;
			}
			GETSHORT(type, cp);
			GETSHORT(class, cp);
			cp += INT32SZ;	/* ttl */
			GETSHORT(rlen, cp);
			cp += rlen;
			if (cp > answer + len) {
				error = ERR_PRINTING;
				done++;
				break;
			}
			if (type == T_SOA && soacnt++ &&
			    ns_samename(dname[0], dname[1]) == 1) {
				done++;
				break;
			}
		}

		/*
		 * Verify the TSIG
		 */

		if (key) {
			if (ns_find_tsig(answer, answer + len) != NULL)
				tsig_present = 1;
			else
				tsig_present = 0;
			if (numAnswers == 1 || soacnt > 1)
				tsig_required = 1;
			else
				tsig_required = 0;
			tsig_ret = ns_verify_tcp(answer, &len, &tsig_state,
						 tsig_required);
			if (tsig_ret == 0) {
				if (tsig_present)
					printf("; TSIG ok\n");
			}
			else
				printf("; TSIG invalid\n");
		}

	}

	printf(";; Received %d answer%s (%d record%s).\n",
	       numAnswers, (numAnswers != 1) ? "s" : "",
	       numRecords, (numRecords != 1) ? "s" : "");

	(void) close(sockFD);
	sockFD = -1;

	/*
	 * If we were uncompressing, reap the uncompressor.
	 */
	if (xfr == ns_t_zxfr) {
		pid_t pid;
		int status;

		pid = wait(&status);
		if (pid < 0) {
			int e = errno;

			perror(";; wait");
			return (e);
		}
		if (pid != zpid) {
			fprintf(stderr, ";; wrong pid (%lu != %lu)\n",
				(u_long)pid, (u_long)zpid);
			return (ERROR);
		}
		printf(";; pid %lu: exit %d, signal %d, core %c\n",
		       (u_long)pid, WEXITSTATUS(status),
		       WIFSIGNALED(status) ? WTERMSIG(status) : 0,
		       WCOREDUMP(status) ? 't' : 'f');
	}

	/* XXX This should probably happen sooner than here */
	if (key)
		free (newmsg);

	switch (error) {
	case NO_ERRORS:
		return (0);

	case ERR_READING_LEN:
		return (EMSGSIZE);

	case ERR_PRINTING:
		return (result);

	case ERR_READING_MSG:
		return (EMSGSIZE);

	default:
		return (EFAULT);
	}
}

static int
print_axfr(FILE *file, const u_char *msg, size_t msglen) {
	ns_msg handle;

	if (ns_initparse(msg, msglen, &handle) < 0) {
		fprintf(file, ";; ns_initparse: %s\n", strerror(errno));
		return (ns_r_formerr);
	}
	if (ns_msg_getflag(handle, ns_f_rcode) != ns_r_noerror)
		return (ns_msg_getflag(handle, ns_f_rcode));

	/*
	 * We are looking for info from answer resource records.
	 * If there aren't any, return with an error. We assume
	 * there aren't any question records.
	 */
	if (ns_msg_count(handle, ns_s_an) == 0)
		return (NO_INFO);

#ifdef PROTOCOLDEBUG
	printf(";;; (message of %d octets has %d answers)\n",
	       msglen, ns_msg_count(handle, ns_s_an));
#endif
	for (;;) {
		static char origin[NS_MAXDNAME], name_ctx[NS_MAXDNAME];
		const char *name;
		char buf[2048];		/* XXX need to malloc/realloc. */
		ns_rr rr;

		if (ns_parserr(&handle, ns_s_an, -1, &rr)) {
			if (errno != ENODEV) {
				fprintf(file, ";; ns_parserr: %s\n",
					strerror(errno));
				return (FORMERR);
			}
			break;
		}
		name = ns_rr_name(rr);
		if (origin[0] == '\0' && name[0] != '\0') {
			if (strcmp(name, ".") != 0)
				strcpy(origin, name);
			fprintf(file, "$ORIGIN %s.\n", origin);
			if (strcmp(name, ".") == 0)
				strcpy(origin, name);
			strcpy(name_ctx, "@");
		}
		if (ns_sprintrr(&handle, &rr, name_ctx, origin,
				buf, sizeof buf) < 0) {
			fprintf(file, ";; ns_sprintrr: %s\n", strerror(errno));
			return (FORMERR);
		}
		strcpy(name_ctx, name);
		fputs(buf, file);
		fputc('\n', file);
	}
	return (SUCCESS);
}

static struct timeval
difftv(struct timeval a, struct timeval b) {
	static struct timeval diff;

	diff.tv_sec = b.tv_sec - a.tv_sec;
	if ((diff.tv_usec = b.tv_usec - a.tv_usec) < 0) {
		diff.tv_sec--;
		diff.tv_usec += 1000000;
	}
	return (diff);
}

static void
prnttime(struct timeval t) {
	printf("%lu msec", (u_long)(t.tv_sec * 1000 + (t.tv_usec / 1000)));
}

/*
 * Take arguments appearing in simple string (from file or command line)
 * place in char**.
 */
static void
stackarg(char *l, char **y) {
	int done = 0;

	while (!done) {
		switch (*l) {
		case '\t':
		case ' ':
			l++;
			break;
		case '\0':
		case '\n':
			done++;
			*y = NULL;
			break;
		default:
			*y++ = l;
			while (!isspace(*l))
				l++;
			if (*l == '\n')
				done++;
			*l++ = '\0';
			*y = NULL;
		}
	}
}
