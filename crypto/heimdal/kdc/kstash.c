/*
 * Copyright (c) 1997-1999 Kungliga Tekniska Högskolan
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

#include "headers.h"

RCSID("$Id: kstash.c,v 1.10 1999/11/13 04:14:17 assar Exp $");

krb5_context context;

char *keyfile = HDB_DB_DIR "/m-key";
char *v4_keyfile;
int convert_flag;
int help_flag;
int version_flag;

struct getargs args[] = {
    { "key-file", 'k', arg_string, &keyfile, "master key file", "file" },
    { "version4-key-file", '4', arg_string, &v4_keyfile, 
      "kerberos 4 master key file", "file" },
    { "convert-file", 0, arg_flag, &convert_flag, 
      "convert keytype of keyfile" },
    { "help", 'h', arg_flag, &help_flag },
    { "version", 0, arg_flag, &version_flag }
};

int num_args = sizeof(args) / sizeof(args[0]);

static void
write_keyfile(EncryptionKey key)
{
    FILE *f;
    char buf[1024];
    size_t len;

#ifdef HAVE_UMASK
    umask(077);
#endif

    f = fopen(keyfile, "w");
    if(f == NULL)
	krb5_err(context, 1, errno, "%s", keyfile);
    encode_EncryptionKey((unsigned char *)buf + sizeof(buf) - 1,
			 sizeof(buf), &key, &len);
    fwrite(buf + sizeof(buf) - len, len, 1, f);
    memset(buf, 0, sizeof(buf));
    if(ferror(f)) {
	int e = errno;
	unlink(keyfile);
	krb5_err(context, 1, e, "%s", keyfile);
    }
    fclose(f);
    chmod(keyfile, 0400);
}

static int
convert_file(void)
{
    FILE *f;
    unsigned char buf[1024];
    char *fn;
    size_t len;
    EncryptionKey key;
    krb5_error_code ret;

    f = fopen(keyfile, "r");
    if(f == NULL) {
	krb5_warn(context, errno, "%s", keyfile);
	return 1;
    }
    len = fread(buf, 1, sizeof(buf), f);
    if(ferror(f)) {
	krb5_warn(context, errno, "fread");
	ret = 1;
	goto out1;
    }
    fclose(f);
    ret = decode_EncryptionKey(buf, len, &key, &len);
    memset(buf, 0, sizeof(buf));
    if(ret) {
	krb5_warn(context, ret, "decode_EncryptionKey");
	goto out2;
    }
    if(key.keytype == KEYTYPE_DES)
	key.keytype = ETYPE_DES_CBC_MD5;
    else if(key.keytype == ETYPE_DES_CBC_MD5) {
	krb5_warnx(context, "keyfile already converted");
	ret = 0;
	goto out2;
    } else {
	krb5_warnx(context, "bad encryption key type (%d)", key.keytype);
	ret = 1;
	goto out2;
    }
    asprintf(&fn, "%s.old", keyfile);
    if(fn == NULL) {
	krb5_warn(context, ENOMEM, "malloc");
	ret = 1;
	goto out1;
    }
    if(rename(keyfile, fn) < 0) {
	krb5_warn(context, errno, "rename");
	ret = 1;
	goto out1;
    }
    write_keyfile(key);
    krb5_free_keyblock_contents(context, &key);
    return 0;
out1:
    memset(buf, 0, sizeof(buf));
    return ret ? 1 : 0;
out2:
    krb5_free_keyblock_contents(context, &key);
    return ret ? 1 : 0;
}

int
main(int argc, char **argv)
{
    char buf[1024];
    EncryptionKey key;
    FILE *f;
    
    krb5_program_setup(&context, argc, argv, args, num_args, NULL);

    if(help_flag)
	krb5_std_usage(0, args, num_args);
    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    if(convert_flag)
	exit(convert_file());

    key.keytype = ETYPE_DES_CBC_MD5; /* XXX */
    if(v4_keyfile) {
	f = fopen(v4_keyfile, "r");
	if(f == NULL)
	    krb5_err(context, 1, errno, "fopen(%s)", v4_keyfile);
	key.keyvalue.length = sizeof(des_cblock);
	key.keyvalue.data = malloc(key.keyvalue.length);
	fread(key.keyvalue.data, 1, key.keyvalue.length, f);
	fclose(f);
    } else {
	krb5_salt salt;
	salt.salttype = KRB5_PW_SALT;
	/* XXX better value? */
	salt.saltvalue.data = NULL;
	salt.saltvalue.length = 0;
	if(des_read_pw_string(buf, sizeof(buf), "Master key: ", 1))
	    exit(1);
	krb5_string_to_key_salt(context, key.keytype, buf, salt, &key);
    }
    
    write_keyfile(key);
    krb5_free_keyblock_contents(context, &key);
    exit(0);
}
