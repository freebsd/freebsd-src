/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2014 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"
#include "common.h"
#include "mglueP.h"
#include "gssapiP_krb5.h"

static const char inputstr[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz123456789";

/* For each test, out1 corresponds to key1 with an empty input, and out2
 * corresponds to key2 with the above 61-byte input string. */
static struct {
    krb5_enctype enctype;
    const char *key1;
    const char *out1;
    const char *key2;
    const char *out2;
} tests[] = {
    { ENCTYPE_DES_CBC_CRC,
      "E607FE9DABB57AE0",
      "803C4121379FC4B87CE413B67707C4632EBED2C6D6B7"
      "2A55E878836E35E21600D915D590DED5B6D77BB30A1F",
      "54758316B6257A75",
      "279E4105F7ADC9BD6EF28ABE31D89B442FE0058388BA"
      "33264ACB5729562DC637950F6BD144B654BE7700B2D6" },
    { ENCTYPE_DES3_CBC_SHA1,
      "70378A19CD64134580C27C0115D6B34A1CF2FEECEF9886A2",
      "9F8D127C520BB826BFF3E0FE5EF352389C17E0C073D9"
      "AC4A333D644D21BA3EF24F4A886D143F85AC9F6377FB",
      "3452A167DF1094BA1089E0A20E9E51ABEF1525922558B69E",
      "6BF24FABC858F8DD9752E4FCD331BB831F238B5BE190"
      "4EEA42E38F7A60C588F075C5C96A67E7F8B7BD0AECF4" },
    { ENCTYPE_ARCFOUR_HMAC,
      "3BB3AE288C12B3B9D06B208A4151B3B6",
      "9AEA11A3BCF3C53F1F91F5A0BA2132E2501ADF5F3C28"
      "3C8A983AB88757CE865A22132D6100EAD63E9E291AFA",
      "6DB7B33A01BD2B72F7655CB7B3D5FA0B",
      "CDA9A544869FC84873B692663A82AFDA101C8611498B"
      "A46138B01E927C9B95EEC953B562807434037837DDDF" },
    { ENCTYPE_AES128_CTS_HMAC_SHA1_96,
      "6C742096EB896230312B73972FA28B5D",
      "94208D982FC1BB7778128BDD77904420B45C9DA699F3"
      "117BCE66E39602128EF0296611A6D191A5828530F20F",
      "FA61138C109D834A477D24C7311BE6DA",
      "0FAEDF0F842CC834FEE750487E1B622739286B975FE5"
      "B7F45AB053143C75CA0DF5D3D4BBB80F6A616C7C9027" },
    { ENCTYPE_AES256_CTS_HMAC_SHA1_96,
      "08FCDAFD5832611B73BA7B497FEBFF8C954B4B58031CAD9B977C3B8C25192FD6",
      "E627EFC14EF5B6D629F830C7109DEA0D3D7D36E8CD57"
      "A1F301C5452494A1928F05AFFBEE3360232209D3BE0D",
      "F5B68B7823D8944F33F41541B4E4D38C9B2934F8D16334A796645B066152B4BE",
      "112F2B2D878590653CCC7DE278E9F0AA46FA5A380B62"
      "59F774CB7C134FCD37F61A50FD0D9F89BF8FE1A6B593" },
    { ENCTYPE_CAMELLIA128_CTS_CMAC,
      "866E0466A178279A32AC0BDA92B72AEB",
      "97FBB354BF341C3A160DCC86A7A910FDA824601DF677"
      "68797BACEEBF5D250AE929DEC9760772084267F50A54",
      "D4893FD37DA1A211E12DD1E03E0F03B7",
      "1DEE2FF126CA563A2A2326B9DD3F0095013257414C83"
      "FAD4398901013D55F367C82681186B7B2FE62F746BA4" },
    { ENCTYPE_CAMELLIA256_CTS_CMAC,
      "203071B1AE77BD3D6FCE70174AF95C225B1CED46B35CF52B6479EFEB47E6B063",
      "9B30020634C10FDA28420CEE7B96B70A90A771CED43A"
      "D8346554163E5949CBAE2FB8EF36AFB6B32CE75116A0",
      "A171AD582C1AFBBAD52ABD622EE6B6A14D19BF95C6914B2BA40FFD99A88EC660",
      "A47CBB6E104DCC77E4DB48A7A474B977F2FB6A7A1AB6"
      "52317D50508AE72B7BE2E4E4BA24164E029CBACF786B" },
    { ENCTYPE_AES128_CTS_HMAC_SHA256_128,
      "089BCA48B105EA6EA77CA5D2F39DC5E7",
      "ED1736209B7C59C9F6A3AE8CCC8A7C97ADFDD11688AD"
      "F304F2F74252CBACD311A2D9253211FDA49745CE4F62",
      "3705D96080C17728A0E800EAB6E0D23C",
      "2BB41B183D76D8D5B30CBB049A7EFE9F350EFA058DC2"
      "C4D868308D354A7B199BE6FD1F22B53C038BC6036581" },
    { ENCTYPE_AES256_CTS_HMAC_SHA384_192,
      "45BD806DBF6A833A9CFFC1C94589A222367A79BC21C413718906E9F578A78467",
      "1C613AE8B77A3B4D783F3DCE6C9178FC025E87F48A44"
      "784A69CB5FC697FE266A6141905067EF78566D309085",
      "6D404D37FAF79F9DF0D33568D320669800EB4836472EA8A026D16B7182460C52",
      "D15944B0A44508D1E61213F6455F292A02298F870C01"
      "A3F74AD0345A4A6651EBE101976E933F32D44F0B5947" },
};

/* Decode hexstr into out.  No length checking. */
static size_t
fromhex(const char *hexstr, unsigned char *out)
{
    const char *p;
    size_t count;

    for (p = hexstr, count = 0; *p != '\0'; p += 2, count++)
        sscanf(p, "%2hhx", &out[count]);
    return count;
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major;
    gss_ctx_id_t context;
    gss_union_ctx_id_desc uctx;
    krb5_gss_ctx_id_rec kgctx;
    krb5_key k1, k2;
    krb5_keyblock kb1, kb2;
    gss_buffer_desc in, out;
    unsigned char k1buf[32], k2buf[32], outbuf[44];
    size_t i;

    /*
     * Fake up just enough of a krb5 GSS context to make gss_pseudo_random
     * work, with chosen subkeys and acceptor subkeys.  If we implement
     * gss_import_lucid_sec_context, we can rewrite this to use public
     * interfaces and stop using private headers and internal knowledge of the
     * implementation.
     */
    context = (gss_ctx_id_t)&uctx;
    memset(&uctx, 0, sizeof(uctx));
    uctx.mech_type = &mech_krb5;
    uctx.internal_ctx_id = (gss_ctx_id_t)&kgctx;
    memset(&kgctx, 0, sizeof(kgctx));
    kgctx.k5_context = NULL;
    kgctx.established = 1;
    kgctx.have_acceptor_subkey = 1;
    kb1.contents = k1buf;
    kb2.contents = k2buf;
    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
        /* Set up the keys for this test. */
        kb1.enctype = tests[i].enctype;
        kb1.length = fromhex(tests[i].key1, k1buf);
        check_k5err(NULL, "create_key", krb5_k_create_key(NULL, &kb1, &k1));
        kgctx.subkey = k1;
        kb2.enctype = tests[i].enctype;
        kb2.length = fromhex(tests[i].key2, k2buf);
        check_k5err(NULL, "create_key", krb5_k_create_key(NULL, &kb2, &k2));
        kgctx.acceptor_subkey = k2;

        /* Generate a PRF value with the subkey and an empty input, and compare
         * it to the first expected output. */
        in.length = 0;
        in.value = NULL;
        major = gss_pseudo_random(&minor, context, GSS_C_PRF_KEY_PARTIAL, &in,
                                  44, &out);
        check_gsserr("gss_pseudo_random", major, minor);
        (void)fromhex(tests[i].out1, outbuf);
        assert(out.length == 44 && memcmp(out.value, outbuf, 44) == 0);
        (void)gss_release_buffer(&minor, &out);

        /* Generate a PRF value with the acceptor subkey and the 61-byte input
         * string, and compare it to the second expected output. */
        in.length = strlen(inputstr);
        in.value = (char *)inputstr;
        major = gss_pseudo_random(&minor, context, GSS_C_PRF_KEY_FULL, &in, 44,
                                  &out);
        check_gsserr("gss_pseudo_random", major, minor);
        (void)fromhex(tests[i].out2, outbuf);
        assert(out.length == 44 && memcmp(out.value, outbuf, 44) == 0);
        (void)gss_release_buffer(&minor, &out);

        /* Also check that generating zero bytes of output works. */
        major = gss_pseudo_random(&minor, context, GSS_C_PRF_KEY_FULL, &in, 0,
                                  &out);
        check_gsserr("gss_pseudo_random", major, minor);
        assert(out.length == 0);
        (void)gss_release_buffer(&minor, &out);

        krb5_k_free_key(NULL, k1);
        krb5_k_free_key(NULL, k2);
    }
    return 0;
}
