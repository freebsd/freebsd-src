/*
 * Copyright (c) 1999 - 2005 Kungliga Tekniska Högskolan
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
#endif
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <roken.h>

#include <asn1-common.h>
#include <asn1_err.h>
#include <der.h>
#include <krb5_asn1.h>
#include <heim_asn1.h>
#include <rfc2459_asn1.h>
#include <test_asn1.h>

#include "check-common.h"

RCSID("$Id: check-gen.c 21539 2007-07-14 16:12:04Z lha $");

static char *lha_principal[] = { "lha" };
static char *lharoot_princ[] = { "lha", "root" };
static char *datan_princ[] = { "host", "nutcracker.e.kth.se" };
static char *nada_tgt_principal[] = { "krbtgt", "NADA.KTH.SE" };


#define IF_OPT_COMPARE(ac,bc,e) \
	if (((ac)->e == NULL && (bc)->e != NULL) || (((ac)->e != NULL && (bc)->e == NULL))) return 1; if ((ab)->e)
#define COMPARE_OPT_STRING(ac,bc,e) \
	do { if (strcmp(*(ac)->e, *(bc)->e) != 0) return 1; } while(0)
#define COMPARE_OPT_OCTECT_STRING(ac,bc,e) \
	do { if ((ac)->e->length != (bc)->e->length || memcmp((ac)->e->data, (bc)->e->data, (ac)->e->length) != 0) return 1; } while(0)
#define COMPARE_STRING(ac,bc,e) \
	do { if (strcmp((ac)->e, (bc)->e) != 0) return 1; } while(0)
#define COMPARE_INTEGER(ac,bc,e) \
	do { if ((ac)->e != (bc)->e) return 1; } while(0)
#define COMPARE_MEM(ac,bc,e,len) \
	do { if (memcmp((ac)->e, (bc)->e,len) != 0) return 1; } while(0)

static int
cmp_principal (void *a, void *b)
{
    Principal *pa = a;
    Principal *pb = b;
    int i;

    COMPARE_STRING(pa,pb,realm);
    COMPARE_INTEGER(pa,pb,name.name_type);
    COMPARE_INTEGER(pa,pb,name.name_string.len);

    for (i = 0; i < pa->name.name_string.len; i++)
	COMPARE_STRING(pa,pb,name.name_string.val[i]);

    return 0;
}

static int
test_principal (void)
{

    struct test_case tests[] = {
	{ NULL, 29, 
	  "\x30\x1b\xa0\x10\x30\x0e\xa0\x03\x02\x01\x01\xa1\x07\x30\x05\x1b"
	  "\x03\x6c\x68\x61\xa1\x07\x1b\x05\x53\x55\x2e\x53\x45"
	},
	{ NULL, 35,
	  "\x30\x21\xa0\x16\x30\x14\xa0\x03\x02\x01\x01\xa1\x0d\x30\x0b\x1b"
	  "\x03\x6c\x68\x61\x1b\x04\x72\x6f\x6f\x74\xa1\x07\x1b\x05\x53\x55"
	  "\x2e\x53\x45"
	},
	{ NULL, 54, 
	  "\x30\x34\xa0\x26\x30\x24\xa0\x03\x02\x01\x03\xa1\x1d\x30\x1b\x1b"
	  "\x04\x68\x6f\x73\x74\x1b\x13\x6e\x75\x74\x63\x72\x61\x63\x6b\x65"
	  "\x72\x2e\x65\x2e\x6b\x74\x68\x2e\x73\x65\xa1\x0a\x1b\x08\x45\x2e"
	  "\x4b\x54\x48\x2e\x53\x45"
	}
    };


    Principal values[] = { 
	{ { KRB5_NT_PRINCIPAL, { 1, lha_principal } },  "SU.SE" },
	{ { KRB5_NT_PRINCIPAL, { 2, lharoot_princ } },  "SU.SE" },
	{ { KRB5_NT_SRV_HST, { 2, datan_princ } },  "E.KTH.SE" }
    };
    int i, ret;
    int ntests = sizeof(tests) / sizeof(*tests);

    for (i = 0; i < ntests; ++i) {
	tests[i].val = &values[i];
	asprintf (&tests[i].name, "Principal %d", i);
    }

    ret = generic_test (tests, ntests, sizeof(Principal),
			(generic_encode)encode_Principal,
			(generic_length)length_Principal,
			(generic_decode)decode_Principal,
			(generic_free)free_Principal,
			cmp_principal);
    for (i = 0; i < ntests; ++i)
	free (tests[i].name);

    return ret;
}

static int
cmp_authenticator (void *a, void *b)
{
    Authenticator *aa = a;
    Authenticator *ab = b;
    int i;

    COMPARE_INTEGER(aa,ab,authenticator_vno);
    COMPARE_STRING(aa,ab,crealm);

    COMPARE_INTEGER(aa,ab,cname.name_type);
    COMPARE_INTEGER(aa,ab,cname.name_string.len);

    for (i = 0; i < aa->cname.name_string.len; i++)
	COMPARE_STRING(aa,ab,cname.name_string.val[i]);

    return 0;
}

static int
test_authenticator (void)
{
    struct test_case tests[] = {
	{ NULL, 63, 
	  "\x62\x3d\x30\x3b\xa0\x03\x02\x01\x05\xa1\x0a\x1b\x08"
	  "\x45\x2e\x4b\x54\x48\x2e\x53\x45\xa2\x10\x30\x0e\xa0"
	  "\x03\x02\x01\x01\xa1\x07\x30\x05\x1b\x03\x6c\x68\x61"
	  "\xa4\x03\x02\x01\x0a\xa5\x11\x18\x0f\x31\x39\x37\x30"
	  "\x30\x31\x30\x31\x30\x30\x30\x31\x33\x39\x5a"
	},
	{ NULL, 67, 
	  "\x62\x41\x30\x3f\xa0\x03\x02\x01\x05\xa1\x07\x1b\x05"
	  "\x53\x55\x2e\x53\x45\xa2\x16\x30\x14\xa0\x03\x02\x01"
	  "\x01\xa1\x0d\x30\x0b\x1b\x03\x6c\x68\x61\x1b\x04\x72"
	  "\x6f\x6f\x74\xa4\x04\x02\x02\x01\x24\xa5\x11\x18\x0f"
	  "\x31\x39\x37\x30\x30\x31\x30\x31\x30\x30\x31\x36\x33"
	  "\x39\x5a"
	}
    };

    Authenticator values[] = {
	{ 5, "E.KTH.SE", { KRB5_NT_PRINCIPAL, { 1, lha_principal } },
	  NULL, 10, 99, NULL, NULL, NULL },
	{ 5, "SU.SE", { KRB5_NT_PRINCIPAL, { 2, lharoot_princ } },
	  NULL, 292, 999, NULL, NULL, NULL }
    };
    int i, ret;
    int ntests = sizeof(tests) / sizeof(*tests);

    for (i = 0; i < ntests; ++i) {
	tests[i].val = &values[i];
	asprintf (&tests[i].name, "Authenticator %d", i);
    }

    ret = generic_test (tests, ntests, sizeof(Authenticator),
			(generic_encode)encode_Authenticator,
			(generic_length)length_Authenticator,
			(generic_decode)decode_Authenticator,
			(generic_free)free_Authenticator,
			cmp_authenticator);
    for (i = 0; i < ntests; ++i)
	free(tests[i].name);

    return ret;
}

static int
cmp_KRB_ERROR (void *a, void *b)
{
    KRB_ERROR *aa = a;
    KRB_ERROR *ab = b;
    int i;

    COMPARE_INTEGER(aa,ab,pvno);
    COMPARE_INTEGER(aa,ab,msg_type);

    IF_OPT_COMPARE(aa,ab,ctime) {
	COMPARE_INTEGER(aa,ab,ctime);
    }
    IF_OPT_COMPARE(aa,ab,cusec) {
	COMPARE_INTEGER(aa,ab,cusec);
    }
    COMPARE_INTEGER(aa,ab,stime);
    COMPARE_INTEGER(aa,ab,susec);
    COMPARE_INTEGER(aa,ab,error_code);

    IF_OPT_COMPARE(aa,ab,crealm) {
	COMPARE_OPT_STRING(aa,ab,crealm);
    }
#if 0
    IF_OPT_COMPARE(aa,ab,cname) {
	COMPARE_OPT_STRING(aa,ab,cname);
    }
#endif
    COMPARE_STRING(aa,ab,realm);

    COMPARE_INTEGER(aa,ab,sname.name_string.len);
    for (i = 0; i < aa->sname.name_string.len; i++)
	COMPARE_STRING(aa,ab,sname.name_string.val[i]);

    IF_OPT_COMPARE(aa,ab,e_text) {
	COMPARE_OPT_STRING(aa,ab,e_text);
    }
    IF_OPT_COMPARE(aa,ab,e_data) {
	/* COMPARE_OPT_OCTECT_STRING(aa,ab,e_data); */
    }

    return 0;
}

static int
test_krb_error (void)
{
    struct test_case tests[] = {
	{ NULL, 127, 
	  "\x7e\x7d\x30\x7b\xa0\x03\x02\x01\x05\xa1\x03\x02\x01\x1e\xa4\x11"
	  "\x18\x0f\x32\x30\x30\x33\x31\x31\x32\x34\x30\x30\x31\x31\x31\x39"
	  "\x5a\xa5\x05\x02\x03\x04\xed\xa5\xa6\x03\x02\x01\x1f\xa7\x0d\x1b"
	  "\x0b\x4e\x41\x44\x41\x2e\x4b\x54\x48\x2e\x53\x45\xa8\x10\x30\x0e"
	  "\xa0\x03\x02\x01\x01\xa1\x07\x30\x05\x1b\x03\x6c\x68\x61\xa9\x0d"
	  "\x1b\x0b\x4e\x41\x44\x41\x2e\x4b\x54\x48\x2e\x53\x45\xaa\x20\x30"
	  "\x1e\xa0\x03\x02\x01\x01\xa1\x17\x30\x15\x1b\x06\x6b\x72\x62\x74"
	  "\x67\x74\x1b\x0b\x4e\x41\x44\x41\x2e\x4b\x54\x48\x2e\x53\x45",
	  "KRB-ERROR Test 1"
	}
    };
    int ntests = sizeof(tests) / sizeof(*tests);
    KRB_ERROR e1;
    PrincipalName lhaprincipalname = { 1, { 1, lha_principal } };
    PrincipalName tgtprincipalname = { 1, { 2, nada_tgt_principal } };
    char *realm = "NADA.KTH.SE";

    e1.pvno = 5;
    e1.msg_type = 30;
    e1.ctime = NULL;
    e1.cusec = NULL;
    e1.stime = 1069632679;
    e1.susec = 322981;
    e1.error_code = 31;
    e1.crealm = &realm;
    e1.cname = &lhaprincipalname;
    e1.realm = "NADA.KTH.SE";
    e1.sname = tgtprincipalname;
    e1.e_text = NULL;
    e1.e_data = NULL;

    tests[0].val = &e1;

    return generic_test (tests, ntests, sizeof(KRB_ERROR),
			 (generic_encode)encode_KRB_ERROR,
			 (generic_length)length_KRB_ERROR,
			 (generic_decode)decode_KRB_ERROR,
			 (generic_free)free_KRB_ERROR,
			 cmp_KRB_ERROR);
}

static int
cmp_Name (void *a, void *b)
{
    Name *aa = a;
    Name *ab = b;

    COMPARE_INTEGER(aa,ab,element);

    return 0;
}

static int
test_Name (void)
{
    struct test_case tests[] = {
	{ NULL, 35, 
	  "\x30\x21\x31\x1f\x30\x0b\x06\x03\x55\x04\x03\x13\x04\x4c\x6f\x76"
	  "\x65\x30\x10\x06\x03\x55\x04\x07\x13\x09\x53\x54\x4f\x43\x4b\x48"
	  "\x4f\x4c\x4d",
	  "Name CN=Love+L=STOCKHOLM"
	},
	{ NULL, 35, 
	  "\x30\x21\x31\x1f\x30\x0b\x06\x03\x55\x04\x03\x13\x04\x4c\x6f\x76"
	  "\x65\x30\x10\x06\x03\x55\x04\x07\x13\x09\x53\x54\x4f\x43\x4b\x48"
	  "\x4f\x4c\x4d",
	  "Name L=STOCKHOLM+CN=Love"
	}
    };

    int ntests = sizeof(tests) / sizeof(*tests);
    Name n1, n2;
    RelativeDistinguishedName rdn1[1];
    RelativeDistinguishedName rdn2[1];
    AttributeTypeAndValue atv1[2];
    AttributeTypeAndValue atv2[2];
    unsigned cmp_CN[] = { 2, 5, 4, 3 };
    unsigned cmp_L[] = { 2, 5, 4, 7 };

    /* n1 */
    n1.element = choice_Name_rdnSequence;
    n1.u.rdnSequence.val = rdn1;
    n1.u.rdnSequence.len = sizeof(rdn1)/sizeof(rdn1[0]);
    rdn1[0].val = atv1;
    rdn1[0].len = sizeof(atv1)/sizeof(atv1[0]);

    atv1[0].type.length = sizeof(cmp_CN)/sizeof(cmp_CN[0]);
    atv1[0].type.components = cmp_CN;
    atv1[0].value.element = choice_DirectoryString_printableString;
    atv1[0].value.u.printableString = "Love";

    atv1[1].type.length = sizeof(cmp_L)/sizeof(cmp_L[0]);
    atv1[1].type.components = cmp_L;
    atv1[1].value.element = choice_DirectoryString_printableString;
    atv1[1].value.u.printableString = "STOCKHOLM";

    /* n2 */
    n2.element = choice_Name_rdnSequence;
    n2.u.rdnSequence.val = rdn2;
    n2.u.rdnSequence.len = sizeof(rdn2)/sizeof(rdn2[0]);
    rdn2[0].val = atv2;
    rdn2[0].len = sizeof(atv2)/sizeof(atv2[0]);

    atv2[0].type.length = sizeof(cmp_L)/sizeof(cmp_L[0]);
    atv2[0].type.components = cmp_L;
    atv2[0].value.element = choice_DirectoryString_printableString;
    atv2[0].value.u.printableString = "STOCKHOLM";

    atv2[1].type.length = sizeof(cmp_CN)/sizeof(cmp_CN[0]);
    atv2[1].type.components = cmp_CN;
    atv2[1].value.element = choice_DirectoryString_printableString;
    atv2[1].value.u.printableString = "Love";

    /* */
    tests[0].val = &n1;
    tests[1].val = &n2;

    return generic_test (tests, ntests, sizeof(Name),
			 (generic_encode)encode_Name,
			 (generic_length)length_Name,
			 (generic_decode)decode_Name,
			 (generic_free)free_Name,
			 cmp_Name);
}

static int
cmp_KeyUsage (void *a, void *b)
{
    KeyUsage *aa = a;
    KeyUsage *ab = b;

    return KeyUsage2int(*aa) != KeyUsage2int(*ab);
}

static int
test_bit_string (void)
{
    struct test_case tests[] = {
	{ NULL, 4,
	  "\x03\x02\x07\x80",
	  "bitstring 1"
	},
	{ NULL, 4,
	  "\x03\x02\x05\xa0",
	  "bitstring 2"
	},
	{ NULL, 5,
	  "\x03\x03\x07\x00\x80",
	  "bitstring 3"
	},
	{ NULL, 3,
	  "\x03\x01\x00",
	  "bitstring 4"
	}
    };

    int ntests = sizeof(tests) / sizeof(*tests);
    KeyUsage ku1, ku2, ku3, ku4;

    memset(&ku1, 0, sizeof(ku1));
    ku1.digitalSignature = 1;
    tests[0].val = &ku1;

    memset(&ku2, 0, sizeof(ku2));
    ku2.digitalSignature = 1;
    ku2.keyEncipherment = 1;
    tests[1].val = &ku2;

    memset(&ku3, 0, sizeof(ku3));
    ku3.decipherOnly = 1;
    tests[2].val = &ku3;

    memset(&ku4, 0, sizeof(ku4));
    tests[3].val = &ku4;


    return generic_test (tests, ntests, sizeof(KeyUsage),
			 (generic_encode)encode_KeyUsage,
			 (generic_length)length_KeyUsage,
			 (generic_decode)decode_KeyUsage,
			 (generic_free)free_KeyUsage,
			 cmp_KeyUsage);
}

static int
cmp_TESTLargeTag (void *a, void *b)
{
    TESTLargeTag *aa = a;
    TESTLargeTag *ab = b;

    COMPARE_INTEGER(aa,ab,foo);
    return 0;
}

static int
test_large_tag (void)
{
    struct test_case tests[] = {
	{ NULL,  8,  "\x30\x06\xbf\x7f\x03\x02\x01\x01", "large tag 1" }
    };

    int ntests = sizeof(tests) / sizeof(*tests);
    TESTLargeTag lt1;

    memset(&lt1, 0, sizeof(lt1));
    lt1.foo = 1;

    tests[0].val = &lt1;

    return generic_test (tests, ntests, sizeof(TESTLargeTag),
			 (generic_encode)encode_TESTLargeTag,
			 (generic_length)length_TESTLargeTag,
			 (generic_decode)decode_TESTLargeTag,
			 (generic_free)free_TESTLargeTag,
			 cmp_TESTLargeTag);
}

struct test_data {
    int ok;
    size_t len;
    size_t expected_len;
    void *data;
};

static int
check_tag_length(void)
{
    struct test_data td[] = {
	{ 1, 3, 3, "\x02\x01\x00"},
	{ 1, 3, 3, "\x02\x01\x7f"},
	{ 1, 4, 4, "\x02\x02\x00\x80"},
	{ 1, 4, 4, "\x02\x02\x01\x00"},
	{ 1, 4, 4, "\x02\x02\x02\x00"},
	{ 0, 3, 0, "\x02\x02\x00"},
	{ 0, 3, 0, "\x02\x7f\x7f"},
	{ 0, 4, 0, "\x02\x03\x00\x80"},
	{ 0, 4, 0, "\x02\x7f\x01\x00"},
	{ 0, 5, 0, "\x02\xff\x7f\x02\x00"}
    };
    size_t sz;
    krb5uint32 values[] = {0, 127, 128, 256, 512,
			 0, 127, 128, 256, 512 };
    krb5uint32 u;
    int i, ret, failed = 0;
    void *buf;

    for (i = 0; i < sizeof(td)/sizeof(td[0]); i++) {
	struct map_page *page;

	buf = map_alloc(OVERRUN, td[i].data, td[i].len, &page);

	ret = decode_krb5uint32(buf, td[i].len, &u, &sz);
	if (ret) {
	    if (td[i].ok) {
		printf("failed with tag len test %d\n", i);
		failed = 1;
	    }
	} else {
	    if (td[i].ok == 0) {
		printf("failed with success for tag len test %d\n", i);
		failed = 1;
	    }
	    if (td[i].expected_len != sz) {
		printf("wrong expected size for tag test %d\n", i);
		failed = 1;
	    }
	    if (values[i] != u) {
		printf("wrong value for tag test %d\n", i);
		failed = 1;
	    }
	}
	map_free(page, "test", "decode");
    }
    return failed;
}

static int
cmp_TESTChoice (void *a, void *b)
{
    return 0;
}

static int
test_choice (void)
{
    struct test_case tests[] = {
	{ NULL,  5,  "\xa1\x03\x02\x01\x01", "large choice 1" },
	{ NULL,  5,  "\xa2\x03\x02\x01\x02", "large choice 2" }
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTChoice1 c1;
    TESTChoice1 c2_1;
    TESTChoice2 c2_2;

    memset(&c1, 0, sizeof(c1));
    c1.element = choice_TESTChoice1_i1;
    c1.u.i1 = 1;
    tests[0].val = &c1;

    memset(&c2_1, 0, sizeof(c2_1));
    c2_1.element = choice_TESTChoice1_i2;
    c2_1.u.i2 = 2;
    tests[1].val = &c2_1;

    ret += generic_test (tests, ntests, sizeof(TESTChoice1),
			 (generic_encode)encode_TESTChoice1,
			 (generic_length)length_TESTChoice1,
			 (generic_decode)decode_TESTChoice1,
			 (generic_free)free_TESTChoice1,
			 cmp_TESTChoice);

    memset(&c2_2, 0, sizeof(c2_2));
    c2_2.element = choice_TESTChoice2_asn1_ellipsis;
    c2_2.u.asn1_ellipsis.data = "\xa2\x03\x02\x01\x02";
    c2_2.u.asn1_ellipsis.length = 5;
    tests[1].val = &c2_2;

    ret += generic_test (tests, ntests, sizeof(TESTChoice2),
			 (generic_encode)encode_TESTChoice2,
			 (generic_length)length_TESTChoice2,
			 (generic_decode)decode_TESTChoice2,
			 (generic_free)free_TESTChoice2,
			 cmp_TESTChoice);

    return ret;
}

static int
cmp_TESTImplicit (void *a, void *b)
{
    TESTImplicit *aa = a;
    TESTImplicit *ab = b;

    COMPARE_INTEGER(aa,ab,ti1);
    COMPARE_INTEGER(aa,ab,ti2.foo);
    COMPARE_INTEGER(aa,ab,ti3);
    return 0;
}

/*
UNIV CONS Sequence 14
  CONTEXT PRIM 0 1 00
  CONTEXT CONS 1 6
   CONTEXT CONS 127 3
     UNIV PRIM Integer 1 02
  CONTEXT PRIM 2 1 03
*/

static int
test_implicit (void)
{
    struct test_case tests[] = {
	{ NULL,  16,  
	  "\x30\x0e\x80\x01\x00\xa1\x06\xbf"
	  "\x7f\x03\x02\x01\x02\x82\x01\x03", 
	  "implicit 1" }
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTImplicit c0;

    memset(&c0, 0, sizeof(c0));
    c0.ti1 = 0;
    c0.ti2.foo = 2;
    c0.ti3 = 3;
    tests[0].val = &c0;

    ret += generic_test (tests, ntests, sizeof(TESTImplicit),
			 (generic_encode)encode_TESTImplicit,
			 (generic_length)length_TESTImplicit,
			 (generic_decode)decode_TESTImplicit,
			 (generic_free)free_TESTImplicit,
			 cmp_TESTImplicit);

#ifdef IMPLICIT_TAGGING_WORKS
    ret += generic_test (tests, ntests, sizeof(TESTImplicit2),
			 (generic_encode)encode_TESTImplicit2,
			 (generic_length)length_TESTImplicit2,
			 (generic_decode)decode_TESTImplicit2,
			 (generic_free)free_TESTImplicit2,
			 cmp_TESTImplicit);

#endif /* IMPLICIT_TAGGING_WORKS */
    return ret;
}

static int
cmp_TESTAlloc (void *a, void *b)
{
    TESTAlloc *aa = a;
    TESTAlloc *ab = b;

    IF_OPT_COMPARE(aa,ab,tagless) {
	COMPARE_INTEGER(aa,ab,tagless->ai);
    }

    COMPARE_INTEGER(aa,ab,three);

    IF_OPT_COMPARE(aa,ab,tagless2) {
	COMPARE_OPT_OCTECT_STRING(aa, ab, tagless2);
    }

    return 0;
}

/*
UNIV CONS Sequence 12
  UNIV CONS Sequence 5
    CONTEXT CONS 0 3
      UNIV PRIM Integer 1 01
  CONTEXT CONS 1 3
    UNIV PRIM Integer 1 03

UNIV CONS Sequence 5
  CONTEXT CONS 1 3
    UNIV PRIM Integer 1 03

UNIV CONS Sequence 8
  CONTEXT CONS 1 3
    UNIV PRIM Integer 1 04
  UNIV PRIM Integer 1 05

*/

static int
test_taglessalloc (void)
{
    struct test_case tests[] = {
	{ NULL,  14,  
	  "\x30\x0c\x30\x05\xa0\x03\x02\x01\x01\xa1\x03\x02\x01\x03", 
	  "alloc 1" },
	{ NULL,  7,  
	  "\x30\x05\xa1\x03\x02\x01\x03", 
	  "alloc 2" },
	{ NULL,  10,  
	  "\x30\x08\xa1\x03\x02\x01\x04\x02\x01\x05", 
	  "alloc 3" }
    };

    int ret = 0, ntests = sizeof(tests) / sizeof(*tests);
    TESTAlloc c1, c2, c3;
    heim_any any3;

    memset(&c1, 0, sizeof(c1));
    c1.tagless = ecalloc(1, sizeof(*c1.tagless));
    c1.tagless->ai = 1;
    c1.three = 3;
    tests[0].val = &c1;

    memset(&c2, 0, sizeof(c2));
    c2.tagless = NULL;
    c2.three = 3;
    tests[1].val = &c2;

    memset(&c3, 0, sizeof(c3));
    c3.tagless = NULL;
    c3.three = 4;
    c3.tagless2 = &any3;
    any3.data = "\x02\x01\x05";
    any3.length = 3;
    tests[2].val = &c3;

    ret += generic_test (tests, ntests, sizeof(TESTAlloc),
			 (generic_encode)encode_TESTAlloc,
			 (generic_length)length_TESTAlloc,
			 (generic_decode)decode_TESTAlloc,
			 (generic_free)free_TESTAlloc,
			 cmp_TESTAlloc);

    free(c1.tagless);

    return ret;
}


static int
check_fail_largetag(void)
{
    struct test_case tests[] = {
	{NULL, 14, "\x30\x0c\xbf\x87\xff\xff\xff\xff\xff\x7f\x03\x02\x01\x01",
	 "tag overflow"},
	{NULL, 0, "", "empty buffer"},
	{NULL, 7, "\x30\x05\xa1\x03\x02\x02\x01",
	 "one too short" },
	{NULL, 7, "\x30\x04\xa1\x03\x02\x02\x01"
	 "two too short" },
	{NULL, 7, "\x30\x03\xa1\x03\x02\x02\x01",
	 "three too short" },
	{NULL, 7, "\x30\x02\xa1\x03\x02\x02\x01",
	 "four too short" },
	{NULL, 7, "\x30\x01\xa1\x03\x02\x02\x01",
	 "five too short" },
	{NULL, 7, "\x30\x00\xa1\x03\x02\x02\x01",
	 "six too short" },
	{NULL, 7, "\x30\x05\xa1\x04\x02\x02\x01",
	 "inner one too long" },
	{NULL, 7, "\x30\x00\xa1\x02\x02\x02\x01",
	 "inner one too short" },
	{NULL, 8, "\x30\x05\xbf\x7f\x03\x02\x02\x01",
	 "inner one too short"},
	{NULL, 8, "\x30\x06\xbf\x64\x03\x02\x01\x01",
	 "wrong tag"},
	{NULL, 10, "\x30\x08\xbf\x9a\x9b\x38\x03\x02\x01\x01",
	 "still wrong tag"}
    };
    int ntests = sizeof(tests) / sizeof(*tests);

    return generic_decode_fail(tests, ntests, sizeof(TESTLargeTag),
			       (generic_decode)decode_TESTLargeTag);
}


static int
check_fail_sequence(void)
{
    struct test_case tests[] = {
	{NULL, 0, "", "empty buffer"},
	{NULL, 24, 
	 "\x30\x16\xa0\x03\x02\x01\x01\xa1\x08\x30\x06\xbf\x7f\x03\x02\x01\x01"
	 "\x02\x01\x01\xa2\x03\x02\x01\x01"
	 "missing one byte from the end, internal length ok"},
	{NULL, 25,
	 "\x30\x18\xa0\x03\x02\x01\x01\xa1\x08\x30\x06\xbf\x7f\x03\x02\x01\x01"
	 "\x02\x01\x01\xa2\x03\x02\x01\x01",
	 "inner length one byte too long"},
	{NULL, 24, 
	 "\x30\x17\xa0\x03\x02\x01\x01\xa1\x08\x30\x06\xbf\x7f\x03\x02\x01"
	 "\x01\x02\x01\x01\xa2\x03\x02\x01\x01",
	 "correct buffer but missing one too short"}
    };
    int ntests = sizeof(tests) / sizeof(*tests);

    return generic_decode_fail(tests, ntests, sizeof(TESTSeq),
			       (generic_decode)decode_TESTSeq);
}

static int
check_fail_choice(void)
{
    struct test_case tests[] = {
	{NULL, 6,
	 "\xa1\x02\x02\x01\x01",
	 "one too short"},
	{NULL, 6,
	 "\xa1\x03\x02\x02\x01",
	 "one too short inner"}
    };
    int ntests = sizeof(tests) / sizeof(*tests);

    return generic_decode_fail(tests, ntests, sizeof(TESTChoice1),
			       (generic_decode)decode_TESTChoice1);
}

static int
check_seq(void)
{
    TESTSeqOf seq;
    TESTInteger i;
    int ret;

    seq.val = NULL;
    seq.len = 0;

    ret = add_TESTSeqOf(&seq, &i);
    if (ret) { printf("failed adding\n"); goto out; }
    ret = add_TESTSeqOf(&seq, &i);
    if (ret) { printf("failed adding\n"); goto out; }
    ret = add_TESTSeqOf(&seq, &i);
    if (ret) { printf("failed adding\n"); goto out; }
    ret = add_TESTSeqOf(&seq, &i);
    if (ret) { printf("failed adding\n"); goto out; }

    ret = remove_TESTSeqOf(&seq, seq.len - 1);
    if (ret) { printf("failed removing\n"); goto out; }
    ret = remove_TESTSeqOf(&seq, 2);
    if (ret) { printf("failed removing\n"); goto out; }
    ret = remove_TESTSeqOf(&seq, 0);
    if (ret) { printf("failed removing\n"); goto out; }
    ret = remove_TESTSeqOf(&seq, 0);
    if (ret) { printf("failed removing\n"); goto out; }
    ret = remove_TESTSeqOf(&seq, 0);
    if (ret == 0) {
	printf("can remove from empty list");
	return 1;
    }

    if (seq.len != 0) {
	printf("seq not empty!");
	return 1;
    }
    free_TESTSeqOf(&seq);
    ret = 0;

out:

    return ret;
}

#define test_seq_of(type, ok, ptr)					\
{									\
    heim_octet_string os;						\
    size_t size;							\
    type decode;							\
    ASN1_MALLOC_ENCODE(type, os.data, os.length, ptr, &size, ret);	\
    if (ret)								\
	return ret;							\
    if (os.length != size)						\
	abort();							\
    ret = decode_##type(os.data, os.length, &decode, &size);		\
    free(os.data);							\
    if (ret) {								\
	if (ok)								\
	    return 1;							\
    } else {								\
	free_##type(&decode);						\
	if (!ok)							\
	    return 1;							\
	if (size != 0)							\
            return 1;							\
    }									\
    return 0;								\
}

static int
check_seq_of_size(void)
{
    TESTInteger integers[4] = { 1, 2, 3, 4 };
    int ret;

    {
	TESTSeqSizeOf1 ssof1f1 = { 1, integers };
	TESTSeqSizeOf1 ssof1ok1 = { 2, integers };
	TESTSeqSizeOf1 ssof1f2 = { 3, integers };

	test_seq_of(TESTSeqSizeOf1, 0, &ssof1f1);
	test_seq_of(TESTSeqSizeOf1, 1, &ssof1ok1);
	test_seq_of(TESTSeqSizeOf1, 0, &ssof1f2);
    }
    {
	TESTSeqSizeOf2 ssof2f1 = { 0, NULL };
	TESTSeqSizeOf2 ssof2ok1 = { 1, integers };
	TESTSeqSizeOf2 ssof2ok2 = { 2, integers };
	TESTSeqSizeOf2 ssof2f2 = { 3, integers };
	
	test_seq_of(TESTSeqSizeOf2, 0, &ssof2f1);
	test_seq_of(TESTSeqSizeOf2, 1, &ssof2ok1);
	test_seq_of(TESTSeqSizeOf2, 1, &ssof2ok2);
	test_seq_of(TESTSeqSizeOf2, 0, &ssof2f2);
    }
    {
	TESTSeqSizeOf3 ssof3f1 = { 0, NULL };
	TESTSeqSizeOf3 ssof3ok1 = { 1, integers };
	TESTSeqSizeOf3 ssof3ok2 = { 2, integers };
	
	test_seq_of(TESTSeqSizeOf3, 0, &ssof3f1);
	test_seq_of(TESTSeqSizeOf3, 1, &ssof3ok1);
	test_seq_of(TESTSeqSizeOf3, 1, &ssof3ok2);
    }
    {
	TESTSeqSizeOf4 ssof4ok1 = { 0, NULL };
	TESTSeqSizeOf4 ssof4ok2 = { 1, integers };
	TESTSeqSizeOf4 ssof4ok3 = { 2, integers };
	TESTSeqSizeOf4 ssof4f1  = { 3, integers };
	
	test_seq_of(TESTSeqSizeOf4, 1, &ssof4ok1);
	test_seq_of(TESTSeqSizeOf4, 1, &ssof4ok2); 
	test_seq_of(TESTSeqSizeOf4, 1, &ssof4ok3);
	test_seq_of(TESTSeqSizeOf4, 0, &ssof4f1);
   }

    return 0;
}



int
main(int argc, char **argv)
{
    int ret = 0;

    ret += test_principal ();
    ret += test_authenticator();
    ret += test_krb_error();
    ret += test_Name();
    ret += test_bit_string();

    ret += check_tag_length();
    ret += test_large_tag();
    ret += test_choice();

    ret += test_implicit();
    ret += test_taglessalloc();

    ret += check_fail_largetag();
    ret += check_fail_sequence();
    ret += check_fail_choice();

    ret += check_seq();
    ret += check_seq_of_size();

    return ret;
}
