/*
 * Copyright (c) 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software 
 *    must display the following acknowledgement: 
 *      This product includes software developed by Kungliga Tekniska 
 *      Högskolan and its contributors. 
 *
 * 4. Neither the name of the Institute nor the names of its contributors 
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

#if 0
RCSID("$Id: getarg.c,v 1.25 1998/11/22 09:45:05 assar Exp $");
#endif

#include <sys/ttycom.h>
#include <time.h>
#include <stdio.h>
#include "getarg.h"

#define ISFLAG(X) ((X).type == arg_flag || (X).type == arg_negative_flag)

static size_t
print_arg (char *string, size_t len, int mdoc, int longp, struct getargs *arg)
{
    const char *s;

    *string = '\0';

    if (ISFLAG(*arg))
	return 0;

    if(mdoc){
	if(longp)
	    strncat(string, "= Ns", len);
	strncat(string, " Ar ", len);
    }else
	if (longp)
	    strncat (string, "=", len);
	else
	    strncat (string, " ", len);

    if (arg->arg_help)
	s = arg->arg_help;
    else if (arg->type == arg_integer)
	s = "number";
    else if (arg->type == arg_string)
	s = "string";
    else
	s = "<undefined>";

    strncat(string, s, len);
    return 1 + strlen(s);
}

static int
check_column(FILE *f, int col, int len, int columns)
{
    if(col + len > columns) {
	fprintf(f, "\n");
	col = fprintf(f, "  ");
    }
    return col;
}

void
arg_printusage (struct getargs *args,
		size_t num_args,
		const char *progname,
		const char *extra_string)
{
    int i;
    size_t max_len = 0;
    char buf[128];
    int col = 0, columns;
    struct winsize ws;

    columns = 80;
    col = 0;
    col += fprintf (stderr, "Usage: %s", progname);
    for (i = 0; i < num_args; ++i) {
	size_t len = 0;

	if (args[i].long_name) {
	    buf[0] = '\0';
	    strncat(buf, "[--", sizeof(buf));
	    len += 2;
	    if(args[i].type == arg_negative_flag) {
		strncat(buf, "no-", sizeof(buf));
		len += 3;
	    }
	    strncat(buf, args[i].long_name, sizeof(buf));
	    len += strlen(args[i].long_name);
	    len += print_arg(buf + strlen(buf), sizeof(buf) - strlen(buf), 
			     0, 1, &args[i]);
	    strncat(buf, "]", sizeof(buf));
	    if(args[i].type == arg_strings)
		strncat(buf, "...", sizeof(buf));
	    col = check_column(stderr, col, strlen(buf) + 1, columns);
	    col += fprintf(stderr, " %s", buf);
	}
	if (args[i].short_name) {
	    snprintf(buf, sizeof(buf), "[-%c", args[i].short_name);
	    len += 2;
	    len += print_arg(buf + strlen(buf), sizeof(buf) - strlen(buf), 
			     0, 0, &args[i]);
	    strncat(buf, "]", sizeof(buf));
	    if(args[i].type == arg_strings)
		strncat(buf, "...", sizeof(buf));
	    col = check_column(stderr, col, strlen(buf) + 1, columns);
	    col += fprintf(stderr, " %s", buf);
	}
	if (args[i].long_name && args[i].short_name)
	    len += 2; /* ", " */
	max_len = max(max_len, len);
    }
    if (extra_string) {
	col = check_column(stderr, col, strlen(extra_string) + 1, columns);
	fprintf (stderr, " %s\n", extra_string);
    } else
	fprintf (stderr, "\n");
    for (i = 0; i < num_args; ++i) {
	if (args[i].help) {
	    size_t count = 0;

	    if (args[i].short_name) {
		count += fprintf (stderr, "-%c", args[i].short_name);
		print_arg (buf, sizeof(buf), 0, 0, &args[i]);
		count += fprintf(stderr, "%s", buf);
	    }
	    if (args[i].short_name && args[i].long_name)
		count += fprintf (stderr, ", ");
	    if (args[i].long_name) {
		count += fprintf (stderr, "--");
		if (args[i].type == arg_negative_flag)
		    count += fprintf (stderr, "no-");
		count += fprintf (stderr, "%s", args[i].long_name);
		print_arg (buf, sizeof(buf), 0, 1, &args[i]);
		count += fprintf(stderr, "%s", buf);
	    }
	    while(count++ <= max_len)
		putc (' ', stderr);
	    fprintf (stderr, "%s\n", args[i].help);
	}
    }
}

static void
add_string(getarg_strings *s, char *value)
{
    s->strings = realloc(s->strings, (s->num_strings + 1) * sizeof(*s->strings));
    s->strings[s->num_strings] = value;
    s->num_strings++;
}

static int
arg_match_long(struct getargs *args, size_t num_args,
	       char *argv)
{
    int i;
    char *optarg = NULL;
    int negate = 0;
    int partial_match = 0;
    struct getargs *partial = NULL;
    struct getargs *current = NULL;
    int argv_len;
    char *p;

    argv_len = strlen(argv);
    p = strchr (argv, '=');
    if (p != NULL)
	argv_len = p - argv;

    for (i = 0; i < num_args; ++i) {
	if(args[i].long_name) {
	    int len = strlen(args[i].long_name);
	    char *p = argv;
	    int p_len = argv_len;
	    negate = 0;

	    for (;;) {
		if (strncmp (args[i].long_name, p, p_len) == 0) {
		    if(p_len == len)
			current = &args[i];
		    else {
			++partial_match;
			partial = &args[i];
		    }
		    optarg  = p + p_len;
		} else if (ISFLAG(args[i]) && strncmp (p, "no-", 3) == 0) {
		    negate = !negate;
		    p += 3;
		    p_len -= 3;
		    continue;
		}
		break;
	    }
	    if (current)
		break;
	}
    }
    if (current == NULL) {
	if (partial_match == 1)
	    current = partial;
	else
	    return ARG_ERR_NO_MATCH;
    }
    
    if(*optarg == '\0' && !ISFLAG(*current))
	return ARG_ERR_NO_MATCH;
    switch(current->type){
    case arg_integer:
    {
	int tmp;
	if(sscanf(optarg + 1, "%d", &tmp) != 1)
	    return ARG_ERR_BAD_ARG;
	*(int*)current->value = tmp;
	return 0;
    }
    case arg_string:
    {
	*(char**)current->value = optarg + 1;
	return 0;
    }
    case arg_strings:
    {
	add_string((getarg_strings*)current->value, optarg + 1);
	return 0;
    }
    case arg_flag:
    case arg_negative_flag:
    {
	int *flag = current->value;
	if(*optarg == '\0' ||
	   strcmp(optarg + 1, "yes") == 0 || 
	   strcmp(optarg + 1, "true") == 0){
	    *flag = !negate;
	    return 0;
	} else if (*optarg && strcmp(optarg + 1, "maybe") == 0) {
	    *flag = rand() & 1;
	} else {
	    *flag = negate;
	    return 0;
	}
	return ARG_ERR_BAD_ARG;
    }
    default:
	abort ();
    }
}

int
getarg(struct getargs *args, size_t num_args, 
       int argc, char **argv, int *optind)
{
    int i, j, k;
    int ret = 0;

    srand (time(NULL));
    (*optind)++;
    for(i = *optind; i < argc; i++) {
	if(argv[i][0] != '-')
	    break;
	if(argv[i][1] == '-'){
	    if(argv[i][2] == 0){
		i++;
		break;
	    }
	    ret = arg_match_long (args, num_args, argv[i] + 2);
	    if(ret)
		return ret;
	}else{
	    for(j = 1; argv[i][j]; j++) {
		for(k = 0; k < num_args; k++) {
		    char *optarg;
		    if(args[k].short_name == 0)
			continue;
		    if(argv[i][j] == args[k].short_name){
			if(args[k].type == arg_flag){
			    *(int*)args[k].value = 1;
			    break;
			}
			if(args[k].type == arg_negative_flag){
			    *(int*)args[k].value = 0;
			    break;
			}
			if(argv[i][j + 1])
			    optarg = &argv[i][j + 1];
			else{
			    i++;
			    optarg = argv[i];
			}
			if(optarg == NULL)
			    return ARG_ERR_NO_ARG;
			if(args[k].type == arg_integer){
			    int tmp;
			    if(sscanf(optarg, "%d", &tmp) != 1)
				return ARG_ERR_BAD_ARG;
			    *(int*)args[k].value = tmp;
			    goto out;
			}else if(args[k].type == arg_string){
			    *(char**)args[k].value = optarg;
			    goto out;
			}else if(args[k].type == arg_strings){
			    add_string((getarg_strings*)args[k].value, optarg);
			    goto out;
			}
			return ARG_ERR_BAD_ARG;
		    }
			
		}
		if (k == num_args)
		    return ARG_ERR_NO_MATCH;
	    }
	out:;
	}
    }
    *optind = i;
    return 0;
}

#if TEST
int foo_flag = 2;
int flag1 = 0;
int flag2 = 0;
int bar_int;
char *baz_string;

struct getargs args[] = {
    { NULL, '1', arg_flag, &flag1, "one", NULL },
    { NULL, '2', arg_flag, &flag2, "two", NULL },
    { "foo", 'f', arg_negative_flag, &foo_flag, "foo", NULL },
    { "bar", 'b', arg_integer, &bar_int, "bar", "seconds"},
    { "baz", 'x', arg_string, &baz_string, "baz", "name" },
};

int main(int argc, char **argv)
{
    int optind = 0;
    while(getarg(args, 5, argc, argv, &optind))
	printf("Bad arg: %s\n", argv[optind]);
    printf("flag1 = %d\n", flag1);  
    printf("flag2 = %d\n", flag2);  
    printf("foo_flag = %d\n", foo_flag);  
    printf("bar_int = %d\n", bar_int);
    printf("baz_flag = %s\n", baz_string);
    arg_printusage (args, 5, argv[0], "nothing here");
}
#endif
