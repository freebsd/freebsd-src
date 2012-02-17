/*
 * Copyright (c) 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

#include "kadmin_locl.h"
#include <kadm5/private.h>

RCSID("$Id: dump.c,v 1.26 1999/12/02 17:04:58 joda Exp $");

int
dump(int argc, char **argv)
{
    krb5_error_code ret;
    FILE *f;
    HDB *db = _kadm5_s_get_db(kadm_handle);
    int decrypt = 0;
    int optind = 0;

    struct getargs args[] = {
	{ "decrypt", 'd', arg_flag, NULL, "decrypt keys" }
    };
    args[0].value = &decrypt;

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optind)) {
	arg_printusage(args, sizeof(args) / sizeof(args[0]), "kadmin dump", 
		       "[dump-file]");
	return 0;
    }

    argc -= optind;
    argv += optind;
    if(argc < 1)
	f = stdout;
    else
	f = fopen(argv[0], "w");
    
    ret = db->open(context, db, O_RDONLY, 0600);
    if(ret){
	krb5_warn(context, ret, "hdb_open");
	if(f != stdout)
	    fclose(f);
	return 0;
    }

    hdb_foreach(context, db, decrypt ? HDB_F_DECRYPT : 0, hdb_print_entry, f);

    if(f != stdout)
	fclose(f);
    db->close(context, db);
    return 0;
}
