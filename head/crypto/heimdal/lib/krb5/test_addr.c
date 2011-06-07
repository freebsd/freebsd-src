/*
 * Copyright (c) 2005 Kungliga Tekniska Högskolan
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
#include <err.h>

RCSID("$Id: test_addr.c 15036 2005-04-30 15:19:58Z lha $");

static void
print_addr(krb5_context context, const char *addr)
{
    krb5_addresses addresses;
    krb5_error_code ret;
    char buf[38];
    char buf2[1000];
    size_t len;
    int i;

    ret = krb5_parse_address(context, addr, &addresses);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_address");

    if (addresses.len < 1)
	krb5_err(context, 1, ret, "too few addresses");
    
    for (i = 0; i < addresses.len; i++) {
	krb5_print_address(&addresses.val[i], buf, sizeof(buf), &len);
#if 0
	printf("addr %d: %s (%d/%d)\n", i, buf, (int)len, (int)strlen(buf)); 
#endif
	if (strlen(buf) > sizeof(buf))
	    abort();
	krb5_print_address(&addresses.val[i], buf2, sizeof(buf2), &len);
#if 0
	printf("addr %d: %s (%d/%d)\n", i, buf2, (int)len, (int)strlen(buf2)); 
#endif
	if (strlen(buf2) > sizeof(buf2))
	    abort();

    }
    krb5_free_addresses(context, &addresses);

}

static void
truncated_addr(krb5_context context, const char *addr, 
	       size_t truncate_len, size_t outlen)
{
    krb5_addresses addresses;
    krb5_error_code ret;
    char *buf;
    size_t len;

    buf = ecalloc(1, outlen + 1);

    ret = krb5_parse_address(context, addr, &addresses);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_address");

    if (addresses.len != 1)
	krb5_err(context, 1, ret, "addresses should be one");
    
    krb5_print_address(&addresses.val[0], buf, truncate_len, &len);
    
#if 0
    printf("addr %s (%d/%d)\n", buf, (int)len, (int)strlen(buf)); 
#endif
    
    if (truncate_len > strlen(buf) + 1)
	abort();
    if (outlen != len)
	abort();
    
    krb5_print_address(&addresses.val[0], buf, outlen + 1, &len);

#if 0
    printf("addr %s (%d/%d)\n", buf, (int)len, (int)strlen(buf)); 
#endif

    if (len != outlen)
	abort();
    if (strlen(buf) != len)
	abort();

    krb5_free_addresses(context, &addresses);
    free(buf);
}

static void
check_truncation(krb5_context context, const char *addr)
{
    int i, len = strlen(addr);

    for (i = 0; i < len; i++)
	truncated_addr(context, addr, i, len);
}

static void
match_addr(krb5_context context, const char *range_addr, 
	   const char *one_addr, int match)
{
    krb5_addresses range, one;
    krb5_error_code ret;

    ret = krb5_parse_address(context, range_addr, &range);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_address");

    if (range.len != 1)
	krb5_err(context, 1, ret, "wrong num of addresses");
    
    ret = krb5_parse_address(context, one_addr, &one);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_address");

    if (one.len != 1)
	krb5_err(context, 1, ret, "wrong num of addresses");

    if (krb5_address_order(context, &range.val[0], &one.val[0]) == 0) {
	if (!match)
	    krb5_errx(context, 1, "match when one shouldn't be");
    } else {
	if (match)
	    krb5_errx(context, 1, "no match when one should be");
    }

    krb5_free_addresses(context, &range);
    krb5_free_addresses(context, &one);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;

    setprogname(argv[0]);

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    print_addr(context, "RANGE:127.0.0.0/8");
    print_addr(context, "RANGE:127.0.0.0/24");
    print_addr(context, "RANGE:IPv4:127.0.0.0-IPv4:127.0.0.255");
    print_addr(context, "RANGE:130.237.237.4/29");
#ifdef HAVE_IPV6
    print_addr(context, "RANGE:fe80::209:6bff:fea0:e522/64");
    print_addr(context, "RANGE:IPv6:fe80::209:6bff:fea0:e522/64");
    print_addr(context, "RANGE:IPv6:fe80::-IPv6:fe80::ffff:ffff:ffff:ffff");
    print_addr(context, "RANGE:fe80::-fe80::ffff:ffff:ffff:ffff");
#endif

    check_truncation(context, "IPv4:127.0.0.0");
    check_truncation(context, "RANGE:IPv4:127.0.0.0-IPv4:127.0.0.255");
#ifdef HAVE_IPV6
    check_truncation(context, "IPv6:::1");
    check_truncation(context, "IPv6:fe80::ffff:ffff:ffff:ffff");
#endif

    match_addr(context, "RANGE:127.0.0.0/8", "inet:127.0.0.0", 1);
    match_addr(context, "RANGE:127.0.0.0/8", "inet:127.255.255.255", 1);
    match_addr(context, "RANGE:127.0.0.0/8", "inet:128.0.0.0", 0);

    match_addr(context, "RANGE:130.237.237.8/29", "inet:130.237.237.7", 0);
    match_addr(context, "RANGE:130.237.237.8/29", "inet:130.237.237.8", 1);
    match_addr(context, "RANGE:130.237.237.8/29", "inet:130.237.237.15", 1);
    match_addr(context, "RANGE:130.237.237.8/29", "inet:130.237.237.16", 0);

    krb5_free_context(context);

    return 0;
}
