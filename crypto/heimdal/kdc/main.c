/*
 * Copyright (c) 1997-2002 Kungliga Tekniska Högskolan
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

#include "kdc_locl.h"
#ifdef HAVE_UTIL_H
#include <util.h>
#endif

RCSID("$Id: main.c,v 1.27 2002/08/28 21:27:16 joda Exp $");

sig_atomic_t exit_flag = 0;
krb5_context context;

#ifdef HAVE_DAEMON
extern int detach_from_console;
#endif

static RETSIGTYPE
sigterm(int sig)
{
    exit_flag = 1;
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    setprogname(argv[0]);
    
    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    configure(argc, argv);

    if(databases == NULL) {
	db = malloc(sizeof(*db));
	num_db = 1;
	ret = hdb_create(context, &db[0], NULL);
	if(ret)
	    krb5_err(context, 1, ret, "hdb_create %s", HDB_DEFAULT_DB);
	ret = hdb_set_master_keyfile(context, db[0], NULL);
	if (ret)
	    krb5_err(context, 1, ret, "hdb_set_master_keyfile");
    } else {
	struct dbinfo *d;
	int i;
	/* count databases */
	for(d = databases, i = 0; d; d = d->next, i++);
	db = malloc(i * sizeof(*db));
	for(d = databases, num_db = 0; d; d = d->next, num_db++) {
	    ret = hdb_create(context, &db[num_db], d->dbname);
	    if(ret)
		krb5_err(context, 1, ret, "hdb_create %s", d->dbname);
	    ret = hdb_set_master_keyfile(context, db[num_db], d->mkey_file);
	    if (ret)
		krb5_err(context, 1, ret, "hdb_set_master_keyfile");
	}
    }

#ifdef HAVE_SIGACTION
    {
	struct sigaction sa;

	sa.sa_flags = 0;
	sa.sa_handler = sigterm;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
    }
#else
    signal(SIGINT, sigterm);
    signal(SIGTERM, sigterm);
#endif
#ifdef HAVE_DAEMON
    if (detach_from_console)
	daemon(0, 0);
#endif
    pidfile(NULL);
    loop();
    krb5_free_context(context);
    return 0;
}
