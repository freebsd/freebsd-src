#ifndef lint
static char rcsid[] = "$Id: dig.c,v 8.19 1998/03/19 19:30:18 halley Exp $";
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
 * Copyright (c) 1996 by Internet Software Consortium
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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

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

extern char *_res_resultcodes[];	/* res_debug.c */

/* Global. */

#define VERSION 81
#define VSTRING "8.1"

#define PRF_DEF		0x2ff9
#define PRF_MIN		0xA930
#define PRF_ZONE        0x24f9

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

int eecode = 0;

FILE *qfp;
int sockFD;

#define SAVEENV "DiG.env"
#define DIG_MAXARGS 30

static char		*defsrv, *srvmsg;
static char		defbuf[40] = "default -- ";
static char		srvbuf[60];
static char		myhostname[MAXHOSTNAMELEN];

/* stuff for nslookup modules */
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
static int		SetOption(const char *);
static void		res_re_init(void);
static int		xstrtonum(char *);
static int		printZone(const char *, const struct sockaddr_in *);
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
#define	packet  (packet_.packet_)
	u_char answer[8*1024];
	int n;
	char doping[90];
	char pingstr[50];
	char *afile;
	char *addrc, *addrend, *addrbegin;

	struct timeval exectime, tv1, tv2, start_time, end_time, query_time;

	char *srv;
	int anyflag = 0;
	int sticky = 0;
	int tmp; 
	int qtypeSet;
	int addrflag = 0;
	int zone = 0;
        int bytes_out, bytes_in;

	char cmd[256];
	char domain[MAXDNAME];
        char msg[120], *msgptr;
	char **vtmp;
	char *args[DIG_MAXARGS];
	char **ax;
	int once = 1, dofile = 0; /* batch -vs- interactive control */
	char fileq[100];
	int  fp;
	int wait=0, delay;
	int envset=0, envsave=0;
	struct __res_state res_x, res_t;
	char *pp;

	res_init();
	_res.pfcode = PRF_DEF;
	qtypeSet = 0;
	memset(domain, 0, sizeof domain);
	gethostname(myhostname, (sizeof myhostname));
	defsrv = strcat(defbuf, inet_ntoa(_res.nsaddr.sin_addr));
	res_x = _res;

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
		_res = res_x;
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

	_res.id = 1;
	gettimeofday(&tv1, NULL);

/*
 * Main section: once if cmd-line query
 *               while !EOF if batch mode
 */
	*fileq = '\0';
	while ((dofile && fgets(fileq, sizeof fileq, qfp) != NULL) || 
	       (!dofile && once--)) 
	{
		if (*fileq == '\n' || *fileq == '#' || *fileq==';')
			continue;	/* ignore blank lines & comments */

/*
 * "Sticky" requests that before current parsing args
 * return to current "working" environment (X******).
 */
		if (sticky) {
			printf(";; (using sticky settings)\n");
			_res = res_x;
		}

/*
 * Concat cmd-line and file args.
 */
		stackarg(fileq, ax);

		/* defaults */
		queryType = ns_t_ns;
		queryClass = ns_c_in;
		zone = 0;
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
				SetOption(*argv+1);
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
					wait = atoi(*++argv);
					break;
				case 'c': 
					if ((tmp = atoi(*++argv))
					    || *argv[0]=='0') {
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
					if ((tmp = atoi(*++argv))
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
					if (!(addrc = *++argv)) {
						printf(
						       "; no arg for -x?\n"
						       );
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
				case 'p': port = htons(atoi(*++argv)); break;
				case 'P':
					if (argv[0][2] != '\0')
						strcpy(pingstr,&argv[0][2]);
					else
						strcpy(pingstr,"ping -s");
					break;
				case 'n':
					_res.ndots = atoi(&argv[0][2]);
					break;
				} /* switch - */
				continue;
			} /* if '-'   */

			if ((tmp = StringToType(*argv, -1, NULL)) != -1) { 
				if ((T_ANY == tmp) && anyflag++) {  
					queryClass = C_ANY; 	
					continue; 
				}
				if (T_AXFR == tmp) {
					_res.pfcode = PRF_ZONE;
					zone++;
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

		if (_res.pfcode & 0x80000)
			printf("; pfcode: %08lx, options: %08lx\n",
			       _res.pfcode, _res.options);
	  
/*
 * Current env. (after this parse) is to become the
 * new "working" environmnet. Used in conj. with sticky.
 */
		if (envset) {
			res_x = _res;
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
				write(fp, (char *)&_res, (sizeof _res));
				close(fp);
			}
			envsave = 0;
		}

		if (_res.pfcode & RES_PRF_CMD)
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
				_res.nscount = 1;
				_res.nsaddr.sin_addr = addr;
				srvmsg = strcat(srvbuf, srv);
			} else {
				res_t = _res;
				_res.pfcode = 0;
				_res.options = RES_DEFAULT;
				res_init();
				hp = gethostbyname(srv);
				_res = res_t;
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

					_res.nscount = 0;
					for (addr = (u_int32_t**)hp->h_addr_list;
					     *addr && (_res.nscount < MAXNS);
					     addr++) {
						_res.nsaddr_list[
							_res.nscount++
						].sin_addr.s_addr = **addr;
					}

					srvmsg = strcat(srvbuf,srv);
					strcat(srvbuf, "  ");
					strcat(srvmsg,
					       inet_ntoa(_res.nsaddr.sin_addr)
					       );
				}
			}
			printf("; (%d server%s found)\n",
			       _res.nscount, (_res.nscount==1)?"":"s");
			_res.id += _res.retry;
		}

		{
			int i;

			for (i = 0;  i < _res.nscount;  i++) {
				_res.nsaddr_list[i].sin_family = AF_INET;
				_res.nsaddr_list[i].sin_port = port;
			}
			_res.id += _res.retry;
		}

		if (zone) {
			int i;

			for (i = 0;  i < _res.nscount;  i++) {
				int x = printZone(domain,
						  &_res.nsaddr_list[i]);
				if (_res.pfcode & RES_PRF_STATS) {
					struct timeval exectime;
					time_t t;

					printf(";; FROM: %s to SERVER: %s\n",
					       myhostname,
					       inet_ntoa(_res.nsaddr_list[i]
							 .sin_addr));
					gettimeofday(&exectime, NULL);
					t = (time_t)exectime.tv_sec;
					printf(";; WHEN: %s", ctime(&t));
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
		
		bytes_out = n = res_mkquery(QUERY, domain,
					    queryClass, queryType,
					    NULL, 0, NULL,
					    packet, sizeof packet);
		if (n < 0) {
			fflush(stderr);
			printf(";; res_mkquery: buffer too small\n\n");
			continue;
		}
		eecode = 0;
		if (_res.pfcode & RES_PRF_HEAD1)
			__fp_resstat(NULL, stdout);
		(void) gettimeofday(&start_time, NULL);
		if ((bytes_in = n = res_send(packet, n,
					     answer, sizeof answer)) < 0) {
			fflush(stdout);
			n = 0 - n;
			msg[0]=0;
			strcat(msg,";; res_send to server ");
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

		if (_res.pfcode & RES_PRF_STATS) {
			time_t t;

			query_time = difftv(start_time, end_time);
			printf(";; Total query time: ");
			prnttime(query_time);
			putchar('\n');
			printf(";; FROM: %s to SERVER: %s\n",
			       myhostname, srvmsg);
			gettimeofday(&exectime,NULL);
			t = (time_t)exectime.tv_sec;
			printf(";; WHEN: %s", ctime(&t));
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
		delay = (int)(tv2.tv_sec - tv1.tv_sec);
		if (delay < wait) {
			sleep(wait - delay);
		}
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
		-Pping-string		(see man page)\n\
		-t query-type		(synonym for q-type)\n\
		-c query-class		(synonym for q-class)\n\
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
", stderr);
}

static int
SetOption(const char *string) {
	char option[NAME_LEN], type[NAME_LEN], *ptr;
	int i;

	i = sscanf(string, " %s", option);
	if (i != 1) {
		fprintf(stderr, ";*** Invalid option: %s\n",  option);
		return (ERROR);
	} 
   
	if (strncmp(option, "aa", 2) == 0) {	/* aaonly */
		_res.options |= RES_AAONLY;
	} else if (strncmp(option, "noaa", 4) == 0) {
		_res.options &= ~RES_AAONLY;
	} else if (strncmp(option, "deb", 3) == 0) {	/* debug */
		_res.options |= RES_DEBUG;
	} else if (strncmp(option, "nodeb", 5) == 0) {
		_res.options &= ~(RES_DEBUG | RES_DEBUG2);
	} else if (strncmp(option, "ko", 2) == 0) {	/* keepopen */
		_res.options |= (RES_STAYOPEN | RES_USEVC);
	} else if (strncmp(option, "noko", 4) == 0) {
		_res.options &= ~RES_STAYOPEN;
	} else if (strncmp(option, "d2", 2) == 0) {	/* d2 (more debug) */
		_res.options |= (RES_DEBUG | RES_DEBUG2);
	} else if (strncmp(option, "nod2", 4) == 0) {
		_res.options &= ~RES_DEBUG2;
	} else if (strncmp(option, "def", 3) == 0) {	/* defname */
		_res.options |= RES_DEFNAMES;
	} else if (strncmp(option, "nodef", 5) == 0) {
		_res.options &= ~RES_DEFNAMES;
	} else if (strncmp(option, "sea", 3) == 0) {	/* search list */
		_res.options |= RES_DNSRCH;
	} else if (strncmp(option, "nosea", 5) == 0) {
		_res.options &= ~RES_DNSRCH;
	} else if (strncmp(option, "do", 2) == 0) {	/* domain */
		ptr = strchr(option, '=');
		if (ptr != NULL)
			sscanf(++ptr, "%s", _res.defdname);
	} else if (strncmp(option, "ti", 2) == 0) {      /* timeout */
		ptr = strchr(option, '=');
		if (ptr != NULL)
			sscanf(++ptr, "%d", &_res.retrans);
	} else if (strncmp(option, "ret", 3) == 0) {    /* retry */
		ptr = strchr(option, '=');
		if (ptr != NULL)
			sscanf(++ptr, "%d", &_res.retry);
	} else if (strncmp(option, "i", 1) == 0) {	/* ignore */
		_res.options |= RES_IGNTC;
	} else if (strncmp(option, "noi", 3) == 0) {
		_res.options &= ~RES_IGNTC;
	} else if (strncmp(option, "pr", 2) == 0) {	/* primary */
		_res.options |= RES_PRIMARY;
	} else if (strncmp(option, "nop", 3) == 0) {
		_res.options &= ~RES_PRIMARY;
	} else if (strncmp(option, "rec", 3) == 0) {	/* recurse */
		_res.options |= RES_RECURSE;
	} else if (strncmp(option, "norec", 5) == 0) {
		_res.options &= ~RES_RECURSE;
	} else if (strncmp(option, "v", 1) == 0) {	/* vc */
		_res.options |= RES_USEVC;
	} else if (strncmp(option, "nov", 3) == 0) {
		_res.options &= ~RES_USEVC;
	} else if (strncmp(option, "pfset", 5) == 0) {
		ptr = strchr(option, '=');
		if (ptr != NULL)
			_res.pfcode = xstrtonum(++ptr);
	} else if (strncmp(option, "pfand", 5) == 0) {
		ptr = strchr(option, '=');
		if (ptr != NULL)
			_res.pfcode = _res.pfcode & xstrtonum(++ptr);
	} else if (strncmp(option, "pfor", 4) == 0) {
		ptr = strchr(option, '=');
		if (ptr != NULL)
			_res.pfcode |= xstrtonum(++ptr);
	} else if (strncmp(option, "pfmin", 5) == 0) {
		_res.pfcode = PRF_MIN;
	} else if (strncmp(option, "pfdef", 5) == 0) {
		_res.pfcode = PRF_DEF;
	} else if (strncmp(option, "an", 2) == 0) {  /* answer section */
		_res.pfcode |= RES_PRF_ANS;
	} else if (strncmp(option, "noan", 4) == 0) {
		_res.pfcode &= ~RES_PRF_ANS;
	} else if (strncmp(option, "qu", 2) == 0) {  /* question section */
		_res.pfcode |= RES_PRF_QUES;
	} else if (strncmp(option, "noqu", 4) == 0) {  
		_res.pfcode &= ~RES_PRF_QUES;
	} else if (strncmp(option, "au", 2) == 0) {  /* authority section */
		_res.pfcode |= RES_PRF_AUTH;
	} else if (strncmp(option, "noau", 4) == 0) {  
		_res.pfcode &= ~RES_PRF_AUTH;
	} else if (strncmp(option, "ad", 2) == 0) {  /* addition section */
		_res.pfcode |= RES_PRF_ADD;
	} else if (strncmp(option, "noad", 4) == 0) {  
		_res.pfcode &= ~RES_PRF_ADD;
	} else if (strncmp(option, "tt", 2) == 0) {  /* TTL & ID */
		_res.pfcode |= RES_PRF_TTLID;
	} else if (strncmp(option, "nott", 4) == 0) {  
		_res.pfcode &= ~RES_PRF_TTLID;
	} else if (strncmp(option, "he", 2) == 0) {  /* head flags stats */
		_res.pfcode |= RES_PRF_HEAD2;
	} else if (strncmp(option, "nohe", 4) == 0) {  
		_res.pfcode &= ~RES_PRF_HEAD2;
	} else if (strncmp(option, "H", 1) == 0) {  /* header all */
		_res.pfcode |= RES_PRF_HEADX;
	} else if (strncmp(option, "noH", 3) == 0) {  
		_res.pfcode &= ~(RES_PRF_HEADX);
	} else if (strncmp(option, "qr", 2) == 0) {  /* query */
		_res.pfcode |= RES_PRF_QUERY;
	} else if (strncmp(option, "noqr", 4) == 0) {  
		_res.pfcode &= ~RES_PRF_QUERY;
	} else if (strncmp(option, "rep", 3) == 0) {  /* reply */
		_res.pfcode |= RES_PRF_REPLY;
	} else if (strncmp(option, "norep", 5) == 0) {  
		_res.pfcode &= ~RES_PRF_REPLY;
	} else if (strncmp(option, "cm", 2) == 0) {  /* command line */
		_res.pfcode |= RES_PRF_CMD;
	} else if (strncmp(option, "nocm", 4) == 0) {  
		_res.pfcode &= ~RES_PRF_CMD;
	} else if (strncmp(option, "cl", 2) == 0) {  /* class mnemonic */
		_res.pfcode |= RES_PRF_CLASS;
	} else if (strncmp(option, "nocl", 4) == 0) {  
		_res.pfcode &= ~RES_PRF_CLASS;
	} else if (strncmp(option, "st", 2) == 0) {  /* stats*/
		_res.pfcode |= RES_PRF_STATS;
	} else if (strncmp(option, "nost", 4) == 0) {  
		_res.pfcode &= ~RES_PRF_STATS;
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
res_re_init()
{
	static char localdomain[] = "LOCALDOMAIN";
	long pfcode = _res.pfcode;
	long ndots = _res.ndots;
	char *buf;

	/*
	 * This is ugly but putenv() is more portable than setenv().
	 */
	buf = malloc((sizeof localdomain) + strlen(_res.defdname) +10/*fuzz*/);
	sprintf(buf, "%s=%s", localdomain, _res.defdname);
	putenv(buf);	/* keeps the argument, so we won't free it */
	res_init();
	_res.pfcode = pfcode;
	_res.ndots = ndots;
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
printZone(const char *zone, const struct sockaddr_in *sin) {
	static u_char *answer = NULL;
	static int answerLen = 0;

	querybuf buf;
	HEADER *headerPtr;
	int msglen, amtToRead, numRead, result = 0, sockFD, len;
	int count, type, class, rlen, done, n;
	int numAnswers = 0, numRecords = 0, soacnt = 0;
	u_char *cp, tmp[NS_INT16SZ];
	char dname[2][NS_MAXDNAME], file[NAME_LEN];
	enum { NO_ERRORS, ERR_READING_LEN, ERR_READING_MSG, ERR_PRINTING }
		error = NO_ERRORS;

	/*
	 *  Create a query packet for the requested zone name.
	 */
	msglen = res_mkquery(ns_o_query, zone, queryClass, ns_t_axfr, NULL,
			     0, 0, buf.qb2, sizeof buf);
	if (msglen < 0) {
		if (_res.options & RES_DEBUG)
			fprintf(stderr, ";; res_mkquery failed\n");
		return (ERROR);
	}

	/*
	 *  Set up a virtual circuit to the server.
	 */
	if ((sockFD = socket(sin->sin_family, SOCK_STREAM, 0)) < 0) {
		int e = errno;

		perror(";; socket");
		return (e);
	}
	if (connect(sockFD, (struct sockaddr *)sin, sizeof *sin) < 0) {
		int e = errno;

		perror(";; connect");
		(void) close(sockFD);
		sockFD = -1;
		return e;
	}

	/*
	 * Send length & message for zone transfer
	 */

	ns_put16(msglen, tmp);
        if (write(sockFD, (char *)tmp, NS_INT16SZ) != NS_INT16SZ ||
            write(sockFD, (char *)&buf, msglen) != msglen) {
		int e = errno;
		perror(";; write");
		(void) close(sockFD);
		sockFD = -1;
		return (e);
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

		result = print_axfr(stdout, answer, cp - answer);
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
			    !strcasecmp(dname[0], dname[1])) {
				done++;
				break;
			}
		}
	}

	printf(";; Received %d answer%s (%d record%s).\n",
	       numAnswers, (numAnswers != 1) ? "s" : "",
	       numRecords, (numRecords != 1) ? "s" : "");

	(void) close(sockFD);
	sockFD = -1;

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
			fprintf(file, "$ORIGIN %s.\n", name);
			strcpy(origin, name);
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
