/*
 * Copyright (c) 2002 Kungliga Tekniska Högskolan
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
RCSID("$Id");
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
    f = fopen(argv[1], "w");
    if(f == NULL) {
	perror(argv[1]);
	exit(1);
    }
    for(p = argv[1]; *p; p++)
	if(!isalnum((int)*p))
	    *p = '_';
    fprintf(f, "#ifndef __%s__\n", argv[1]);
    fprintf(f, "#define __%s__\n", argv[1]);
#ifdef HAVE_OPENSSL
    fputs("#include <openssl/des.h>\n", f);
    fputs("#include <openssl/rc4.h>\n", f);
    fputs("#include <openssl/md4.h>\n", f);
    fputs("#include <openssl/md5.h>\n", f);
    fputs("#include <openssl/sha.h>\n", f);
#else
    fputs("#include <des.h>\n", f);
    fputs("#include <md4.h>\n", f);
    fputs("#include <md5.h>\n", f);
    fputs("#include <sha.h>\n", f);
    fputs("#include <rc4.h>\n", f);
#ifdef HAVE_OLD_HASH_NAMES
    fputs("\n", f);
    fputs("    typedef struct md4 MD4_CTX;\n", f);
    fputs("#define MD4_Init md4_init\n", f);
    fputs("#define MD4_Update md4_update\n", f);
    fputs("#define MD4_Final(D, C) md4_finito((C), (D))\n", f);
    fputs("\n", f);
    fputs("    typedef struct md5 MD5_CTX;\n", f);
    fputs("#define MD5_Init md5_init\n", f);
    fputs("#define MD5_Update md5_update\n", f);
    fputs("#define MD5_Final(D, C) md5_finito((C), (D))\n", f);
    fputs("\n", f);
    fputs("    typedef struct sha SHA_CTX;\n", f);
    fputs("#define SHA1_Init sha_init\n", f);
    fputs("#define SHA1_Update sha_update\n", f);
    fputs("#define SHA1_Final(D, C) sha_finito((C), (D))\n", f);
#endif
#endif
    fprintf(f, "#endif /* __%s__ */\n", argv[1]);
    fclose(f);
    exit(0);
}
