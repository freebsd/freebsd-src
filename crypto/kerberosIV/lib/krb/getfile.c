/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

#include "krb_locl.h"

RCSID("$Id: getfile.c,v 1.5.2.1 2000/12/07 17:04:48 assar Exp $");

static int
is_suid(void)
{
    int ret = 0;
#ifdef HAVE_GETUID
    ret |= getuid() != geteuid();
#endif
#ifdef HAVE_GETGID
    ret |= getgid() != getegid();
#endif
    return ret;
}

static int
get_file(const char **files, int num, const char *file, char *buf, size_t len)
{
    const char *p, **q;
    int i = 0;
    if(getuid() != 0 && !is_suid() && (p = getenv("KRBCONFDIR"))){
	if(num == i){
	    snprintf(buf, len, "%s/%s", p, file);
	    return 0;
	}
	i++;
    }
    for(q = files; *q; q++, i++){
	if(num == i){
	    snprintf(buf, len, "%s", *q);
	    return 0;
	}
    }
    return -1;
}

int
krb_get_krbconf(int num, char *buf, size_t len)
{
    const char *files[] = KRB_CNF_FILES;
    return get_file(files, num, "krb.conf", buf, len);
}

int
krb_get_krbrealms(int num, char *buf, size_t len)
{
    const char *files[] = KRB_RLM_FILES;
    return get_file(files, num, "krb.realms", buf, len);
}

int
krb_get_krbextra(int num, char *buf, size_t len)
{
    const char *files[] = { "/etc/krb.extra", NULL };
    return get_file(files, num, "krb.extra", buf, len);
}
