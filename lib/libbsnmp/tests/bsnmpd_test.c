/*-
 * Copyright (c) 2020 Dell EMC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <bsnmp/asn1.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(sa_19_20_bsnmp_test);
ATF_TC_BODY(sa_19_20_bsnmp_test, tc)
{
	struct asn_buf b = {};
	char test_buf[] = { 0x25, 0x7f };
	enum asn_err err;
	asn_len_t len;
	u_char type;

	b.asn_cptr = test_buf;
	b.asn_len = sizeof(test_buf);

	err = asn_get_header(&b, &type, &len);
	ATF_CHECK_EQ(ASN_ERR_EOBUF, err);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sa_19_20_bsnmp_test);
	return (atf_no_error());
}
