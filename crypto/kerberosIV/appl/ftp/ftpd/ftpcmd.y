/*	$NetBSD: ftpcmd.y,v 1.6 1995/06/03 22:46:45 mycroft Exp $	*/

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
 *
 *	@(#)ftpcmd.y	8.3 (Berkeley) 4/6/94
 */

/*
 * Grammar for FTP commands.
 * See RFC 959.
 */

%{


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$Id: ftpcmd.y,v 1.35 1997/05/25 14:38:49 assar Exp $");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_FTP_H
#include <arpa/ftp.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <glob.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_BSD_BSD_H
#include <bsd/bsd.h>
#endif

#include <roken.h>

#ifdef SOCKS
#include <socks.h>
extern int LIBPREFIX(fclose)      __P((FILE *));
#endif

#include "extern.h"
#include "auth.h"

off_t	restart_point;

static	int cmd_type;
static	int cmd_form;
static	int cmd_bytesz;
char	cbuf[512];
char	*fromname;

struct tab {
	char	*name;
	short	token;
	short	state;
	short	implemented;	/* 1 if command is implemented */
	char	*help;
};

extern struct tab cmdtab[];
extern struct tab sitetab[];

static char	*copy (char *);
static void	 help (struct tab *, char *);
static struct tab *
		 lookup (struct tab *, char *);
static void	 sizecmd (char *);
static void	 toolong (int);
static int	 yylex (void);

/* This is for bison */

#if !defined(alloca) && !defined(HAVE_ALLOCA)
#define alloca(x) malloc(x)
#endif

%}

%union {
	int	i;
	char   *s;
}

%token
	A	B	C	E	F	I
	L	N	P	R	S	T

	SP	CRLF	COMMA

	USER	PASS	ACCT	REIN	QUIT	PORT
	PASV	TYPE	STRU	MODE	RETR	STOR
	APPE	MLFL	MAIL	MSND	MSOM	MSAM
	MRSQ	MRCP	ALLO	REST	RNFR	RNTO
	ABOR	DELE	CWD	LIST	NLST	SITE
	STAT	HELP	NOOP	MKD	RMD	PWD
	CDUP	STOU	SMNT	SYST	SIZE	MDTM

	UMASK	IDLE	CHMOD

	AUTH	ADAT	PROT	PBSZ	CCC	MIC
	CONF	ENC

	KAUTH	KLIST	FIND	URL

	LEXERR

%token	<s> STRING
%token	<i> NUMBER

%type	<i> check_login check_login_no_guest octal_number byte_size
%type	<i> struct_code mode_code type_code form_code
%type	<s> pathstring pathname password username

%start	cmd_list

%%

cmd_list
	: /* empty */
	| cmd_list cmd
		{
			fromname = (char *) 0;
			restart_point = (off_t) 0;
		}
	| cmd_list rcmd
	;

cmd
	: USER SP username CRLF
		{
			user($3);
			free($3);
		}
	| AUTH SP STRING CRLF
		{
			auth($3);
			free($3);
		}
	| ADAT SP STRING CRLF
		{
			adat($3);
			free($3);
		}
	| PBSZ SP NUMBER CRLF
		{
			pbsz($3);
		}
	| PROT SP STRING CRLF
		{
			prot($3);
		}
	| CCC CRLF
		{
			ccc();
		}
	| MIC SP STRING CRLF
		{
			mic($3);
			free($3);
		}
	| CONF SP STRING CRLF
		{
			conf($3);
			free($3);
		}
	| PASS SP password CRLF
		{
			pass($3);
			memset ($3, 0, strlen($3));
			free($3);
		}
	| PORT SP host_port CRLF
		{
			usedefault = 0;
			if (pdata >= 0) {
				close(pdata);
				pdata = -1;
			}
			reply(200, "PORT command successful.");
		}
	| PASV CRLF
		{
			passive();
		}
	| TYPE SP type_code CRLF
		{
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
	| STRU SP struct_code CRLF
		{
			switch ($3) {

			case STRU_F:
				reply(200, "STRU F ok.");
				break;

			default:
				reply(504, "Unimplemented STRU type.");
			}
		}
	| MODE SP mode_code CRLF
		{
			switch ($3) {

			case MODE_S:
				reply(200, "MODE S ok.");
				break;

			default:
				reply(502, "Unimplemented MODE type.");
			}
		}
	| ALLO SP NUMBER CRLF
		{
			reply(202, "ALLO command ignored.");
		}
	| ALLO SP NUMBER SP R SP NUMBER CRLF
		{
			reply(202, "ALLO command ignored.");
		}
	| RETR check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				retrieve((char *) 0, $4);
			if ($4 != NULL)
				free($4);
		}
	| STOR check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				do_store($4, "w", 0);
			if ($4 != NULL)
				free($4);
		}
	| APPE check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				do_store($4, "a", 0);
			if ($4 != NULL)
				free($4);
		}
	| NLST check_login CRLF
		{
			if ($2)
				send_file_list(".");
		}
	| NLST check_login SP STRING CRLF
		{
			if ($2 && $4 != NULL)
				send_file_list($4);
			if ($4 != NULL)
				free($4);
		}
	| LIST check_login CRLF
		{
#ifdef HAVE_LS_A
		  char *cmd = "/bin/ls -lA";
#else
		  char *cmd = "/bin/ls -la";
#endif
			if ($2)
				retrieve(cmd, "");
			
		}
	| LIST check_login SP pathname CRLF
		{
#ifdef HAVE_LS_A
		  char *cmd = "/bin/ls -lA %s";
#else
		  char *cmd = "/bin/ls -la %s";
#endif
			if ($2 && $4 != NULL)
				retrieve(cmd, $4);
			if ($4 != NULL)
				free($4);
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
			if(oobflag){
				if (file_size != (off_t) -1)
					reply(213, "Status: %ld of %ld bytes transferred",
						byte_count, file_size);
				else
					reply(213, "Status: %ld bytes transferred", byte_count);
			}else
				statcmd();
	}
	| DELE check_login_no_guest SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				do_delete($4);
			if ($4 != NULL)
				free($4);
		}
	| RNTO check_login_no_guest SP pathname CRLF
		{
			if($2){
				if (fromname) {
					renamecmd(fromname, $4);
					free(fromname);
					fromname = (char *) 0;
				} else {
					reply(503, "Bad sequence of commands.");
				}
			}
			if ($4 != NULL)
				free($4);
		}
	| ABOR CRLF
		{
			if(oobflag){
				reply(426, "Transfer aborted. Data connection closed.");
				reply(226, "Abort successful");
				oobflag = 0;
				longjmp(urgcatch, 1);
			}else
				reply(225, "ABOR command successful.");
		}
	| CWD check_login CRLF
		{
			if ($2)
				cwd(pw->pw_dir);
		}
	| CWD check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				cwd($4);
			if ($4 != NULL)
				free($4);
		}
	| HELP CRLF
		{
			help(cmdtab, (char *) 0);
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
					help(sitetab, (char *) 0);
			} else
				help(cmdtab, $3);
		}
	| NOOP CRLF
		{
			reply(200, "NOOP command successful.");
		}
	| MKD check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				makedir($4);
			if ($4 != NULL)
				free($4);
		}
	| RMD check_login_no_guest SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				removedir($4);
			if ($4 != NULL)
				free($4);
		}
	| PWD check_login CRLF
		{
			if ($2)
				pwd();
		}
	| CDUP check_login CRLF
		{
			if ($2)
				cwd("..");
		}
	| SITE SP HELP CRLF
		{
			help(sitetab, (char *) 0);
		}
	| SITE SP HELP SP STRING CRLF
		{
			help(sitetab, $5);
		}
	| SITE SP UMASK check_login CRLF
		{
			int oldmask;

			if ($4) {
				oldmask = umask(0);
				umask(oldmask);
				reply(200, "Current UMASK is %03o", oldmask);
			}
		}
	| SITE SP UMASK check_login_no_guest SP octal_number CRLF
		{
			int oldmask;

			if ($4) {
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
	| SITE SP CHMOD check_login_no_guest SP octal_number SP pathname CRLF
		{
			if ($4 && $8 != NULL) {
				if ($6 > 0777)
					reply(501,
				"CHMOD: Mode value must be between 0 and 0777");
				else if (chmod($8, $6) < 0)
					perror_reply(550, $8);
				else
					reply(200, "CHMOD command successful.");
			}
			if ($8 != NULL)
				free($8);
		}
	| SITE SP IDLE CRLF
		{
			reply(200,
			    "Current IDLE time limit is %d seconds; max %d",
				ftpd_timeout, maxtimeout);
		}
	| SITE SP IDLE SP NUMBER CRLF
		{
			if ($5 < 30 || $5 > maxtimeout) {
				reply(501,
			"Maximum IDLE time must be between 30 and %d seconds",
				    maxtimeout);
			} else {
				ftpd_timeout = $5;
				alarm((unsigned) ftpd_timeout);
				reply(200,
				    "Maximum IDLE time set to %d seconds",
				    ftpd_timeout);
			}
		}

	| SITE SP KAUTH check_login SP STRING CRLF
		{
			char *p;
			
			if(guest)
				reply(500, "Can't be done as guest.");
			else{
				if($4 && $6 != NULL){
				    p = strpbrk($6, " \t");
				    if(p){
					*p++ = 0;
					kauth($6, p + strspn(p, " \t"));
				    }else
					kauth($6, NULL);
				}
			}
			if($6 != NULL)
			    free($6);
		}
	| SITE SP KLIST check_login CRLF
		{
		    if($4)
			klist();
		}
	| SITE SP FIND check_login SP STRING CRLF
		{
		    if($4 && $6 != NULL)
			find($6);
		    if($6 != NULL)
			free($6);
		}
	| SITE SP URL CRLF
		{
			reply(200, "http://www.pdc.kth.se/kth-krb/");
		}
	| STOU check_login SP pathname CRLF
		{
			if ($2 && $4 != NULL)
				do_store($4, "w", 1);
			if ($4 != NULL)
				free($4);
		}
	| SYST CRLF
		{
#if defined(unix) || defined(__unix__) || defined(__unix) || defined(_AIX) || defined(_CRAY)
		    reply(215, "UNIX Type: L%d", NBBY);
#else
		    reply(215, "UNKNOWN Type: L%d", NBBY);
#endif
		}

		/*
		 * SIZE is not in RFC959, but Postel has blessed it and
		 * it will be in the updated RFC.
		 *
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
		 * MDTM is not in RFC959, but Postel has blessed it and
		 * it will be in the updated RFC.
		 *
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
					reply(550, "%s: %s",
					    $4, strerror(errno));
				else if (!S_ISREG(stbuf.st_mode)) {
					reply(550, "%s: not a plain file.", $4);
				} else {
					struct tm *t;
					t = gmtime(&stbuf.st_mtime);
					reply(213,
					      "%04d%02d%02d%02d%02d%02d",
					      t->tm_year + 1900,
					      t->tm_mon + 1,
					      t->tm_mday,
					      t->tm_hour,
					      t->tm_min,
					      t->tm_sec);
				}
			}
			if ($4 != NULL)
				free($4);
		}
	| QUIT CRLF
		{
			reply(221, "Goodbye.");
			dologout(0);
		}
	| error CRLF
		{
			yyerrok;
		}
	;
rcmd
	: RNFR check_login_no_guest SP pathname CRLF
		{
			restart_point = (off_t) 0;
			if ($2 && $4) {
				fromname = renamefrom($4);
				if (fromname == (char *) 0 && $4) {
					free($4);
				}
			}
		}
	| REST SP byte_size CRLF
		{
			fromname = (char *) 0;
			restart_point = $3;	/* XXX $3 is only "int" */
			reply(350, "Restarting at %ld. %s",
			      (long)restart_point,
			      "Send STORE or RETRIEVE to initiate transfer.");
		}
	| ENC SP STRING CRLF
		{
			enc($3);
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
	;

host_port
	: NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA
		NUMBER COMMA NUMBER
		{
			data_dest.sin_family = AF_INET;
			data_dest.sin_port = htons($9 * 256 + $11);
			data_dest.sin_addr.s_addr = 
			    htonl(($1 << 24) | ($3 << 16) | ($5 << 8) | $7);
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
			 * This is a valid reply in some cases but not in others.
			 */
			if (logged_in && $1 && *$1 == '~') {
				glob_t gl;
				int flags =
				 GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE;

				memset(&gl, 0, sizeof(gl));
				if (glob($1, flags, NULL, &gl) ||
				    gl.gl_pathc == 0) {
					reply(550, "not found");
					$$ = NULL;
				} else {
					$$ = strdup(gl.gl_pathv[0]);
				}
				globfree(&gl);
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
			dec = $1;
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


check_login_no_guest : check_login
		{
			$$ = $1 && !guest;
			if($1 && !$$)
				reply(550, "Permission denied");
		}
	;

check_login
	: /* empty */
		{
		    if(auth_complete && prot_level == prot_clear){
			reply(533, "Command protection level denied for paranoid reasons.");
			$$ = 0;
		    }else
			if (logged_in)
			    $$ = 1;
			else {
			    reply(530, "Please login with USER and PASS.");
			    $$ = 0;
			}
		}
	;

%%

extern jmp_buf errcatch;

#define	CMD	0	/* beginning of command */
#define	ARGS	1	/* expect miscellaneous arguments */
#define	STR1	2	/* expect SP followed by STRING */
#define	STR2	3	/* expect STRING */
#define	OSTR	4	/* optional SP then STRING */
#define	ZSTR1	5	/* SP then optional STRING */
#define	ZSTR2	6	/* optional STRING after SP */
#define	SITECMD	7	/* SITE command */
#define	NSTR	8	/* Number followed by a string */

struct tab cmdtab[] = {		/* In order defined in RFC 765 */
	{ "USER", USER, STR1, 1,	"<sp> username" },
	{ "PASS", PASS, ZSTR1, 1,	"<sp> password" },
	{ "ACCT", ACCT, STR1, 0,	"(specify account)" },
	{ "SMNT", SMNT, ARGS, 0,	"(structure mount)" },
	{ "REIN", REIN, ARGS, 0,	"(reinitialize server state)" },
	{ "QUIT", QUIT, ARGS, 1,	"(terminate service)", },
	{ "PORT", PORT, ARGS, 1,	"<sp> b0, b1, b2, b3, b4" },
	{ "PASV", PASV, ARGS, 1,	"(set server in passive mode)" },
	{ "TYPE", TYPE, ARGS, 1,	"<sp> [ A | E | I | L ]" },
	{ "STRU", STRU, ARGS, 1,	"(specify file structure)" },
	{ "MODE", MODE, ARGS, 1,	"(specify transfer mode)" },
	{ "RETR", RETR, STR1, 1,	"<sp> file-name" },
	{ "STOR", STOR, STR1, 1,	"<sp> file-name" },
	{ "APPE", APPE, STR1, 1,	"<sp> file-name" },
	{ "MLFL", MLFL, OSTR, 0,	"(mail file)" },
	{ "MAIL", MAIL, OSTR, 0,	"(mail to user)" },
	{ "MSND", MSND, OSTR, 0,	"(mail send to terminal)" },
	{ "MSOM", MSOM, OSTR, 0,	"(mail send to terminal or mailbox)" },
	{ "MSAM", MSAM, OSTR, 0,	"(mail send to terminal and mailbox)" },
	{ "MRSQ", MRSQ, OSTR, 0,	"(mail recipient scheme question)" },
	{ "MRCP", MRCP, STR1, 0,	"(mail recipient)" },
	{ "ALLO", ALLO, ARGS, 1,	"allocate storage (vacuously)" },
	{ "REST", REST, ARGS, 1,	"<sp> offset (restart command)" },
	{ "RNFR", RNFR, STR1, 1,	"<sp> file-name" },
	{ "RNTO", RNTO, STR1, 1,	"<sp> file-name" },
	{ "ABOR", ABOR, ARGS, 1,	"(abort operation)" },
	{ "DELE", DELE, STR1, 1,	"<sp> file-name" },
	{ "CWD",  CWD,  OSTR, 1,	"[ <sp> directory-name ]" },
	{ "XCWD", CWD,	OSTR, 1,	"[ <sp> directory-name ]" },
	{ "LIST", LIST, OSTR, 1,	"[ <sp> path-name ]" },
	{ "NLST", NLST, OSTR, 1,	"[ <sp> path-name ]" },
	{ "SITE", SITE, SITECMD, 1,	"site-cmd [ <sp> arguments ]" },
	{ "SYST", SYST, ARGS, 1,	"(get type of operating system)" },
	{ "STAT", STAT, OSTR, 1,	"[ <sp> path-name ]" },
	{ "HELP", HELP, OSTR, 1,	"[ <sp> <string> ]" },
	{ "NOOP", NOOP, ARGS, 1,	"" },
	{ "MKD",  MKD,  STR1, 1,	"<sp> path-name" },
	{ "XMKD", MKD,  STR1, 1,	"<sp> path-name" },
	{ "RMD",  RMD,  STR1, 1,	"<sp> path-name" },
	{ "XRMD", RMD,  STR1, 1,	"<sp> path-name" },
	{ "PWD",  PWD,  ARGS, 1,	"(return current directory)" },
	{ "XPWD", PWD,  ARGS, 1,	"(return current directory)" },
	{ "CDUP", CDUP, ARGS, 1,	"(change to parent directory)" },
	{ "XCUP", CDUP, ARGS, 1,	"(change to parent directory)" },
	{ "STOU", STOU, STR1, 1,	"<sp> file-name" },
	{ "SIZE", SIZE, OSTR, 1,	"<sp> path-name" },
	{ "MDTM", MDTM, OSTR, 1,	"<sp> path-name" },

	/* extensions from draft-ietf-cat-ftpsec-08 */
	{ "AUTH", AUTH,	STR1, 1,	"<sp> auth-type" },
	{ "ADAT", ADAT,	STR1, 1,	"<sp> auth-data" },
	{ "PBSZ", PBSZ,	ARGS, 1,	"<sp> buffer-size" },
	{ "PROT", PROT,	STR1, 1,	"<sp> prot-level" },
	{ "CCC",  CCC,	ARGS, 1,	"" },
	{ "MIC",  MIC,	STR1, 1,	"<sp> integrity command" },
	{ "CONF", CONF,	STR1, 1,	"<sp> confidentiality command" },
	{ "ENC",  ENC,	STR1, 1,	"<sp> privacy command" },

	{ NULL,   0,    0,    0,	0 }
};

struct tab sitetab[] = {
	{ "UMASK", UMASK, ARGS, 1,	"[ <sp> umask ]" },
	{ "IDLE", IDLE, ARGS, 1,	"[ <sp> maximum-idle-time ]" },
	{ "CHMOD", CHMOD, NSTR, 1,	"<sp> mode <sp> file-name" },
	{ "HELP", HELP, OSTR, 1,	"[ <sp> <string> ]" },

	{ "KAUTH", KAUTH, STR1, 1,	"<sp> principal [ <sp> ticket ]" },
	{ "KLIST", KLIST, ARGS, 1,	"(show ticket file)" },

	{ "FIND", FIND, STR1, 1,	"<sp> globexpr" },

	{ "URL",  URL,  ARGS, 1,	"?" },
	
	{ NULL,   0,    0,    0,	0 }
};

static struct tab *
lookup(struct tab *p, char *cmd)
{

	for (; p->name != NULL; p++)
		if (strcmp(cmd, p->name) == 0)
			return (p);
	return (0);
}

#include <arpa/telnet.h>

/*
 * getline - a hacked up version of fgets to ignore TELNET escape codes.
 */
char *
getline(char *s, int n)
{
	int c;
	char *cs;

	cs = s;
/* tmpline may contain saved command from urgent mode interruption */
	if(ftp_command){
	  strncpy(s, ftp_command, n);
	  if (debug)
	    syslog(LOG_DEBUG, "command: %s", s);
#ifdef XXX
	  fprintf(stderr, "%s\n", s);
#endif
	  return s;
	}
	prot_level = prot_clear;
	while ((c = getc(stdin)) != EOF) {
		c &= 0377;
		if (c == IAC) {
		    if ((c = getc(stdin)) != EOF) {
			c &= 0377;
			switch (c) {
			case WILL:
			case WONT:
				c = getc(stdin);
				printf("%c%c%c", IAC, DONT, 0377&c);
				fflush(stdout);
				continue;
			case DO:
			case DONT:
				c = getc(stdin);
				printf("%c%c%c", IAC, WONT, 0377&c);
				fflush(stdout);
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
	if (debug) {
		if (!guest && strncasecmp("pass ", s, 5) == 0) {
			/* Don't syslog passwords */
			syslog(LOG_DEBUG, "command: %.5s ???", s);
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
#ifdef XXX
	fprintf(stderr, "%s\n", s);
#endif
	return (s);
}

static RETSIGTYPE
toolong(int signo)
{

	reply(421,
	    "Timeout (%d seconds): closing control connection.",
	      ftpd_timeout);
	if (logging)
		syslog(LOG_INFO, "User %s timed out after %d seconds",
		    (pw ? pw -> pw_name : "unknown"), ftpd_timeout);
	dologout(1);
	SIGRETURN(0);
}

static int
yylex(void)
{
	static int cpos, state;
	char *cp, *cp2;
	struct tab *p;
	int n;
	char c;

	for (;;) {
		switch (state) {

		case CMD:
			signal(SIGALRM, toolong);
			alarm((unsigned) ftpd_timeout);
			if (getline(cbuf, sizeof(cbuf)-1) == NULL) {
				reply(221, "You could at least say goodbye.");
				dologout(0);
			}
			alarm(0);
#ifdef HASSETPROCTITLE
			if (strncasecmp(cbuf, "PASS", 4) != NULL)
				setproctitle("%s: %s", proctitle, cbuf);
#endif /* HASSETPROCTITLE */
			if ((cp = strchr(cbuf, '\r'))) {
				*cp++ = '\n';
				*cp = '\0';
			}
			if ((cp = strpbrk(cbuf, " \n")))
				cpos = cp - cbuf;
			if (cpos == 0)
				cpos = 4;
			c = cbuf[cpos];
			cbuf[cpos] = '\0';
			strupr(cbuf);
			p = lookup(cmdtab, cbuf);
			cbuf[cpos] = c;
			if (p != 0) {
				if (p->implemented == 0) {
					nack(p->name);
					longjmp(errcatch,0);
					/* NOTREACHED */
				}
				state = p->state;
				yylval.s = p->name;
				return (p->token);
			}
			break;

		case SITECMD:
			if (cbuf[cpos] == ' ') {
				cpos++;
				return (SP);
			}
			cp = &cbuf[cpos];
			if ((cp2 = strpbrk(cp, " \n")))
				cpos = cp2 - cbuf;
			c = cbuf[cpos];
			cbuf[cpos] = '\0';
			strupr(cp);
			p = lookup(sitetab, cp);
			cbuf[cpos] = c;
			if (p != 0) {
				if (p->implemented == 0) {
					state = CMD;
					nack(p->name);
					longjmp(errcatch,0);
					/* NOTREACHED */
				}
				state = p->state;
				yylval.s = p->name;
				return (p->token);
			}
			state = CMD;
			break;

		case OSTR:
			if (cbuf[cpos] == '\n') {
				state = CMD;
				return (CRLF);
			}
			/* FALLTHROUGH */

		case STR1:
		case ZSTR1:
		dostr1:
			if (cbuf[cpos] == ' ') {
				cpos++;
				state = state == OSTR ? STR2 : ++state;
				return (SP);
			}
			break;

		case ZSTR2:
			if (cbuf[cpos] == '\n') {
				state = CMD;
				return (CRLF);
			}
			/* FALLTHROUGH */

		case STR2:
			cp = &cbuf[cpos];
			n = strlen(cp);
			cpos += n - 1;
			/*
			 * Make sure the string is nonempty and \n terminated.
			 */
			if (n > 1 && cbuf[cpos] == '\n') {
				cbuf[cpos] = '\0';
				yylval.s = copy(cp);
				cbuf[cpos] = '\n';
				state = ARGS;
				return (STRING);
			}
			break;

		case NSTR:
			if (cbuf[cpos] == ' ') {
				cpos++;
				return (SP);
			}
			if (isdigit(cbuf[cpos])) {
				cp = &cbuf[cpos];
				while (isdigit(cbuf[++cpos]))
					;
				c = cbuf[cpos];
				cbuf[cpos] = '\0';
				yylval.i = atoi(cp);
				cbuf[cpos] = c;
				state = STR1;
				return (NUMBER);
			}
			state = STR1;
			goto dostr1;

		case ARGS:
			if (isdigit(cbuf[cpos])) {
				cp = &cbuf[cpos];
				while (isdigit(cbuf[++cpos]))
					;
				c = cbuf[cpos];
				cbuf[cpos] = '\0';
				yylval.i = atoi(cp);
				cbuf[cpos] = c;
				return (NUMBER);
			}
			switch (cbuf[cpos++]) {

			case '\n':
				state = CMD;
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

		default:
			fatal("Unknown state in scanner.");
		}
		yyerror((char *) 0);
		state = CMD;
		longjmp(errcatch,0);
	}
}

static char *
copy(char *s)
{
	char *p;

	p = strdup(s);
	if (p == NULL)
		fatal("Ran out of memory.");
	return p;
}

static void
help(struct tab *ctab, char *s)
{
	struct tab *c;
	int width, NCMDS;
	char *type;
	char buf[1024];

	if (ctab == sitetab)
		type = "SITE ";
	else
		type = "";
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

		lreply(214, "The following %scommands are recognized %s.",
		    type, "(* =>'s unimplemented)");
		columns = 76 / width;
		if (columns == 0)
			columns = 1;
		lines = (NCMDS + columns - 1) / columns;
		for (i = 0; i < lines; i++) {
		    strcpy (buf, "   ");
		    for (j = 0; j < columns; j++) {
			c = ctab + j * lines + i;
			snprintf (buf + strlen(buf), sizeof(buf) - strlen(buf),
				  "%s%c", c->name, c->implemented ? ' ' : '*');
			if (c + lines >= &ctab[NCMDS])
			    break;
			w = strlen(c->name) + 1;
			while (w < width) {
			    strcat(buf, " ");
			    w++;
			}
		    }
		    lreply(214, buf);
		}
		reply(214, "Direct comments to kth-krb-bugs@pdc.kth.se");
		return;
	}
	strupr(s);
	c = lookup(ctab, s);
	if (c == (struct tab *)0) {
		reply(502, "Unknown command %s.", s);
		return;
	}
	if (c->implemented)
		reply(214, "Syntax: %s%s %s", type, c->name, c->help);
	else
		reply(214, "%s%-*s\t%s; unimplemented.", type, width,
		    c->name, c->help);
}

static void
sizecmd(char *filename)
{
	switch (type) {
	case TYPE_L:
	case TYPE_I: {
		struct stat stbuf;
		if (stat(filename, &stbuf) < 0 || !S_ISREG(stbuf.st_mode))
			reply(550, "%s: not a plain file.", filename);
		else
			reply(213, "%lu", (unsigned long)stbuf.st_size);
		break; }
	case TYPE_A: {
		FILE *fin;
		int c;
		off_t count;
		struct stat stbuf;
		fin = fopen(filename, "r");
		if (fin == NULL) {
			perror_reply(550, filename);
			return;
		}
		if (fstat(fileno(fin), &stbuf) < 0 || !S_ISREG(stbuf.st_mode)) {
			reply(550, "%s: not a plain file.", filename);
			fclose(fin);
			return;
		}

		count = 0;
		while((c=getc(fin)) != EOF) {
			if (c == '\n')	/* will get expanded to \r\n */
				count++;
			count++;
		}
		fclose(fin);

		reply(213, "%ld", count);
		break; }
	default:
		reply(504, "SIZE not implemented for Type %c.", "?AEIL"[type]);
	}
}
