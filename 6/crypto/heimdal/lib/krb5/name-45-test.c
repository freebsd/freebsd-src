/*
 * Copyright (c) 2002 - 2003 Kungliga Tekniska Högskolan
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

RCSID("$Id: name-45-test.c,v 1.3.2.1 2003/05/06 16:49:14 joda Exp $");

enum { MAX_COMPONENTS = 3 };

static struct testcase {
    const char *v4_name;
    const char *v4_inst;
    const char *v4_realm;

    krb5_realm v5_realm;
    unsigned ncomponents;
    char *comp_val[MAX_COMPONENTS];

    const char *config_file;
    krb5_error_code ret;	/* expected error code from 524 */

    krb5_error_code ret2;	/* expected error code from 425 */
} tests[] = {
    {"", "", "", "", 1, {""}, NULL, 0, 0},
    {"a", "", "", "", 1, {"a"}, NULL, 0, 0},
    {"a", "b", "", "", 2, {"a", "b"}, NULL, 0, 0},
    {"a", "b", "c", "c", 2, {"a", "b"}, NULL, 0, 0},

    {"krbtgt", "FOO.SE", "FOO.SE", "FOO.SE", 2,
     {"krbtgt", "FOO.SE"}, NULL, 0, 0},

    {"foo", "bar", "BAZ", "BAZ", 2,
     {"foo", "bar"}, NULL, 0, 0},
    {"foo", "bar", "BAZ", "BAZ", 2,
     {"foo", "bar"},
     "[libdefaults]\n"
     "	v4_name_convert = {\n"
     "		host = {\n"
     "			foo = foo5\n"
     "		}\n"
     "}\n",
    HEIM_ERR_V4_PRINC_NO_CONV, 0},
    {"foo", "bar", "BAZ", "BAZ", 2,
     {"foo5", "bar.baz"},
     "[realms]\n"
     "  BAZ = {\n"
     "		v4_name_convert = {\n"
     "			host = {\n"
     "				foo = foo5\n"
     "			}\n"
     "		}\n"
     "		v4_instance_convert = {\n"
     "			bar = bar.baz\n"
     "		}\n"
     "  }\n",
     0, 0},

    {"rcmd", "foo", "realm", "realm", 2, {"host", "foo"}, NULL,
     HEIM_ERR_V4_PRINC_NO_CONV, 0},
    {"rcmd", "foo", "realm", "realm", 2, {"host", "foo.realm"},
     "[realms]\n"
     "	realm = {\n"
     "		v4_instance_convert = {\n"
     "			foo = foo.realm\n"
     "		}\n"
     "	}\n",
     0, 0},

    {"pop", "mail0", "NADA.KTH.SE", "NADA.KTH.SE", 2,
     {"pop", "mail0.nada.kth.se"}, "", HEIM_ERR_V4_PRINC_NO_CONV, 0},
    {"pop", "mail0", "NADA.KTH.SE", "NADA.KTH.SE", 2,
     {"pop", "mail0.nada.kth.se"},
     "[realms]\n"
     "	NADA.KTH.SE = {\n"
     "		default_domain = nada.kth.se\n"
     "	}\n",
     0, 0},
    {"pop", "mail0", "NADA.KTH.SE", "NADA.KTH.SE", 2,
     {"pop", "mail0.nada.kth.se"},
     "[libdefaults]\n"
     "	v4_instance_resolve = true\n",
     HEIM_ERR_V4_PRINC_NO_CONV, 0},

    {"rcmd", "hokkigai", "NADA.KTH.SE", "NADA.KTH.SE", 2,
     {"host", "hokkigai.pdc.kth.se"}, "", HEIM_ERR_V4_PRINC_NO_CONV, 0},
    {"rcmd", "hokkigai", "NADA.KTH.SE", "NADA.KTH.SE", 2,
     {"host", "hokkigai.pdc.kth.se"},
     "[libdefaults]\n"
     "	v4_instance_resolve = true\n"
     "[realms]\n"
     "	NADA.KTH.SE = {\n"
     "		v4_name_convert = {\n"
     "			host = {\n"
     "				rcmd = host\n"
     "			}\n"
     "		}\n"
     "		default_domain = pdc.kth.se\n"
     "	}\n",
     0, 0},

    {"0123456789012345678901234567890123456789",
     "0123456789012345678901234567890123456789",
     "0123456789012345678901234567890123456789",
     "0123456789012345678901234567890123456789",
     2, {"0123456789012345678901234567890123456789",
	 "0123456789012345678901234567890123456789"}, NULL,
     0, KRB5_PARSE_MALFORMED},

    {"012345678901234567890123456789012345678",
     "012345678901234567890123456789012345678",
     "012345678901234567890123456789012345678",
     "012345678901234567890123456789012345678",
     2, {"012345678901234567890123456789012345678",
	 "012345678901234567890123456789012345678"}, NULL,
     0, 0},

    {NULL, NULL, NULL, NULL, 0, {NULL}, NULL, 0}
};

int
main(int argc, char **argv)
{
    struct testcase *t;
    krb5_context context;
    krb5_error_code ret;
    int val = 0;

    for (t = tests; t->v4_name; ++t) {
	krb5_principal princ;
	int i;
	char name[40], inst[40], realm[40];
	char printable_princ[256];

	ret = krb5_init_context (&context);
	if (ret)
	    errx (1, "krb5_init_context failed: %d", ret);

	if (t->config_file != NULL) {
	    char template[] = "/tmp/krb5-conf-XXXXXX";
	    int fd = mkstemp(template);
	    char *files[2];

	    if (fd < 0)
		krb5_err (context, 1, errno, "mkstemp %s", template);

	    if (write (fd, t->config_file, strlen(t->config_file))
		!= strlen(t->config_file))
		krb5_err (context, 1, errno, "write %s", template);
	    close (fd);
	    files[0] = template;
	    files[1] = NULL;

	    ret = krb5_set_config_files (context, files);
	    unlink (template);
	    if (ret)
		krb5_err (context, 1, ret, "krb5_set_config_files");
	}

	ret = krb5_425_conv_principal (context,
				       t->v4_name,
				       t->v4_inst,
				       t->v4_realm,
				       &princ);
	if (ret) {
	    if (ret != t->ret) {
		krb5_warn (context, ret,
			   "krb5_425_conv_principal %s.%s@%s",
			   t->v4_name, t->v4_inst, t->v4_realm);
		val = 1;
	    }
	} else {
	    if (t->ret) {
		char *s;
		krb5_unparse_name(context, princ, &s);
		krb5_warnx (context,
			    "krb5_425_conv_principal %s.%s@%s "
			    "passed unexpected: %s",
			    t->v4_name, t->v4_inst, t->v4_realm, s);
		free(s);
		val = 1;
		continue;
	    }
	}

	if (ret)
	    continue;

	if (strcmp (t->v5_realm, princ->realm) != 0) {
	    printf ("wrong realm (\"%s\" should be \"%s\")"
		    " for \"%s.%s@%s\"\n",
		    princ->realm, t->v5_realm,
		    t->v4_name,
		    t->v4_inst,
		    t->v4_realm);
	    val = 1;
	}

	if (t->ncomponents != princ->name.name_string.len) {
	    printf ("wrong number of components (%u should be %u)"
		    " for \"%s.%s@%s\"\n",
		    princ->name.name_string.len, t->ncomponents,
		    t->v4_name,
		    t->v4_inst,
		    t->v4_realm);
	    val = 1;
	} else {
	    for (i = 0; i < t->ncomponents; ++i) {
		if (strcmp(t->comp_val[i],
			   princ->name.name_string.val[i]) != 0) {
		    printf ("bad component %d (\"%s\" should be \"%s\")"
			    " for \"%s.%s@%s\"\n",
			    i,
			    princ->name.name_string.val[i],
			    t->comp_val[i],
			    t->v4_name,
			    t->v4_inst,
			    t->v4_realm);
		    val = 1;
		}
	    }
	}
	ret = krb5_524_conv_principal (context, princ,
				       name, inst, realm);
	if (krb5_unparse_name_fixed(context, princ,
				    printable_princ, sizeof(printable_princ)))
	    strlcpy(printable_princ, "unknown principal",
		    sizeof(printable_princ));
	if (ret) {
	    if (ret != t->ret2) {
		krb5_warn (context, ret,
			   "krb5_524_conv_principal %s", printable_princ);
		val = 1;
	    }
	} else {
	    if (t->ret2) {
		krb5_warnx (context,
			    "krb5_524_conv_principal %s "
			    "passed unexpected", printable_princ);
		val = 1;
		continue;
	    }
	}
	if (ret) {
	    krb5_free_principal (context, princ);
	    continue;
	}

	krb5_free_principal (context, princ);
    }
    return val;
}
