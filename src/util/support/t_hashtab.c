/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/t_hash.c - tests for hash table code */
/*
 * Copyright (C) 2018 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* hash.c has no linker dependencies, so we can simply include its source code
 * to test its static functions and look inside its structures. */
#include "hashtab.c"

/* These match the sip64 test vectors in the reference C implementation of
 * siphash at https://github.com/veorq/SipHash */
const uint64_t vectors[64] = {
    0x726FDB47DD0E0E31,
    0x74F839C593DC67FD,
    0x0D6C8009D9A94F5A,
    0x85676696D7FB7E2D,
    0xCF2794E0277187B7,
    0x18765564CD99A68D,
    0xCBC9466E58FEE3CE,
    0xAB0200F58B01D137,
    0x93F5F5799A932462,
    0x9E0082DF0BA9E4B0,
    0x7A5DBBC594DDB9F3,
    0xF4B32F46226BADA7,
    0x751E8FBC860EE5FB,
    0x14EA5627C0843D90,
    0xF723CA908E7AF2EE,
    0xA129CA6149BE45E5,
    0x3F2ACC7F57C29BDB,
    0x699AE9F52CBE4794,
    0x4BC1B3F0968DD39C,
    0xBB6DC91DA77961BD,
    0xBED65CF21AA2EE98,
    0xD0F2CBB02E3B67C7,
    0x93536795E3A33E88,
    0xA80C038CCD5CCEC8,
    0xB8AD50C6F649AF94,
    0xBCE192DE8A85B8EA,
    0x17D835B85BBB15F3,
    0x2F2E6163076BCFAD,
    0xDE4DAAACA71DC9A5,
    0xA6A2506687956571,
    0xAD87A3535C49EF28,
    0x32D892FAD841C342,
    0x7127512F72F27CCE,
    0xA7F32346F95978E3,
    0x12E0B01ABB051238,
    0x15E034D40FA197AE,
    0x314DFFBE0815A3B4,
    0x027990F029623981,
    0xCADCD4E59EF40C4D,
    0x9ABFD8766A33735C,
    0x0E3EA96B5304A7D0,
    0xAD0C42D6FC585992,
    0x187306C89BC215A9,
    0xD4A60ABCF3792B95,
    0xF935451DE4F21DF2,
    0xA9538F0419755787,
    0xDB9ACDDFF56CA510,
    0xD06C98CD5C0975EB,
    0xE612A3CB9ECBA951,
    0xC766E62CFCADAF96,
    0xEE64435A9752FE72,
    0xA192D576B245165A,
    0x0A8787BF8ECB74B2,
    0x81B3E73D20B49B6F,
    0x7FA8220BA3B2ECEA,
    0x245731C13CA42499,
    0xB78DBFAF3A8D83BD,
    0xEA1AD565322A1A0B,
    0x60E61C23A3795013,
    0x6606D7E446282B93,
    0x6CA4ECB15C5F91E1,
    0x9F626DA15C9625F3,
    0xE51B38608EF25F57,
    0x958A324CEB064572
};

static void
test_siphash()
{
    uint8_t seq[64];
    uint64_t k0, k1, hval;
    size_t i;

    for (i = 0; i < sizeof(seq); i++)
        seq[i] = i;
    k0 = load_64_le(seq);
    k1 = load_64_le(seq + 8);

    for (i = 0; i < sizeof(seq); i++) {
        hval = siphash24(seq, i, k0, k1);
        assert(hval == vectors[i]);
    }
}

static void
test_hashtab()
{
    int st;
    struct k5_hashtab *ht;
    size_t i;
    char zeros[100] = { 0 };

    st = k5_hashtab_create(NULL, 4, &ht);
    assert(st == 0 && ht != NULL && ht->nentries == 0);

    st = k5_hashtab_add(ht, "abc", 3, &st);
    assert(st == 0 && ht->nentries == 1);
    assert(k5_hashtab_get(ht, "abc", 3) == &st);
    assert(k5_hashtab_get(ht, "bcde", 4) == NULL);

    st = k5_hashtab_add(ht, "bcde", 4, &ht);
    assert(st == 0 && ht->nentries == 2);
    assert(k5_hashtab_get(ht, "abc", 3) == &st);
    assert(k5_hashtab_get(ht, "bcde", 4) == &ht);

    k5_hashtab_remove(ht, "abc", 3);
    assert(ht->nentries == 1);
    assert(k5_hashtab_get(ht, "abc", 3) == NULL);
    assert(k5_hashtab_get(ht, "bcde", 4) == &ht);

    k5_hashtab_remove(ht, "bcde", 4);
    assert(ht->nentries == 0);
    assert(k5_hashtab_get(ht, "abc", 3) == NULL);
    assert(k5_hashtab_get(ht, "bcde", 4) == NULL);

    for (i = 0; i < sizeof(zeros); i++) {
        st = k5_hashtab_add(ht, zeros, i, zeros + i);
        assert(st == 0 && ht->nentries == i + 1 && ht->nbuckets >= i + 1);
    }
    for (i = 0; i < sizeof(zeros); i++) {
        assert(k5_hashtab_get(ht, zeros, i) == zeros + i);
        k5_hashtab_remove(ht, zeros, i);
        assert(ht->nentries == sizeof(zeros) - i - 1);
        if (i > 0)
            assert(k5_hashtab_get(ht, zeros, i - 1) == NULL);
    }

    k5_hashtab_free(ht);
}

int
main()
{
    test_siphash();
    test_hashtab();
    return 0;
}
