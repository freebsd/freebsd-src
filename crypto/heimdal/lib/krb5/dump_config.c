/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "krb5_locl.h"

RCSID("$Id: dump_config.c,v 1.2 1999/10/28 23:22:41 assar Exp $");

/* print contents of krb5.conf */

static void
print_tree(struct krb5_config_binding *b, int level)
{
    if (b == NULL)
	return;

    printf("%*s%s%s%s", level * 4, "", 
	   (level == 0) ? "[" : "", b->name, (level == 0) ? "]" : "");
    if(b->type == krb5_config_list) {
	if(level > 0)
	    printf(" = {");
	printf("\n");
	print_tree(b->u.list, level + 1);
	if(level > 0)
	    printf("%*s}\n", level * 4, "");
    } else if(b->type == krb5_config_string) {
	printf(" = %s\n", b->u.string);
    }
    if(b->next)
	print_tree(b->next, level);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret = krb5_init_context(&context);
    if(ret == 0) {
	print_tree(context->cf, 0);
	return 0;
    }
    return 1;
}
