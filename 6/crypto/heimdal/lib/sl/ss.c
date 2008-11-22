/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "sl_locl.h"
#include <com_err.h>
#include "ss.h"

RCSID("$Id: ss.c,v 1.6 2000/05/25 00:14:58 assar Exp $");

struct ss_subst {
    char *name;
    char *version;
    char *info;
    ss_request_table *table;
};

static struct ss_subst subsystems[2];
static int num_subsystems;

int
ss_create_invocation(const char *subsystem, 
		     const char *version, 
		     const char *info, 
		     ss_request_table *table, 
		     int *code)
{
    struct ss_subst *ss;

    if(num_subsystems >= sizeof(subsystems) / sizeof(subsystems[0])) {
	*code = 17;
	return 0;
    }
    ss = &subsystems[num_subsystems];
    ss->name = ss->version = ss->info = NULL;
    if (subsystem != NULL) {
	ss->name = strdup (subsystem);
	if (ss->name == NULL) {
	    *code = ENOMEM;
	    return 0;
	}
    }
    if (version != NULL) {
	ss->version = strdup (version);
	if (ss->version == NULL) {
	    *code = ENOMEM;
	    return 0;
	}
    }
    if (info != NULL) {
	ss->info = strdup (info);
	if (ss->info == NULL) {
	    *code = ENOMEM;
	    return 0;
	}
    }
    ss->table = table;
    *code = 0;
    return num_subsystems++;
}

void
ss_error (int index, long code, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    com_err_va (subsystems[index].name, code, fmt, ap);
    va_end(ap);
}

void
ss_perror (int index, long code, const char *msg)
{
    ss_error(index, code, "%s", msg);
}

int
ss_execute_command(int index, char **argv)
{
    int argc = 0;
    int ret;

    while(argv[argc++]);
    ret = sl_command(subsystems[index].table, argc, argv);
    if (ret == SL_BADCOMMAND)
	return SS_ET_COMMAND_NOT_FOUND;
    return 0;
}

int
ss_execute_line (int index, const char *line)
{
    char *buf = strdup(line);
    int argc;
    char **argv;
    int ret;
    
    if (buf == NULL)
	return ENOMEM;
    sl_make_argv(buf, &argc, &argv);
    ret = sl_command(subsystems[index].table, argc, argv);
    free(buf);
    if (ret == SL_BADCOMMAND)
	return SS_ET_COMMAND_NOT_FOUND;
    return 0;
}

int
ss_listen (int index)
{
    char *prompt = malloc(strlen(subsystems[index].name) + 3);
    if (prompt == NULL)
	return ENOMEM;

    strcpy(prompt, subsystems[index].name);
    strcat(prompt, ": ");
    sl_loop(subsystems[index].table, prompt);
    free(prompt);
    return 0;
}

int
ss_list_requests(int argc, char **argv /* , int index, void *info */)
{
    sl_help(subsystems[0 /* index */].table, argc, argv);
    return 0;
}

int
ss_quit(int argc, char **argv)
{
    return 1;
}
