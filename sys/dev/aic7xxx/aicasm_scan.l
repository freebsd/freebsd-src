%{
/*
 * Lexical Analyzer for the Aic7xxx SCSI Host adapter sequencer assembler.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: aicasm_scan.l,v 1.1.2.1 1997/03/16 19:01:28 gibbs Exp $
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <sys/queue.h>
#include <sys/types.h>

#include "aicasm.h"
#include "aicasm_symbol.h"
#include "y.tab.h"
%}

PATH		[-/A-Za-z0-9_.]*[./][-/A-Za-z0-9_.]*
WORD		[A-Za-z_][-A-Za-z_0-9]*
SPACE		[ \t]+

%x COMMENT

%%
\n			{ ++yylineno; }
"/*"			{ BEGIN COMMENT;  /* Enter comment eating state */ }
<COMMENT>"/*"		{ fprintf(stderr, "Warning! Comment within comment."); }
<COMMENT>\n		{ ++yylineno; }
<COMMENT>[^*/\n]*	;
<COMMENT>"*"+[^*/\n]*	;
<COMMENT>"/"+[^*/\n]*	;
<COMMENT>"*"+"/"	{ BEGIN INITIAL; }

{SPACE}			;

	/* Register/SCB/SRAM definition keywords */
register		{ return T_REGISTER; }
const			{ yylval.value = FALSE; return T_CONST; }
address			{ return T_ADDRESS; }
access_mode		{ return T_ACCESS_MODE; }
RW|RO|WO		{
				 if (strcmp(yytext, "RW") == 0)
					yylval.value = RW;
				 else if (strcmp(yytext, "RO") == 0)
					yylval.value = RO;
				 else
					yylval.value = WO;
				 return T_MODE;
			}
bit			{ return T_BIT; }
mask			{ return T_MASK; }
alias			{ return T_ALIAS; }
size			{ return T_SIZE; }
scb			{ return T_SCB; }
scratch_ram		{ return T_SRAM; }
accumulator		{ return T_ACCUM; }
allones			{ return T_ALLONES; }
allzeros		{ return T_ALLZEROS; }
none			{ return T_NONE; }
sindex			{ return T_SINDEX; }
A			{ return T_A; }

	/* Opcodes */
shl			{ return T_SHL; }
shr			{ return T_SHR; }
ror			{ return T_ROR; }
rol			{ return T_ROL; }
mvi			{ return T_MVI; }
mov			{ return T_MOV; }
clr			{ return T_CLR; }
jmp			{ return T_JMP; }
jc			{ return T_JC;	}
jnc			{ return T_JNC;	}
je			{ return T_JE;	}
jne			{ return T_JNE;	}
jz			{ return T_JZ;	}
jnz			{ return T_JNZ;	}
call			{ return T_CALL; }
add			{ return T_ADD; }
adc			{ return T_ADC; }
inc			{ return T_INC; }
dec			{ return T_DEC; }
stc			{ return T_STC;	}
clc			{ return T_CLC; }
cmp			{ return T_CMP;	}
xor			{ return T_XOR;	}
test			{ return T_TEST;}
and			{ return T_AND;	}
or			{ return T_OR;	}
ret			{ return T_RET; }
nop			{ return T_NOP; }
.if			{ return T_IF;  }
.else			{ return T_ELSE; }
.endif			{ return T_ENDIF; }

	/* Allowed Symbols */
[-+,:()~|&."{};<>[\]!]		{ return yytext[0]; }

	/* Number processing */
0[0-7]*			{
				yylval.value = strtol(yytext, NULL, 8);
				return T_NUMBER;
			}

0[xX][0-9a-fA-F]+	{
				yylval.value = strtoul(yytext + 2, NULL, 16);
				return T_NUMBER;
			}

[1-9][0-9]*		{
				yylval.value = strtol(yytext, NULL, 10);
				return T_NUMBER;
			}

	/* Include Files */
#include		{ return T_INCLUDE; }

	/* For parsing C include files with #define foo */
#define			{ yylval.value = TRUE; return T_CONST; }
	/* Throw away macros */
#define[^\n]*[()]+[^\n]* ;
{PATH}			{ yylval.str = strdup(yytext); return T_PATH; }

{WORD}			{ yylval.sym = symtable_get(yytext);  return T_SYMBOL; }

.			{ 
				char buf[255];

				snprintf(buf, sizeof(buf), "Invalid character "
					 "'%c'", yytext[0]);
				stop(buf, EX_DATAERR);
			}
%%

typedef struct include {
        YY_BUFFER_STATE  buffer;
        int              lineno;
        char            *filename;
	SLIST_ENTRY(include) links;
}include_t;

SLIST_HEAD(, include) include_stack;

void
include_file(file_name, type)
	char	*file_name;
	include_type type;
{
	FILE *newfile;
	include_t *include;

	newfile = NULL;
	/* Try the current directory first */
	if (includes_search_curdir != 0 || type == SOURCE_FILE)
		newfile = fopen(file_name, "r");

	if (newfile == NULL && type != SOURCE_FILE) {
                path_entry_t include_dir;
                for (include_dir = search_path.slh_first;
                     include_dir != NULL;                
                     include_dir = include_dir->links.sle_next) {
			char fullname[PATH_MAX];

			if ((include_dir->quoted_includes_only == TRUE)
			 && (type != QUOTED_INCLUDE))
				continue;

			snprintf(fullname, sizeof(fullname),
				 "%s/%s", include_dir->directory, file_name);

			if ((newfile = fopen(fullname, "r")) != NULL)
				break;
                }
        }

	if (newfile == NULL) {
		perror(file_name);
		stop("Unable to open input file", EX_SOFTWARE);
		/* NOTREACHED */
	}
	include = (include_t *)malloc(sizeof(include_t));
	if (include == NULL) {
		stop("Unable to allocate include stack entry", EX_SOFTWARE);
		/* NOTREACHED */
	}
	include->buffer = YY_CURRENT_BUFFER;
	include->lineno = yylineno;
	include->filename = yyfilename;
	SLIST_INSERT_HEAD(&include_stack, include, links);
	yy_switch_to_buffer(yy_create_buffer(newfile, YY_BUF_SIZE));
	yylineno = 1;
	yyfilename = strdup(file_name);
}

int
yywrap()
{
	include_t *include;

	yy_delete_buffer(YY_CURRENT_BUFFER);
	(void)fclose(yyin);
	if (yyfilename != NULL)
		free(yyfilename);
	include = include_stack.slh_first;
	if (include != NULL) {
		yy_switch_to_buffer(include->buffer);
		yylineno = include->lineno;
		yyfilename = include->filename;
		SLIST_REMOVE_HEAD(&include_stack, links);
		free(include);
		return (0);
	}
	return (1);
}
