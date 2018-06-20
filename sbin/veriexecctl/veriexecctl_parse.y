%{
/*
 * $FreeBSD$
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Originally derived from:
 * 	$NetBSD: veriexecctl_parse.y,v 1.3 2004/03/06 11:59:30 blymn Exp $
 *
 * Parser for verified exec fingerprint file.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <dev/veriexec/veriexec_ioctl.h>

/* yacc internal function */
int yylex(void);
void yyerror(const char *);

/* function prototypes */
static int convert(char *fp, unsigned int count, unsigned char *out);
static void do_ioctl(void);

/* ioctl parameter struct */
static struct verified_exec_params params;
extern int fd;
extern int lineno;
extern const char **algorithms;

%}

%union {
  char *string;
  int  intval;
}

%token EOL
%token <string> PATH
%token <string> STRING

%%

statement: /* empty */
  | statement path type fingerprint flags eol
  | statement error eol {
	yyclearin; /* discard lookahead */
	yyerrok;   /* no more error */
	fprintf(stderr, "skipping to next fingerprint\n");
  }
  ;

path: PATH 
{
	if (strlen($1) >= MAXPATHLEN) {
		yyerror("Path >= MAXPATHLEN");
		YYERROR;
	}
	strncpy(params.file, $1, MAXPATHLEN);
};

type: STRING
{
	const char **algop;

	for (algop = algorithms; *algop != NULL; algop++) {
		if (strcasecmp($1, *algop) == 0) {
			strlcpy(params.fp_type, $1, sizeof(params.fp_type));
			break;
		}
	}
	if (*algop == NULL) {
		yyerror("bad fingerprint type");
		YYERROR;
	}
};

fingerprint: STRING
{
	if (convert($1, MAXFINGERPRINTLEN, params.fingerprint) < 0) {
		yyerror("bad fingerprint");
		YYERROR;
	}
};

flags: /* empty */
	| flag_spec flags;

flag_spec: STRING
{
	if (strcasecmp($1, "indirect") == 0)
		params.flags |= VERIEXEC_INDIRECT;
	else if (strcasecmp($1, "file") == 0)
		params.flags |= VERIEXEC_FILE;
	else if (strcasecmp($1, "no_ptrace") == 0)
		params.flags |= VERIEXEC_NOTRACE;
	else if (strcasecmp($1, "trusted") == 0)
		params.flags |= VERIEXEC_TRUSTED;
	else {
		yyeror("bad flag specification");
		YYERROR;
	}
};

eol: EOL
{
	if (!YYRECOVERING()) /* Don't do the ioctl if we saw an error */
		do_ioctl();
};

%%
		
/*
 * Convert: takes the hexadecimal string pointed to by fp and converts
 * it to a "count" byte binary number which is stored in the array pointed to
 * by out.  Returns -1 if the conversion fails.
 */
static int
convert(char *fp, unsigned int count, unsigned char *out)
{
        unsigned int i;
	int value;

        for (i = 0; i < count; i++) {
		value = 0;
                if (isdigit(fp[i * 2]))
			value += fp[i * 2] - '0';
                else if (isxdigit(fp[i * 2]))
                        value = 10 + tolower(fp[i * 2]) - 'a';
                else
			return (-1);

		value <<= 4;
                if (isdigit(fp[i * 2 + 1]))
			value += fp[i * 2 + 1] - '0';
                else if (isxdigit(fp[i * 2 + 1]))
                        value = 10 + tolower(fp[i * 2 + 1]) - 'a';
                else
			return (-1);

		out[i] = value;
        }
        
        return (i);
}

/*
 * Perform the load of the fingerprint.  Assumes that the fingerprint
 * pseudo-device is opened and the file handle is in fd.
 */
static void
do_ioctl(void)
{
	if (ioctl(fd, VERIEXEC_LOAD, &params) < 0)
		fprintf(stderr,	"Ioctl failed with error `%s' on file %s\n",
			strerror(errno), params.file);
	bzero(&params, sizeof(params));
}
