/*
 * Copyright (c) 2002 - 2005 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: make_crypto.c 19477 2006-12-20 19:51:53Z lha $");
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int
main(int argc, char **argv)
{
    char *p;
    FILE *f;
    if(argc != 2) {
	fprintf(stderr, "Usage: make_crypto file\n");
	exit(1);
    }
    if (strcmp(argv[1], "--version") == 0) {
	printf("some version");
	return 0;
    }
    f = fopen(argv[1], "w");
    if(f == NULL) {
	perror(argv[1]);
	exit(1);
    }
    for(p = argv[1]; *p; p++)
	if(!isalnum((unsigned char)*p))
	    *p = '_';
    fprintf(f, "#ifndef __%s__\n", argv[1]);
    fprintf(f, "#define __%s__\n", argv[1]);
#ifdef HAVE_OPENSSL
    fputs("#ifndef OPENSSL_DES_LIBDES_COMPATIBILITY\n", f);
    fputs("#define OPENSSL_DES_LIBDES_COMPATIBILITY\n", f);
    fputs("#endif\n", f);
    fputs("#include <openssl/evp.h>\n", f);
    fputs("#include <openssl/des.h>\n", f);
    fputs("#include <openssl/rc4.h>\n", f);
    fputs("#include <openssl/rc2.h>\n", f);
    fputs("#include <openssl/md2.h>\n", f);
    fputs("#include <openssl/md4.h>\n", f);
    fputs("#include <openssl/md5.h>\n", f);
    fputs("#include <openssl/sha.h>\n", f);
    fputs("#include <openssl/aes.h>\n", f);
    fputs("#include <openssl/ui.h>\n", f);
    fputs("#include <openssl/rand.h>\n", f);
    fputs("#include <openssl/engine.h>\n", f);
    fputs("#include <openssl/pkcs12.h>\n", f);
    fputs("#include <openssl/pem.h>\n", f);
    fputs("#include <openssl/hmac.h>\n", f);
    fputs("#ifndef BN_is_negative\n", f);
    fputs("#define BN_set_negative(bn, flag) ((bn)->neg=(flag)?1:0)\n", f);
    fputs("#define BN_is_negative(bn) ((bn)->neg != 0)\n", f);
    fputs("#endif\n", f);
#else
    fputs("#ifdef KRB5\n", f);
    fputs("#include <krb5-types.h>\n", f);
    fputs("#endif\n", f);
    fputs("#include <hcrypto/evp.h>\n", f);
    fputs("#include <hcrypto/des.h>\n", f);
    fputs("#include <hcrypto/md2.h>\n", f);
    fputs("#include <hcrypto/md4.h>\n", f);
    fputs("#include <hcrypto/md5.h>\n", f);
    fputs("#include <hcrypto/sha.h>\n", f);
    fputs("#include <hcrypto/rc4.h>\n", f);
    fputs("#include <hcrypto/rc2.h>\n", f);
    fputs("#include <hcrypto/aes.h>\n", f);
    fputs("#include <hcrypto/ui.h>\n", f);
    fputs("#include <hcrypto/rand.h>\n", f);
    fputs("#include <hcrypto/engine.h>\n", f);
    fputs("#include <hcrypto/pkcs12.h>\n", f);
    fputs("#include <hcrypto/hmac.h>\n", f);
#endif
    fprintf(f, "#endif /* __%s__ */\n", argv[1]);
    fclose(f);
    exit(0);
}
