/*
 * Copyright (c) 2003 Kungliga Tekniska Högskolan
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
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "krb5_locl.h"
#include <err.h>

RCSID("$Id: test_config.c 15036 2005-04-30 15:19:58Z lha $");

static int
check_config_file(krb5_context context, char *filelist, char **res, int def)
{
    krb5_error_code ret;
    char **pp;
    int i;

    pp = NULL;

    if (def)
	ret = krb5_prepend_config_files_default(filelist, &pp);
    else
	ret = krb5_prepend_config_files(filelist, NULL, &pp);
    
    if (ret)
	krb5_err(context, 1, ret, "prepend_config_files");
    
    for (i = 0; res[i] && pp[i]; i++)
	if (strcmp(pp[i], res[i]) != 0)
	    krb5_errx(context, 1, "'%s' != '%s'", pp[i], res[i]);
    
    if (res[i] != NULL)
	krb5_errx(context, 1, "pp ended before res list");
    
    if (def) {
	char **deflist;
	int j;
	
	ret = krb5_get_default_config_files(&deflist);
	if (ret)
	    krb5_err(context, 1, ret, "get_default_config_files");
	
	for (j = 0 ; pp[i] && deflist[j]; i++, j++)
	    if (strcmp(pp[i], deflist[j]) != 0)
		krb5_errx(context, 1, "'%s' != '%s'", pp[i], deflist[j]);
	
	if (deflist[j] != NULL)
	    krb5_errx(context, 1, "pp ended before def list");
	krb5_free_config_files(deflist);
    }
    
    if (pp[i] != NULL)
	krb5_errx(context, 1, "pp ended after res (and def) list");
    
    krb5_free_config_files(pp);
    
    return 0;
}

char *list0[] =  { "/tmp/foo", NULL };
char *list1[] =  { "/tmp/foo", "/tmp/foo/bar", NULL };
char *list2[] =  { "", NULL };

struct {
    char *fl;
    char **res;
} test[] = {
    { "/tmp/foo", NULL },
    { "/tmp/foo:/tmp/foo/bar", NULL },
    { "", NULL }
};

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    int i;

    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_context %d", ret);

    test[0].res = list0;
    test[1].res = list1;
    test[2].res = list2;

    for (i = 0; i < sizeof(test)/sizeof(*test); i++) {
	check_config_file(context, test[i].fl, test[i].res, 0);
	check_config_file(context, test[i].fl, test[i].res, 1);
    }

    krb5_free_context(context);

    return 0;
}
