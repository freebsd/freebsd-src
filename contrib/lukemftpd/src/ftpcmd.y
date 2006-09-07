/*	$NetBSD: ftpcmd.y,v 1.84 2006/02/01 14:20:12 christos Exp $	*/

/*-
 * Copyright (c) 1997-2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1985, 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)ftpcmd.y	8.3 (Berkeley) 4/6/94
 */

/*
 * Grammar for FTP commands.
 * See RFC 959.
 */

%{
#include <sys/cdefs.h>

#ifndef lint
#if 0
static char sccsid[] = "@(#)ftpcmd.y	8.3 (Berkeley) 4/6/94";
#else
__RCSID("$NetBSD: ftpcmd.y,v 1.84 2006/02/01 14:20:12 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/ftp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <netdb.h>

#ifdef KERBEROS5
#include <krb5/krb5.h>
#endif

#include "extern.h"
#include "version.h"

static	int cmd_type;
static	int cmd_form;
static	int cmd_bytesz;

char	cbuf[FTP_BUFLEN];
char	*cmdp;
char	*fromname;

extern int	epsvall;
struct tab	sitetab[];

static	int	check_write(const char *, int);
static	void	help(struct tab *, const char *);
static	void	port_check(const char *, int);
	int	yylex(void);

%}

%union {
	struct {
		LLT	ll;
		int	i;
	} u;
	char   *s;
}

%token
	A	B	C	E	F	I
	L	N	P	R	S	T

	SP	CRLF	COMMA	ALL

	USER	PASS	ACCT	CWD	CDUP	SMNT
	QUIT	REIN	PORT	PASV	TYPE	STRU
	MODE	RETR	STOR	STOU	APPE	ALLO
	REST	RNFR	RNTO	ABOR	DELE	RMD
	MKD	PWD	LIST	NLST	SITE	SYST
	STAT	HELP	NOOP

	AUTH	ADAT	PROT	PBSZ	CCC	MIC
	CONF	ENC

	FEAT	OPTS

	SIZE	MDTM	MLST	MLSD

	LPRT	LPSV	EPRT	EPSV

	MAIL	MLFL	MRCP	MRSQ	MSAM	MSND
	MSOM

	CHMOD	IDLE	RATEGET	RATEPUT	UMASK

	LEXERR

%token	<s> STRING
%token	<u> NUMBER

%type	<u.i> check_login octal_number byte_size
%type	<u.i> struct_code mode_code type_code form_code decimal_integer
%type	<s> pathstring pathname password username
%type	<s> mechanism_name base64data prot_code

%start	cmd_sel

%%

cmd_sel
	: cmd
		{
			REASSIGN(fromname, NULL);
			restart_point = (off_t) 0;
		}

	| rcmd

	;

cmd
						/* RFC 959 */
	: USER SP username CRLF
		{
			user($3);
			free($3);
		}

	| PASS SP password CRLF
		{
			pass($3);
			memset($3, 0, strlen($3));
			free($3);
		}

	| CWD check_login CRLF
		{
			if ($2)
				cwd(homedir);
		}

	| CWD check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				cwd($4);
			if ($4 != NULL)
				free($4);
		}

	| CDUP check_login CRLF
		{
			if ($2)
				cwd("..");
		}

	| QUIT CRLF
		{
			if (logged_in) {
				reply(-221, "%s", "");
				reply(0,
 "Data traffic for this session was " LLF " byte%s in " LLF " file%s.",
				    (LLT)total_data, PLURAL(total_data),
				    (LLT)total_files, PLURAL(total_files));
				reply(0,
 "Total traffic for this session was " LLF " byte%s in " LLF " transfer%s.",
				    (LLT)total_bytes, PLURAL(total_bytes),
				    (LLT)total_xfers, PLURAL(total_xfers));
			}
			reply(221,
			    "Thank you for using the FTP service on %s.",
			    hostname);
			if (logged_in && logging) {
				syslog(LOG_INFO,
		"Data traffic: " LLF " byte%s in " LLF " file%s",
				    (LLT)total_data, PLURAL(total_data),
				    (LLT)total_files, PLURAL(total_files));
				syslog(LOG_INFO,
		"Total traffic: " LLF " byte%s in " LLF " transfer%s",
				    (LLT)total_bytes, PLURAL(total_bytes),
				    (LLT)total_xfers, PLURAL(total_xfers));
			}

			dologout(0);
		}

	| PORT check_login SP host_port CRLF
		{
			if ($2)
				port_check("PORT", AF_INET);
		}

	| LPRT check_login SP host_long_port4 CRLF
		{
			if ($2)
				port_check("LPRT", AF_INET);
		}

	| LPRT check_login SP host_long_port6 CRLF
		{
#ifdef INET6
			if ($2)
				port_check("LPRT", AF_INET6);
#else
			reply(500, "IPv6 support not available.");
#endif
		}

	| EPRT check_login SP STRING CRLF
		{
			if ($2) {
				if (extended_port($4) == 0)
					port_check("EPRT", -1);
			}
			free($4);
		}

	| PASV check_login CRLF
		{
			if ($2) {
				if (CURCLASS_FLAGS_ISSET(passive))
					passive();
				else
					reply(500, "PASV mode not available.");
			}
		}

	| LPSV check_login CRLF
		{
			if ($2) {
				if (CURCLASS_FLAGS_ISSET(passive)) {
					if (epsvall)
						reply(501,
						    "LPSV disallowed after EPSV ALL");
					else
						long_passive("LPSV", PF_UNSPEC);
				} else
					reply(500, "LPSV mode not available.");
			}
		}

	| EPSV check_login SP NUMBER CRLF
		{
			if ($2) {
				if (CURCLASS_FLAGS_ISSET(passive))
					long_passive("EPSV",
					    epsvproto2af($4.i));
				else
					reply(500, "EPSV mode not available.");
			}
		}

	| EPSV check_login SP ALL CRLF
		{
			if ($2) {
				if (CURCLASS_FLAGS_ISSET(passive)) {
					reply(200,
					    "EPSV ALL command successful.");
					epsvall++;
				} else
					reply(500, "EPSV mode not available.");
			}
		}

	| EPSV check_login CRLF
		{
			if ($2) {
				if (CURCLASS_FLAGS_ISSET(passive))
					long_passive("EPSV", PF_UNSPEC);
				else
					reply(500, "EPSV mode not available.");
			}
		}

	| TYPE check_login SP type_code CRLF
		{
			if ($2) {

			switch (cmd_type) {

			case TYPE_A:
				if (cmd_form == FORM_N) {
					reply(200, "Type set to A.");
					type = cmd_type;
					form = cmd_form;
				} else
					reply(504, "Form must be N.");
				break;

			case TYPE_E:
				reply(504, "Type E not implemented.");
				break;

			case TYPE_I:
				reply(200, "Type set to I.");
				type = cmd_type;
				break;

			case TYPE_L:
#if NBBY == 8
				if (cmd_bytesz == 8) {
					reply(200,
					    "Type set to L (byte size 8).");
					type = cmd_type;
				} else
					reply(504, "Byte size must be 8.");
#else /* NBBY == 8 */
				UNIMPLEMENTED for NBBY != 8
#endif /* NBBY == 8 */
			}
			
			}
		}

	| STRU check_login SP struct_code CRLF
		{
			if ($2) {
				switch ($4) {

				case STRU_F:
					reply(200, "STRU F ok.");
					break;

				default:
					reply(504, "Unimplemented STRU type.");
				}
			}
		}

	| MODE check_login SP mode_code CRLF
		{
			if ($2) {
				switch ($4) {

				case MODE_S:
					reply(200, "MODE S ok.");
					break;

				default:
					reply(502, "Unimplemented MODE type.");
				}
			}
		}

	| RETR check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				retrieve(NULL, $4);
			if ($4 != NULL)
				free($4);
		}

	| STOR SP pathname CRLF
		{
			if (check_write($3, 1))
				store($3, "w", 0);
			if ($3 != NULL)
				free($3);
		}

	| STOU SP pathname CRLF
		{
			if (check_write($3, 1))
				store($3, "w", 1);
			if ($3 != NULL)
				free($3);
		}
		
	| APPE SP pathname CRLF
		{
			if (check_write($3, 1))
				store($3, "a", 0);
			if ($3 != NULL)
				free($3);
		}

	| ALLO check_login SP NUMBER CRLF
		{
			if ($2)
				reply(202, "ALLO command ignored.");
		}

	| ALLO check_login SP NUMBER SP R SP NUMBER CRLF
		{
			if ($2)
				reply(202, "ALLO command ignored.");
		}

	| RNTO SP pathname CRLF
		{
			if (check_write($3, 0)) {
				if (fromname) {
					renamecmd(fromname, $3);
					REASSIGN(fromname, NULL);
				} else {
					reply(503, "Bad sequence of commands.");
				}
			}
			if ($3 != NULL)
				free($3);
		}

	| ABOR check_login CRLF
		{
			if (is_oob)
				abor();
			else if ($2)
				reply(225, "ABOR command successful.");
		}

	| DELE SP pathname CRLF
		{
			if (check_write($3, 0))
				delete($3);
			if ($3 != NULL)
				free($3);
		}

	| RMD SP pathname CRLF
		{
			if (check_write($3, 0))
				removedir($3);
			if ($3 != NULL)
				free($3);
		}

	| MKD SP pathname CRLF
		{
			if (check_write($3, 0))
				makedir($3);
			if ($3 != NULL)
				free($3);
		}

	| PWD check_login CRLF
		{
			if ($2)
				pwd();
		}

	| LIST check_login CRLF
		{
			char *argv[] = { INTERNAL_LS, "-lgA", NULL };
			
			if (CURCLASS_FLAGS_ISSET(hidesymlinks))
				argv[1] = "-LlgA";
			if ($2)
				retrieve(argv, "");
		}

	| LIST check_login SP pathname CRLF
		{
			char *argv[] = { INTERNAL_LS, "-lgA", NULL, NULL };

			if (CURCLASS_FLAGS_ISSET(hidesymlinks))
				argv[1] = "-LlgA";
			if ($2 && $4 != NULL) {
				argv[2] = $4;
				retrieve(argv, $4);
			}
			if ($4 != NULL)
				free($4);
		}

	| NLST check_login CRLF
		{
			if ($2)
				send_file_list(".");
		}

	| NLST check_login SP pathname CRLF
		{
			if ($2)
				send_file_list($4);
			free($4);
		}

	| SITE SP HELP CRLF
		{
			help(sitetab, NULL);
		}

	| SITE SP CHMOD SP octal_number SP pathname CRLF
		{
			if (check_write($7, 0)) {
				if (($5 == -1) || ($5 > 0777))
					reply(501,
				"CHMOD: Mode value must be between 0 and 0777");
				else if (chmod($7, $5) < 0)
					perror_reply(550, $7);
				else
					reply(200, "CHMOD command successful.");
			}
			if ($7 != NULL)
				free($7);
		}

	| SITE SP HELP SP STRING CRLF
		{
			help(sitetab, $5);
			free($5);
		}

	| SITE SP IDLE check_login CRLF
		{
			if ($4) {
				reply(200,
				    "Current IDLE time limit is " LLF
				    " seconds; max " LLF,
				    (LLT)curclass.timeout,
				    (LLT)curclass.maxtimeout);
			}
		}

	| SITE SP IDLE check_login SP NUMBER CRLF
		{
			if ($4) {
				if ($6.i < 30 || $6.i > curclass.maxtimeout) {
					reply(501,
				"IDLE time limit must be between 30 and "
					    LLF " seconds",
					    (LLT)curclass.maxtimeout);
				} else {
					curclass.timeout = $6.i;
					(void) alarm(curclass.timeout);
					reply(200,
					    "IDLE time limit set to "
					    LLF " seconds",
					    (LLT)curclass.timeout);
				}
			}
		}

	| SITE SP RATEGET check_login CRLF
		{
			if ($4) {
				reply(200,
				    "Current RATEGET is " LLF " bytes/sec",
				    (LLT)curclass.rateget);
			}
		}

	| SITE SP RATEGET check_login SP STRING CRLF
		{
			char errbuf[100];
			char *p = $6;
			LLT rate;

			if ($4) {
				rate = strsuftollx("RATEGET", p, 0,
				    curclass.maxrateget
				    ? curclass.maxrateget
				    : LLTMAX, errbuf, sizeof(errbuf));
				if (errbuf[0])
					reply(501, "%s", errbuf);
				else {
					curclass.rateget = rate;
					reply(200,
					    "RATEGET set to " LLF " bytes/sec",
					    (LLT)curclass.rateget);
				}
			}
			free($6);
		}

	| SITE SP RATEPUT check_login CRLF
		{
			if ($4) {
				reply(200,
				    "Current RATEPUT is " LLF " bytes/sec",
				    (LLT)curclass.rateput);
			}
		}

	| SITE SP RATEPUT check_login SP STRING CRLF
		{
			char errbuf[100];
			char *p = $6;
			LLT rate;

			if ($4) {
				rate = strsuftollx("RATEPUT", p, 0,
				    curclass.maxrateput
				    ? curclass.maxrateput
				    : LLTMAX, errbuf, sizeof(errbuf));
				if (errbuf[0])
					reply(501, "%s", errbuf);
				else {
					curclass.rateput = rate;
					reply(200,
					    "RATEPUT set to " LLF " bytes/sec",
					    (LLT)curclass.rateput);
				}
			}
			free($6);
		}

	| SITE SP UMASK check_login CRLF
		{
			int oldmask;

			if ($4) {
				oldmask = umask(0);
				(void) umask(oldmask);
				reply(200, "Current UMASK is %03o", oldmask);
			}
		}

	| SITE SP UMASK check_login SP octal_number CRLF
		{
			int oldmask;

			if ($4 && check_write("", 0)) {
				if (($6 == -1) || ($6 > 0777)) {
					reply(501, "Bad UMASK value");
				} else {
					oldmask = umask($6);
					reply(200,
					    "UMASK set to %03o (was %03o)",
					    $6, oldmask);
				}
			}
		}

	| SYST CRLF
		{
			if (EMPTYSTR(version))
				reply(215, "UNIX Type: L%d", NBBY);
			else
				reply(215, "UNIX Type: L%d Version: %s", NBBY,
				    version);
		}

	| STAT check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				statfilecmd($4);
			if ($4 != NULL)
				free($4);
		}
		
	| STAT CRLF
		{
			if (is_oob)
				statxfer();
			else
				statcmd();
		}

	| HELP CRLF
		{
			help(cmdtab, NULL);
		}

	| HELP SP STRING CRLF
		{
			char *cp = $3;

			if (strncasecmp(cp, "SITE", 4) == 0) {
				cp = $3 + 4;
				if (*cp == ' ')
					cp++;
				if (*cp)
					help(sitetab, cp);
				else
					help(sitetab, NULL);
			} else
				help(cmdtab, $3);
			free($3);
		}

	| NOOP CRLF
		{
			reply(200, "NOOP command successful.");
		}

						/* RFC 2228 */
	| AUTH SP mechanism_name CRLF
		{
			reply(502, "RFC 2228 authentication not implemented.");
			free($3);
		}

	| ADAT SP base64data CRLF
		{
			reply(503,
			    "Please set authentication state with AUTH.");
			free($3);
		}

	| PROT SP prot_code CRLF
		{
			reply(503,
			    "Please set protection buffer size with PBSZ.");
			free($3);
		}

	| PBSZ SP decimal_integer CRLF
		{
			reply(503,
			    "Please set authentication state with AUTH.");
		}

	| CCC CRLF
		{
			reply(533, "No protection enabled.");
		}

	| MIC SP base64data CRLF
		{
			reply(502, "RFC 2228 authentication not implemented.");
			free($3);
		}

	| CONF SP base64data CRLF
		{
			reply(502, "RFC 2228 authentication not implemented.");
			free($3);
		}

	| ENC SP base64data CRLF
		{
			reply(502, "RFC 2228 authentication not implemented.");
			free($3);
		}

						/* RFC 2389 */
	| FEAT CRLF
		{

			feat();
		}

	| OPTS SP STRING CRLF
		{
			
			opts($3);
			free($3);
		}


				/* extensions from draft-ietf-ftpext-mlst-11 */

		/*
		 * Return size of file in a format suitable for
		 * using with RESTART (we just count bytes).
		 */
	| SIZE check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				sizecmd($4);
			if ($4 != NULL)
				free($4);
		}

		/*
		 * Return modification time of file as an ISO 3307
		 * style time. E.g. YYYYMMDDHHMMSS or YYYYMMDDHHMMSS.xxx
		 * where xxx is the fractional second (of any precision,
		 * not necessarily 3 digits)
		 */
	| MDTM check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL) {
				struct stat stbuf;
				if (stat($4, &stbuf) < 0)
					perror_reply(550, $4);
				else if (!S_ISREG(stbuf.st_mode)) {
					reply(550, "%s: not a plain file.", $4);
				} else {
					struct tm *t;

					t = gmtime(&stbuf.st_mtime);
					reply(213,
					    "%04d%02d%02d%02d%02d%02d",
					    TM_YEAR_BASE + t->tm_year,
					    t->tm_mon+1, t->tm_mday,
					    t->tm_hour, t->tm_min, t->tm_sec);
				}
			}
			if ($4 != NULL)
				free($4);
		}

	| MLST check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				mlst($4);
			if ($4 != NULL)
				free($4);
		}
		
	| MLST check_login CRLF
		{
			mlst(NULL);
		}

	| MLSD check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				mlsd($4);
			if ($4 != NULL)
				free($4);
		}
		
	| MLSD check_login CRLF
		{
			mlsd(NULL);
		}

	| error CRLF
		{
			yyerrok;
		}
	;

rcmd
	: REST check_login SP NUMBER CRLF
		{
			if ($2) {
				REASSIGN(fromname, NULL);
				restart_point = (off_t)$4.ll;
				reply(350,
    "Restarting at " LLF ". Send STORE or RETRIEVE to initiate transfer.",
				    (LLT)restart_point);
			}
		}

	| RNFR SP pathname CRLF
		{
			restart_point = (off_t) 0;
			if (check_write($3, 0)) {
				REASSIGN(fromname, NULL);
				fromname = renamefrom($3);
			}
			if ($3 != NULL)
				free($3);
		}
	;

username
	: STRING
	;

password
	: /* empty */
		{
			$$ = (char *)calloc(1, sizeof(char));
		}

	| STRING
	;

byte_size
	: NUMBER
		{
			$$ = $1.i;
		}
	;

host_port
	: NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER
		{
			char *a, *p;

			memset(&data_dest, 0, sizeof(data_dest));
			data_dest.su_len = sizeof(struct sockaddr_in);
			data_dest.su_family = AF_INET;
			p = (char *)&data_dest.su_port;
			p[0] = $9.i; p[1] = $11.i;
			a = (char *)&data_dest.su_addr;
			a[0] = $1.i; a[1] = $3.i; a[2] = $5.i; a[3] = $7.i;
		}
	;

host_long_port4
	: NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER
		{
			char *a, *p;

			memset(&data_dest, 0, sizeof(data_dest));
			data_dest.su_len = sizeof(struct sockaddr_in);
			data_dest.su_family = AF_INET;
			p = (char *)&data_dest.su_port;
			p[0] = $15.i; p[1] = $17.i;
			a = (char *)&data_dest.su_addr;
			a[0] = $5.i; a[1] = $7.i; a[2] = $9.i; a[3] = $11.i;

			/* reject invalid LPRT command */
			if ($1.i != 4 || $3.i != 4 || $13.i != 2)
				memset(&data_dest, 0, sizeof(data_dest));
		}
	;

host_long_port6
	: NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER
		{
#ifdef INET6
			char *a, *p;

			memset(&data_dest, 0, sizeof(data_dest));
			data_dest.su_len = sizeof(struct sockaddr_in6);
			data_dest.su_family = AF_INET6;
			p = (char *)&data_dest.su_port;
			p[0] = $39.i; p[1] = $41.i;
			a = (char *)&data_dest.si_su.su_sin6.sin6_addr;
			a[0] = $5.i; a[1] = $7.i; a[2] = $9.i; a[3] = $11.i;
			a[4] = $13.i; a[5] = $15.i; a[6] = $17.i; a[7] = $19.i;
			a[8] = $21.i; a[9] = $23.i; a[10] = $25.i; a[11] = $27.i;
			a[12] = $29.i; a[13] = $31.i; a[14] = $33.i; a[15] = $35.i;
			if (his_addr.su_family == AF_INET6) {
				/* XXX: more sanity checks! */
				data_dest.su_scope_id = his_addr.su_scope_id;
			}
#else
			memset(&data_dest, 0, sizeof(data_dest));
#endif /* INET6 */
			/* reject invalid LPRT command */
			if ($1.i != 6 || $3.i != 16 || $37.i != 2)
				memset(&data_dest, 0, sizeof(data_dest));
		}
	;

form_code
	: N
		{
			$$ = FORM_N;
		}

	| T
		{
			$$ = FORM_T;
		}

	| C
		{
			$$ = FORM_C;
		}
	;

type_code
	: A
		{
			cmd_type = TYPE_A;
			cmd_form = FORM_N;
		}

	| A SP form_code
		{
			cmd_type = TYPE_A;
			cmd_form = $3;
		}

	| E
		{
			cmd_type = TYPE_E;
			cmd_form = FORM_N;
		}

	| E SP form_code
		{
			cmd_type = TYPE_E;
			cmd_form = $3;
		}

	| I
		{
			cmd_type = TYPE_I;
		}

	| L
		{
			cmd_type = TYPE_L;
			cmd_bytesz = NBBY;
		}

	| L SP byte_size
		{
			cmd_type = TYPE_L;
			cmd_bytesz = $3;
		}

		/* this is for a bug in the BBN ftp */
	| L byte_size
		{
			cmd_type = TYPE_L;
			cmd_bytesz = $2;
		}
	;

struct_code
	: F
		{
			$$ = STRU_F;
		}

	| R
		{
			$$ = STRU_R;
		}

	| P
		{
			$$ = STRU_P;
		}
	;

mode_code
	: S
		{
			$$ = MODE_S;
		}

	| B
		{
			$$ = MODE_B;
		}

	| C
		{
			$$ = MODE_C;
		}
	;

pathname
	: pathstring
		{
			/*
			 * Problem: this production is used for all pathname
			 * processing, but only gives a 550 error reply.
			 * This is a valid reply in some cases but not in
			 * others.
			 */
			if (logged_in && $1 && *$1 == '~') {
				char	*path, *home, *result;
				size_t	len;

				path = strchr($1 + 1, '/');
				if (path != NULL)
					*path++ = '\0';
				if ($1[1] == '\0')
					home = homedir;
				else {
					struct passwd	*hpw;

					if ((hpw = getpwnam($1 + 1)) != NULL)
						home = hpw->pw_dir;
					else
						home = $1;
				}
				len = strlen(home) + 1;
				if (path != NULL)
					len += strlen(path) + 1;
				if ((result = malloc(len)) == NULL)
					fatal("Local resource failure: malloc");
				strlcpy(result, home, len);
				if (path != NULL) {
					strlcat(result, "/", len);
					strlcat(result, path, len);
				}
				$$ = result;
				free($1);
			} else
				$$ = $1;
		}
	;

pathstring
	: STRING
	;

octal_number
	: NUMBER
		{
			int ret, dec, multby, digit;

			/*
			 * Convert a number that was read as decimal number
			 * to what it would be if it had been read as octal.
			 */
			dec = $1.i;
			multby = 1;
			ret = 0;
			while (dec) {
				digit = dec%10;
				if (digit > 7) {
					ret = -1;
					break;
				}
				ret += digit * multby;
				multby *= 8;
				dec /= 10;
			}
			$$ = ret;
		}
	;

mechanism_name
	: STRING
	;

base64data
	: STRING
	;

prot_code
	: STRING
	;

decimal_integer
	: NUMBER
		{
			$$ = $1.i;
		}
	;

check_login
	: /* empty */
		{
			if (logged_in)
				$$ = 1;
			else {
				reply(530, "Please login with USER and PASS.");
				$$ = 0;
				hasyyerrored = 1;
			}
		}
	;

%%

#define	CMD	0	/* beginning of command */
#define	ARGS	1	/* expect miscellaneous arguments */
#define	STR1	2	/* expect SP followed by STRING */
#define	STR2	3	/* expect STRING */
#define	OSTR	4	/* optional SP then STRING */
#define	ZSTR1	5	/* SP then optional STRING */
#define	ZSTR2	6	/* optional STRING after SP */
#define	SITECMD	7	/* SITE command */
#define	NSTR	8	/* Number followed by a string */
#define NOARGS	9	/* No arguments allowed */
#define EOLN	10	/* End of line */

struct tab cmdtab[] = {
				/* From RFC 959, in order defined (5.3.1) */
	{ "USER", USER, STR1,	1,	"<sp> username" },
	{ "PASS", PASS, ZSTR1,	1,	"<sp> password" },
	{ "ACCT", ACCT, STR1,	0,	"(specify account)" },
	{ "CWD",  CWD,  OSTR,	1,	"[ <sp> directory-name ]" },
	{ "CDUP", CDUP, NOARGS,	1,	"(change to parent directory)" },
	{ "SMNT", SMNT, ARGS,	0,	"(structure mount)" },
	{ "QUIT", QUIT, NOARGS,	1,	"(terminate service)" },
	{ "REIN", REIN, NOARGS,	0,	"(reinitialize server state)" },
	{ "PORT", PORT, ARGS,	1,	"<sp> b0, b1, b2, b3, b4, b5" },
	{ "LPRT", LPRT, ARGS,	1,	"<sp> af, hal, h1, h2, h3,..., pal, p1, p2..." },
	{ "EPRT", EPRT, STR1,	1,	"<sp> |af|addr|port|" },
	{ "PASV", PASV, NOARGS,	1,	"(set server in passive mode)" },
	{ "LPSV", LPSV, ARGS,	1,	"(set server in passive mode)" },
	{ "EPSV", EPSV, ARGS,	1,	"[<sp> af|ALL]" },
	{ "TYPE", TYPE, ARGS,	1,	"<sp> [ A | E | I | L ]" },
	{ "STRU", STRU, ARGS,	1,	"(specify file structure)" },
	{ "MODE", MODE, ARGS,	1,	"(specify transfer mode)" },
	{ "RETR", RETR, STR1,	1,	"<sp> file-name" },
	{ "STOR", STOR, STR1,	1,	"<sp> file-name" },
	{ "STOU", STOU, STR1,	1,	"<sp> file-name" },
	{ "APPE", APPE, STR1,	1,	"<sp> file-name" },
	{ "ALLO", ALLO, ARGS,	1,	"allocate storage (vacuously)" },
	{ "REST", REST, ARGS,	1,	"<sp> offset (restart command)" },
	{ "RNFR", RNFR, STR1,	1,	"<sp> file-name" },
	{ "RNTO", RNTO, STR1,	1,	"<sp> file-name" },
	{ "ABOR", ABOR, NOARGS,	4,	"(abort operation)" },
	{ "DELE", DELE, STR1,	1,	"<sp> file-name" },
	{ "RMD",  RMD,  STR1,	1,	"<sp> path-name" },
	{ "MKD",  MKD,  STR1,	1,	"<sp> path-name" },
	{ "PWD",  PWD,  NOARGS,	1,	"(return current directory)" },
	{ "LIST", LIST, OSTR,	1,	"[ <sp> path-name ]" },
	{ "NLST", NLST, OSTR,	1,	"[ <sp> path-name ]" },
	{ "SITE", SITE, SITECMD, 1,	"site-cmd [ <sp> arguments ]" },
	{ "SYST", SYST, NOARGS,	1,	"(get type of operating system)" },
	{ "STAT", STAT, OSTR,	4,	"[ <sp> path-name ]" },
	{ "HELP", HELP, OSTR,	1,	"[ <sp> <string> ]" },
	{ "NOOP", NOOP, NOARGS,	2,	"" },

				/* From RFC 2228, in order defined */
	{ "AUTH", AUTH, STR1,	1,	"<sp> mechanism-name" },
	{ "ADAT", ADAT, STR1,	1,	"<sp> base-64-data" },
	{ "PROT", PROT, STR1,	1,	"<sp> prot-code" },
	{ "PBSZ", PBSZ, ARGS,	1,	"<sp> decimal-integer" },
	{ "CCC",  CCC,  NOARGS,	1,	"(Disable data protection)" },
	{ "MIC",  MIC,  STR1,	4,	"<sp> base64data" },
	{ "CONF", CONF, STR1,	4,	"<sp> base64data" },
	{ "ENC",  ENC,  STR1,	4,	"<sp> base64data" },

				/* From RFC 2389, in order defined */
	{ "FEAT", FEAT, NOARGS,	1,	"(display extended features)" },
	{ "OPTS", OPTS, STR1,	1,	"<sp> command [ <sp> options ]" },

				/* from draft-ietf-ftpext-mlst-11 */
	{ "MDTM", MDTM, OSTR,	1,	"<sp> path-name" },
	{ "SIZE", SIZE, OSTR,	1,	"<sp> path-name" },
	{ "MLST", MLST, OSTR,	2,	"[ <sp> path-name ]" },
	{ "MLSD", MLSD, OSTR,	1,	"[ <sp> directory-name ]" },

				/* obsolete commands */
	{ "MAIL", MAIL, OSTR,	0,	"(mail to user)" },
	{ "MLFL", MLFL, OSTR,	0,	"(mail file)" },
	{ "MRCP", MRCP, STR1,	0,	"(mail recipient)" },
	{ "MRSQ", MRSQ, OSTR,	0,	"(mail recipient scheme question)" },
	{ "MSAM", MSAM, OSTR,	0,	"(mail send to terminal and mailbox)" },
	{ "MSND", MSND, OSTR,	0,	"(mail send to terminal)" },
	{ "MSOM", MSOM, OSTR,	0,	"(mail send to terminal or mailbox)" },
	{ "XCUP", CDUP, NOARGS,	1,	"(change to parent directory)" },
	{ "XCWD", CWD,  OSTR,	1,	"[ <sp> directory-name ]" },
	{ "XMKD", MKD,  STR1,	1,	"<sp> path-name" },
	{ "XPWD", PWD,  NOARGS,	1,	"(return current directory)" },
	{ "XRMD", RMD,  STR1,	1,	"<sp> path-name" },

	{  NULL,  0,	0,	0,	0 }
};

struct tab sitetab[] = {
	{ "CHMOD",	CHMOD,	NSTR,	1,	"<sp> mode <sp> file-name" },
	{ "HELP",	HELP,	OSTR,	1,	"[ <sp> <string> ]" },
	{ "IDLE",	IDLE,	ARGS,	1,	"[ <sp> maximum-idle-time ]" },
	{ "RATEGET",	RATEGET,OSTR,	1,	"[ <sp> get-throttle-rate ]" },
	{ "RATEPUT",	RATEPUT,OSTR,	1,	"[ <sp> put-throttle-rate ]" },
	{ "UMASK",	UMASK,	ARGS,	1,	"[ <sp> umask ]" },
	{ NULL,		0,	0,	0,	NULL }
};

/*
 * Check if a filename is allowed to be modified (isupload == 0) or
 * uploaded (isupload == 1), and if necessary, check the filename is `sane'.
 * If the filename is NULL, fail.
 * If the filename is "", don't do the sane name check.
 */
static int
check_write(const char *file, int isupload)
{
	if (file == NULL)
		return (0);
	if (! logged_in) {
		reply(530, "Please login with USER and PASS.");
		return (0);
	}
		/* checking modify */
	if (! isupload && ! CURCLASS_FLAGS_ISSET(modify)) {
		reply(502, "No permission to use this command.");
		return (0);
	}
		/* checking upload */
	if (isupload && ! CURCLASS_FLAGS_ISSET(upload)) {
		reply(502, "No permission to use this command.");
		return (0);
	}

		/* checking sanenames */
	if (file[0] != '\0' && CURCLASS_FLAGS_ISSET(sanenames)) {
		const char *p;

		if (file[0] == '.')
			goto insane_name;
		for (p = file; *p; p++) {
			if (isalnum((unsigned char)*p) || *p == '-' || *p == '+' ||
			    *p == ',' || *p == '.' || *p == '_')
				continue;
 insane_name:
			reply(553, "File name `%s' not allowed.", file);
			return (0);
		}
	}
	return (1);
}

struct tab *
lookup(struct tab *p, const char *cmd)
{

	for (; p->name != NULL; p++)
		if (strcasecmp(cmd, p->name) == 0)
			return (p);
	return (0);
}

#include <arpa/telnet.h>

/*
 * getline - a hacked up version of fgets to ignore TELNET escape codes.
 */
char *
getline(char *s, int n, FILE *iop)
{
	int c;
	char *cs;

	cs = s;
/* tmpline may contain saved command from urgent mode interruption */
	for (c = 0; tmpline[c] != '\0' && --n > 0; ++c) {
		*cs++ = tmpline[c];
		if (tmpline[c] == '\n') {
			*cs++ = '\0';
			if (ftpd_debug)
				syslog(LOG_DEBUG, "command: %s", s);
			tmpline[0] = '\0';
			return(s);
		}
		if (c == 0)
			tmpline[0] = '\0';
	}
	while ((c = getc(iop)) != EOF) {
		total_bytes++;
		total_bytes_in++;
		c &= 0377;
		if (c == IAC) {
		    if ((c = getc(iop)) != EOF) {
			total_bytes++;
			total_bytes_in++;
			c &= 0377;
			switch (c) {
			case WILL:
			case WONT:
				c = getc(iop);
				total_bytes++;
				total_bytes_in++;
				cprintf(stdout, "%c%c%c", IAC, DONT, 0377&c);
				(void) fflush(stdout);
				continue;
			case DO:
			case DONT:
				c = getc(iop);
				total_bytes++;
				total_bytes_in++;
				cprintf(stdout, "%c%c%c", IAC, WONT, 0377&c);
				(void) fflush(stdout);
				continue;
			case IAC:
				break;
			default:
				continue;	/* ignore command */
			}
		    }
		}
		*cs++ = c;
		if (--n <= 0 || c == '\n')
			break;
	}
	if (c == EOF && cs == s)
		return (NULL);
	*cs++ = '\0';
	if (ftpd_debug) {
		if ((curclass.type != CLASS_GUEST &&
		    strncasecmp(s, "PASS ", 5) == 0) ||
		    strncasecmp(s, "ACCT ", 5) == 0) {
			/* Don't syslog passwords */
			syslog(LOG_DEBUG, "command: %.4s ???", s);
		} else {
			char *cp;
			int len;

			/* Don't syslog trailing CR-LF */
			len = strlen(s);
			cp = s + len - 1;
			while (cp >= s && (*cp == '\n' || *cp == '\r')) {
				--cp;
				--len;
			}
			syslog(LOG_DEBUG, "command: %.*s", len, s);
		}
	}
	return (s);
}

void
ftp_handle_line(char *cp)
{

	cmdp = cp;
	yyparse();
}

void
ftp_loop(void)
{

	while (1) {
		(void) alarm(curclass.timeout);
		if (getline(cbuf, sizeof(cbuf)-1, stdin) == NULL) {
			reply(221, "You could at least say goodbye.");
			dologout(0);
		}
		(void) alarm(0);
		ftp_handle_line(cbuf);
	}
	/*NOTREACHED*/
}

int
yylex(void)
{
	static int cpos, state;
	char *cp, *cp2;
	struct tab *p;
	int n;
	char c;

	switch (state) {

	case CMD:
		hasyyerrored = 0;
		if ((cp = strchr(cmdp, '\r'))) {
			*cp = '\0';
#if HAVE_SETPROCTITLE
			if (strncasecmp(cmdp, "PASS", 4) != 0 &&
			    strncasecmp(cmdp, "ACCT", 4) != 0)
				setproctitle("%s: %s", proctitle, cmdp);
#endif /* HAVE_SETPROCTITLE */
			*cp++ = '\n';
			*cp = '\0';
		}
		if ((cp = strpbrk(cmdp, " \n")))
			cpos = cp - cmdp;
		if (cpos == 0)
			cpos = 4;
		c = cmdp[cpos];
		cmdp[cpos] = '\0';
		p = lookup(cmdtab, cmdp);
		cmdp[cpos] = c;
		if (p != NULL) {
			if (is_oob && ! CMD_OOB(p)) {
				/* command will be handled in-band */
				return (0);
			} else if (! CMD_IMPLEMENTED(p)) {
				reply(502, "%s command not implemented.",
				    p->name);
				hasyyerrored = 1;
				break;
			}
			state = p->state;
			yylval.s = p->name;
			return (p->token);
		}
		break;

	case SITECMD:
		if (cmdp[cpos] == ' ') {
			cpos++;
			return (SP);
		}
		cp = &cmdp[cpos];
		if ((cp2 = strpbrk(cp, " \n")))
			cpos = cp2 - cmdp;
		c = cmdp[cpos];
		cmdp[cpos] = '\0';
		p = lookup(sitetab, cp);
		cmdp[cpos] = c;
		if (p != NULL) {
			if (!CMD_IMPLEMENTED(p)) {
				reply(502, "SITE %s command not implemented.",
				    p->name);
				hasyyerrored = 1;
				break;
			}
			state = p->state;
			yylval.s = p->name;
			return (p->token);
		}
		break;

	case OSTR:
		if (cmdp[cpos] == '\n') {
			state = EOLN;
			return (CRLF);
		}
		/* FALLTHROUGH */

	case STR1:
	case ZSTR1:
	dostr1:
		if (cmdp[cpos] == ' ') {
			cpos++;
			state = state == OSTR ? STR2 : state+1;
			return (SP);
		}
		break;

	case ZSTR2:
		if (cmdp[cpos] == '\n') {
			state = EOLN;
			return (CRLF);
		}
		/* FALLTHROUGH */

	case STR2:
		cp = &cmdp[cpos];
		n = strlen(cp);
		cpos += n - 1;
		/*
		 * Make sure the string is nonempty and \n terminated.
		 */
		if (n > 1 && cmdp[cpos] == '\n') {
			cmdp[cpos] = '\0';
			yylval.s = ftpd_strdup(cp);
			cmdp[cpos] = '\n';
			state = ARGS;
			return (STRING);
		}
		break;

	case NSTR:
		if (cmdp[cpos] == ' ') {
			cpos++;
			return (SP);
		}
		if (isdigit((unsigned char)cmdp[cpos])) {
			cp = &cmdp[cpos];
			while (isdigit((unsigned char)cmdp[++cpos]))
				;
			c = cmdp[cpos];
			cmdp[cpos] = '\0';
			yylval.u.i = atoi(cp);
			cmdp[cpos] = c;
			state = STR1;
			return (NUMBER);
		}
		state = STR1;
		goto dostr1;

	case ARGS:
		if (isdigit((unsigned char)cmdp[cpos])) {
			cp = &cmdp[cpos];
			while (isdigit((unsigned char)cmdp[++cpos]))
				;
			c = cmdp[cpos];
			cmdp[cpos] = '\0';
			yylval.u.i = atoi(cp);
			yylval.u.ll = STRTOLL(cp, (char **)NULL, 10);
			cmdp[cpos] = c;
			return (NUMBER);
		}
		if (strncasecmp(&cmdp[cpos], "ALL", 3) == 0
		    && !isalnum((unsigned char)cmdp[cpos + 3])) {
			cpos += 3;
			return (ALL);
		}
		switch (cmdp[cpos++]) {

		case '\n':
			state = EOLN;
			return (CRLF);

		case ' ':
			return (SP);

		case ',':
			return (COMMA);

		case 'A':
		case 'a':
			return (A);

		case 'B':
		case 'b':
			return (B);

		case 'C':
		case 'c':
			return (C);

		case 'E':
		case 'e':
			return (E);

		case 'F':
		case 'f':
			return (F);

		case 'I':
		case 'i':
			return (I);

		case 'L':
		case 'l':
			return (L);

		case 'N':
		case 'n':
			return (N);

		case 'P':
		case 'p':
			return (P);

		case 'R':
		case 'r':
			return (R);

		case 'S':
		case 's':
			return (S);

		case 'T':
		case 't':
			return (T);

		}
		break;

	case NOARGS:
		if (cmdp[cpos] == '\n') {
			state = EOLN;
			return (CRLF);
		}
		c = cmdp[cpos];
		cmdp[cpos] = '\0';
		reply(501, "'%s' command does not take any arguments.", cmdp);
		hasyyerrored = 1;
		cmdp[cpos] = c;
		break;

	case EOLN:
		state = CMD;
		return (0);

	default:
		fatal("Unknown state in scanner.");
	}
	yyerror(NULL);
	state = CMD;
	return (0);
}

/* ARGSUSED */
void
yyerror(char *s)
{
	char *cp;

	if (hasyyerrored || is_oob)
		return;
	if ((cp = strchr(cmdp,'\n')) != NULL)
		*cp = '\0';
	reply(500, "'%s': command not understood.", cmdp);
	hasyyerrored = 1;
}

static void
help(struct tab *ctab, const char *s)
{
	struct tab *c;
	int width, NCMDS;
	char *htype;

	if (ctab == sitetab)
		htype = "SITE ";
	else
		htype = "";
	width = 0, NCMDS = 0;
	for (c = ctab; c->name != NULL; c++) {
		int len = strlen(c->name);

		if (len > width)
			width = len;
		NCMDS++;
	}
	width = (width + 8) &~ 7;
	if (s == 0) {
		int i, j, w;
		int columns, lines;

		reply(-214, "%s", "");
		reply(0, "The following %scommands are recognized.", htype);
		reply(0, "(`-' = not implemented, `+' = supports options)");
		columns = 76 / width;
		if (columns == 0)
			columns = 1;
		lines = (NCMDS + columns - 1) / columns;
		for (i = 0; i < lines; i++) {
			cprintf(stdout, "    ");
			for (j = 0; j < columns; j++) {
				c = ctab + j * lines + i;
				cprintf(stdout, "%s", c->name);
				w = strlen(c->name);
				if (! CMD_IMPLEMENTED(c)) {
					CPUTC('-', stdout);
					w++;
				}
				if (CMD_HAS_OPTIONS(c)) {
					CPUTC('+', stdout);
					w++;
				}
				if (c + lines >= &ctab[NCMDS])
					break;
				while (w < width) {
					CPUTC(' ', stdout);
					w++;
				}
			}
			cprintf(stdout, "\r\n");
		}
		(void) fflush(stdout);
		reply(214, "Direct comments to ftp-bugs@%s.", hostname);
		return;
	}
	c = lookup(ctab, s);
	if (c == (struct tab *)0) {
		reply(502, "Unknown command '%s'.", s);
		return;
	}
	if (CMD_IMPLEMENTED(c))
		reply(214, "Syntax: %s%s %s", htype, c->name, c->help);
	else
		reply(504, "%s%-*s\t%s; not implemented.", htype, width,
		    c->name, c->help);
}

/*
 * Check that the structures used for a PORT, LPRT or EPRT command are
 * valid (data_dest, his_addr), and if necessary, detect ftp bounce attacks.
 * If family != -1 check that his_addr.su_family == family.
 */
static void
port_check(const char *cmd, int family)
{
	char h1[NI_MAXHOST], h2[NI_MAXHOST];
	char s1[NI_MAXHOST], s2[NI_MAXHOST];
#ifdef NI_WITHSCOPEID
	const int niflags = NI_NUMERICHOST | NI_NUMERICSERV | NI_WITHSCOPEID;
#else
	const int niflags = NI_NUMERICHOST | NI_NUMERICSERV;
#endif

	if (epsvall) {
		reply(501, "%s disallowed after EPSV ALL", cmd);
		return;
	}

	if (family != -1 && his_addr.su_family != family) {
 port_check_fail:
		reply(500, "Illegal %s command rejected", cmd);
		return;
	}

	if (data_dest.su_family != his_addr.su_family)
		goto port_check_fail;

			/* be paranoid, if told so */
	if (CURCLASS_FLAGS_ISSET(checkportcmd)) {
#ifdef INET6
		/*
		 * be paranoid, there are getnameinfo implementation that does
		 * not present scopeid portion
		 */
		if (data_dest.su_family == AF_INET6 &&
		    data_dest.su_scope_id != his_addr.su_scope_id)
			goto port_check_fail;
#endif

		if (getnameinfo((struct sockaddr *)&data_dest, data_dest.su_len,
		    h1, sizeof(h1), s1, sizeof(s1), niflags))
			goto port_check_fail;
		if (getnameinfo((struct sockaddr *)&his_addr, his_addr.su_len,
		    h2, sizeof(h2), s2, sizeof(s2), niflags))
			goto port_check_fail;

		if (atoi(s1) < IPPORT_RESERVED || strcmp(h1, h2) != 0)
			goto port_check_fail;
	}

	usedefault = 0;
	if (pdata >= 0) {
		(void) close(pdata);
		pdata = -1;
	}
	reply(200, "%s command successful.", cmd);
}
