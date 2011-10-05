/*
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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

#include <config.h>

#include <stdio.h>

#include <roken.h>
#include <getarg.h>

#include "rand.h"


/*
 *
 */

static int version_flag;
static int help_flag;
static int len = 1024 * 1024;
static char *rand_method;
static char *filename;

static struct getargs args[] = {
    { "length",	0,	arg_integer,	&len,
      "length", NULL },
    { "file",	0,	arg_string,	&filename,
      "file name", NULL },
    { "method",	0,	arg_string,	&rand_method,
      "method", NULL },
    { "version",	0,	arg_flag,	&version_flag,
      "print version", NULL },
    { "help",		0,	arg_flag,	&help_flag,
      NULL, 	NULL }
};

/*
 *
 */

/*
 *
 */

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(args[0]),
		    NULL,
		    "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    int idx = 0;
    char *buffer;
    char path[MAXPATHLEN];

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &idx))
	usage(1);

    if (help_flag)
	usage(0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= idx;
    argv += idx;

    if (argc != 0)
	usage(1);

    buffer = emalloc(len);

    if (rand_method) {
	if (0) {
	}
#ifndef NO_RAND_FORTUNA_METHOD
	else if (strcasecmp(rand_method, "fortuna") == 0)
	    RAND_set_rand_method(RAND_fortuna_method());
#endif
#ifndef NO_RAND_UNIX_METHOD
	else if (strcasecmp(rand_method, "unix") == 0)
	    RAND_set_rand_method(RAND_unix_method());
#endif
#ifndef NO_RAND_EGD_METHOD
	else if (strcasecmp(rand_method, "egd") == 0)
	    RAND_set_rand_method(RAND_egd_method());
#endif
#ifdef WIN32
	else if (strcasecmp(rand_method, "w32crypto") == 0)
	    RAND_set_rand_method(RAND_w32crypto_method());
#endif
	else
	    errx(1, "unknown method %s", rand_method);
    }

    if (RAND_file_name(path, sizeof(path)) == NULL)
	errx(1, "RAND_file_name failed");

    if (RAND_status() != 1)
	errx(1, "random not ready yet");

    if (RAND_bytes(buffer, len) != 1)
	errx(1, "RAND_bytes");

    if (filename)
	rk_dumpdata(filename, buffer, len);

    /* head vs tail */
    if (len >= 100000) {
	int bit, i;
	double res;
	int bits[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	for (i = 0; i < len; i++) {
	    unsigned char c = ((unsigned char *)buffer)[i];
	    for (bit = 0; bit < 8 && c; bit++) {
		if (c & 1)
		    bits[bit]++;
		c = c >> 1;
	    }
	}

	for (bit = 0; bit < 8; bit++) {

	    res = ((double)abs(len - bits[bit] * 2)) / (double)len;
	    if (res > 0.005)
		errx(1, "head%d vs tail%d > 0.5%%%% %lf == %d vs %d",
		     bit, bit, res, len, bits[bit]);

	    printf("head vs tails bit%d: %lf\n", bit, res);
	}
    }

    free(buffer);

    /* test write random file */
    {
	static const char *file = "test.file";
	if (RAND_write_file(file) != 1)
	    errx(1, "RAND_write_file");
	if (RAND_load_file(file, 1024) != 1)
	    errx(1, "RAND_load_file");
	unlink(file);
    }

    return 0;
}
