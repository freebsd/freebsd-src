/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

#include "der_locl.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <getarg.h>
#include <err.h>

RCSID("$Id: asn1_print.c,v 1.7 2001/02/20 01:44:52 assar Exp $");

static struct et_list *et_list;

const char *class_names[] = {
    "UNIV",			/* 0 */
    "APPL",			/* 1 */
    "CONTEXT",			/* 2 */
    "PRIVATE"			/* 3 */
};

const char *type_names[] = {
    "PRIM",			/* 0 */
    "CONS"			/* 1 */
};

const char *tag_names[] = {
    NULL,			/* 0 */
    NULL,			/* 1 */
    "Integer",			/* 2 */
    "BitString",		/* 3 */
    "OctetString",		/* 4 */
    "Null",			/* 5 */
    "ObjectID",			/* 6 */
    NULL,			/* 7 */
    NULL,			/* 8 */
    NULL,			/* 9 */
    NULL,			/* 10 */
    NULL,			/* 11 */
    NULL,			/* 12 */
    NULL,			/* 13 */
    NULL,			/* 14 */
    NULL,			/* 15 */
    "Sequence",			/* 16 */
    "Set",			/* 17 */
    NULL,			/* 18 */
    "PrintableString",		/* 19 */
    NULL,			/* 20 */
    NULL,			/* 21 */
    "IA5String",		/* 22 */
    "UTCTime",			/* 23 */
    "GeneralizedTime",		/* 24 */
    NULL,			/* 25 */
    "VisibleString",		/* 26 */
    "GeneralString"		/* 27 */
};

static int
loop (unsigned char *buf, size_t len, int indent)
{
    while (len > 0) {
	int ret;
	Der_class class;
	Der_type type;
	int tag;
	size_t sz;
	size_t length;
	int i;

	ret = der_get_tag (buf, len, &class, &type, &tag, &sz);
	if (ret)
	    errx (1, "der_get_tag: %s", com_right (et_list, ret));
	if (sz > len)
	    errx (1, "unreasonable length (%u) > %u",
		  (unsigned)sz, (unsigned)len);
	buf += sz;
	len -= sz;
	for (i = 0; i < indent; ++i)
	    printf (" ");
	printf ("%s %s ", class_names[class], type_names[type]);
	if (tag_names[tag])
	    printf ("%s = ", tag_names[tag]);
	else
	    printf ("tag %d = ", tag);
	ret = der_get_length (buf, len, &length, &sz);
	if (ret)
	    errx (1, "der_get_tag: %s", com_right (et_list, ret));
	buf += sz;
	len -= sz;

	if (class == CONTEXT) {
	    printf ("[%d]\n", tag);
	    loop (buf, length, indent);
	} else if (class == UNIV) {
	    switch (tag) {
	    case UT_Sequence :
		printf ("{\n");
		loop (buf, length, indent + 2);
		for (i = 0; i < indent; ++i)
		    printf (" ");
		printf ("}\n");
		break;
	    case UT_Integer : {
		int val;

		ret = der_get_int (buf, length, &val, NULL);
		if (ret)
		    errx (1, "der_get_int: %s", com_right (et_list, ret));
		printf ("integer %d\n", val);
		break;
	    }
	    case UT_OctetString : {
		octet_string str;
		int i;
		unsigned char *uc;

		ret = der_get_octet_string (buf, length, &str, NULL);
		if (ret)
		    errx (1, "der_get_octet_string: %s",
			  com_right (et_list, ret));
		printf ("(length %d), ", length);
		uc = (unsigned char *)str.data;
		for (i = 0; i < 16; ++i)
		    printf ("%02x", uc[i]);
		printf ("\n");
		free (str.data);
		break;
	    }
	    case UT_GeneralizedTime :
	    case UT_GeneralString : {
		general_string str;

		ret = der_get_general_string (buf, length, &str, NULL);
		if (ret)
		    errx (1, "der_get_general_string: %s",
			  com_right (et_list, ret));
		printf ("\"%s\"\n", str);
		free (str);
		break;
	    }
	    default :
		printf ("%d bytes\n", length);
		break;
	    }
	}
	buf += length;
	len -= length;
    }
    return 0;
}

static int
doit (const char *filename)
{
    int fd = open (filename, O_RDONLY);
    struct stat sb;
    unsigned char *buf;
    size_t len;
    int ret;

    if(fd < 0)
	err (1, "opening %s for read", filename);
    if (fstat (fd, &sb) < 0)
	err (1, "stat %s", filename);
    len = sb.st_size;
    buf = malloc (len);
    if (buf == NULL)
	err (1, "malloc %u", len);
    if (read (fd, buf, len) != len)
	errx (1, "read failed");
    close (fd);
    ret = loop (buf, len, 0);
    free (buf);
    return ret;
}


static int version_flag;
static int help_flag;
struct getargs args[] = {
    { "version", 0, arg_flag, &version_flag },
    { "help", 0, arg_flag, &help_flag }
};
int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int code)
{
    arg_printusage(args, num_args, NULL, "dump-file");
    exit(code);
}

int
main(int argc, char **argv)
{
    int optind = 0;

    setprogname (argv[0]);
    initialize_asn1_error_table_r (&et_list);
    if(getarg(args, num_args, argc, argv, &optind))
	usage(1);
    if(help_flag)
	usage(0);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }
    argv += optind;
    argc -= optind;
    if (argc != 1)
	usage (1);
    return doit (argv[0]);
}
