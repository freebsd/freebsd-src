/*-
 * Copyright (c) 1999 Takanori Watanabe
 * Copyright (c) 1999, 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: debug.c,v 1.19 2000/08/16 18:15:00 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/acpi.h>

#include <dev/acpi/aml/aml_name.h>
#include <dev/acpi/aml/aml_amlmem.h>
#include <dev/acpi/aml/aml_status.h>
#include <dev/acpi/aml/aml_env.h>
#include <dev/acpi/aml/aml_obj.h>
#include <dev/acpi/aml/aml_evalobj.h>
#include <dev/acpi/aml/aml_parse.h>
#include <dev/acpi/aml/aml_region.h>
#include <dev/acpi/aml/aml_store.h>
#include <dev/acpi/aml/aml_common.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"

static int
print_named_object(struct aml_name *name, va_list ap)
{

	aml_print_curname(name);
	printf("\n");

	return (0);	/* always return success to continue the search */
}

void 
aml_dbgr(struct aml_environ *env1, struct aml_environ *env2)
{
#define CMDBUFLEN	512
#define ARGBUFLEN	512
	static	char lastcommand[CMDBUFLEN];
	char	commandline[CMDBUFLEN];
	char	argbuf[7][ARGBUFLEN];
	char	*ptr, *method;
	char	*np, *ep;
	int 	i;
	int	argnum;
	struct	aml_name *name;
	union	aml_object argv[7], *retval;

	while (1) {
		fputs("AML>", stderr);
		fgets(commandline, 512, stdin);
		commandline[512 - 1] = '\n';	/* safety */
		if (feof(stdin)) {
			commandline[0] = 'q';
		}
		if (commandline[0] == '\n') {
			memcpy(commandline, lastcommand, sizeof commandline);
		}
		memcpy(lastcommand, commandline, sizeof commandline);
		switch (commandline[0]) {
		case 's':
			if (env2 != NULL) {
				env2->stat = aml_stat_step;
			}
			/* FALLTHROUGH */
		case 'n':
			env1->stat = aml_stat_step;
			return;
		case 'c':
			env1->stat = aml_stat_none;
			return;
		case 'q':
			env1->stat = aml_stat_panic;
			return;
		case 't':
			/* NULL terminate */
			ptr = &commandline[1];
			while (ptr[0] != '\n')
				ptr++;
			ptr[0] = '\0';

			/* move pointer to object name */
			ptr = &commandline[1];
			while (ptr[0] == ' ')
				ptr++;

			/* show current tree if no argument */
			if (ptr[0] == '\0') {
				aml_showtree(env1->curname, 0);
				goto show_variables;
			}
			/* start from root? */
			if (ptr[0] == '\\') {
				if (ptr[1] == '\0') {
					aml_showtree(aml_get_rootname(), 0);
					goto show_variables;
				}
				if ((name = aml_find_from_namespace(aml_get_rootname(), ptr))) {
					aml_showtree(name, 0);
					goto show_variables;
				}
			}
			if ((name = aml_find_from_namespace(env1->curname, ptr))) {
				aml_showtree(name, 0);
			}
show_variables:
			for (i = 0; i < 7; i++) {
				struct aml_name *tmp =
				aml_local_stack_getArgX(NULL, i);

				if (tmp == NULL || tmp->property == NULL) {
					break;
				}
				printf("  Arg%d    ", i);
				aml_showobject(tmp->property);
			}
			for (i = 0; i < 8; i++) {
				struct aml_name *tmp =
				aml_local_stack_getLocalX(i);

				if (tmp == NULL || tmp->property == NULL) {
					continue;
				}
				printf("  Local%d  ", i);
				aml_showobject(tmp->property);
			}
			break;
		case 'i':
			aml_debug_prompt_reginput =
			    (aml_debug_prompt_reginput == 0) ? 1 : 0;
			if (aml_debug_prompt_reginput)
				fputs("REGION INPUT ON\n", stderr);
			else
				fputs("REGION INPUT OFF\n", stderr);
			break;
		case 'o':
			aml_debug_prompt_regoutput =
			    (aml_debug_prompt_regoutput == 0) ? 1 : 0;
			if (aml_debug_prompt_regoutput)
				fputs("REGION OUTPUT ON\n", stderr);
			else
				fputs("REGION OUTPUT OFF\n", stderr);
			break;
		case 'm':
			memman_statistics(aml_memman);
			break;
		case 'r':
			/* NULL terminate */
			ptr = &commandline[1];
			while (ptr[0] != '\n')
				ptr++;
			ptr[0] = '\0';

			/* move pointer to method name */
			ptr = &commandline[1];
			while (ptr[0] == ' ')
				ptr++;

			if (ptr[0] == '\0') {
				break;
			}
			name = aml_find_from_namespace(aml_get_rootname(), ptr);
			if (name == NULL) {
				printf("%s:%d:aml_dbgr: not found name %s\n",
				    __FILE__, __LINE__, ptr);
				break;
			}
			if (name->property == NULL ||
			    name->property->type != aml_t_method) {
				printf("%s:%d:aml_dbgr: not method %s\n",
				    __FILE__, __LINE__, ptr);
				break;
			}
			aml_showobject(name->property);
			method = ptr;

			argnum = name->property->meth.argnum & 0x07;
			if (argnum) {
				fputs("  Enter argument values "
				      "(ex. number 1 / string foo). "
				      "'q' to quit.\n", stderr);
			}
			/* get and parse argument values */
			for (i = 0; i < argnum; i++) {
retry:
				fprintf(stderr, "  Arg%d ? ", i);
				if (read(0, argbuf[i], ARGBUFLEN) == 0) {
					fputs("\n", stderr);
					goto retry;
				}
				argbuf[i][ARGBUFLEN - 1] = '\n';
				if (argbuf[i][0] == 'q') {
					goto finish_execution;
				}
				if (argbuf[i][0] == '\n') {
					goto retry;
				}
				/* move pointer to the value */
				ptr = &argbuf[i][0];
				while (ptr[0] != ' ' && ptr[0] != '\n') {
					ptr++;
				}
				while (ptr[0] == ' ') {
					ptr++;
				}
				if (ptr[0] == '\n') {
					goto retry;
				}
				switch (argbuf[i][0]) {
				case 'n':
					argv[i].type = aml_t_num;
					np = ptr;
					if (ptr[0] == '0' &&
					    ptr[1] == 'x') {
						argv[i].num.number = strtoq(ptr, &ep, 16);
					} else {
						argv[i].num.number = strtoq(ptr, &ep, 10);
					}
					if (np == ep) {
						fputs("Wrong value for number.\n",
						    stderr);
						goto retry;
					}
					break;
				case 's':
					argv[i].type = aml_t_string;
					argv[i].str.needfree = 0;
					argv[i].str.string = ptr;
					/* NULL ternimate */
					while (ptr[0] != '\n') {
						ptr++;
					}
					ptr[0] = '\0';
					break;
				default:
					fputs("Invalid data type "
					      "(supports number or string only)\n",
					    stderr);
					goto retry;
				}
			}
			bzero(lastcommand, sizeof lastcommand);
			fprintf(stderr, "==== Running %s. ====\n", method);
			aml_local_stack_push(aml_local_stack_create());
			retval = aml_invoke_method_by_name(method, argnum, argv);
			aml_showobject(retval);
			aml_local_stack_delete(aml_local_stack_pop());
			fprintf(stderr, "==== %s finished. ====\n", method);
finish_execution:
			break;
		case 'f':
			/* NULL terminate */
			ptr = &commandline[1];
			while (ptr[0] != '\n')
				ptr++;
			ptr[0] = '\0';

			/* move pointer to object name */
			ptr = &commandline[1];
			while (ptr[0] == ' ')
				ptr++;

			aml_apply_foreach_found_objects(aml_get_rootname(),
			    ptr, print_named_object);
			break;
		case 'h':
			fputs("s	Single step\n"
			      "n	Step program\n"
			      "c	Continue program being debugged\n"
			      "q	Quit method execution\n"
			      "t	Show local name space tree and variables\n"
			      "i	Toggle region input prompt\n"
			      "o	Toggle region output prompt\n"
			      "m	Show memory management statistics\n"
			      "r	Run specified method\n"
			      "f	Find named objects from namespace.\n"
			      "h	Show this messsage\n", stderr);
			break;
		}
	}
}
