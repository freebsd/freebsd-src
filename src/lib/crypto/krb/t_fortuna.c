/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/t_fortuna.c - Fortuna test program */
/*
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
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
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
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
#ifdef FORTUNA

/* Include most of prng_fortuna.c so we can test the PRNG internals. */
#define TEST
#include "prng_fortuna.c"

static void
display(const unsigned char *data, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++)
        printf("%02X", data[i]);
    printf("\n");
}

/*
 * Generate data from st with its current internal state and check for
 * significant bias in each bit of the resulting bytes.  This test would have a
 * small chance of failure on random inputs, but we have a predictable state
 * after all the other tests have been run, so it will never fail if the PRNG
 * operates the way we expect.
 */
static void
head_tail_test(struct fortuna_state *st)
{
    static unsigned char buffer[1024 * 1024];
    unsigned char c;
    size_t i, len = sizeof(buffer);
    int bit, bits[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    double res;

    memset(buffer, 0, len);

    generator_output(st, buffer, len);
    for (i = 0; i < len; i++) {
        c = buffer[i];
        for (bit = 0; bit < 8 && c; bit++) {
            if (c & 1)
                bits[bit]++;
            c = c >> 1;
        }
    }

    for (bit = 0; bit < 8; bit++) {
        res = ((double)abs(len - bits[bit] * 2)) / (double)len;
        if (res > 0.005){
            fprintf(stderr,
                    "Bit %d: %d zero, %d one exceeds 0.5%% variance (%f)\n",
                    bit, (int)len - bits[bit], bits[bit], res);
            exit(1);
        }
    }
}

int
main(int argc, char **argv)
{
    struct fortuna_state test_state;
    struct fortuna_state *st = &test_state;
    static unsigned char buf[2 * 1024 * 1024];
    unsigned int i;

    /* Seed the generator with a known state. */
    init_state(&test_state);
    generator_reseed(st, (unsigned char *)"test", 4);

    /* Generate two pieces of output; key should change for each request. */
    generator_output(st, buf, 32);
    display(buf, 32);
    generator_output(st, buf, 32);
    display(buf, 32);

    /* Generate a lot of output to test key changes during request. */
    generator_output(st, buf, sizeof(buf));
    display(buf, 32);
    display(buf + sizeof(buf) - 32, 32);

    /* Reseed the generator and generate more output. */
    generator_reseed(st, (unsigned char *)"retest", 6);
    generator_output(st, buf, 32);
    display(buf, 32);

    /* Add sample data to accumulator pools. */
    for (i = 0; i < 44; i++) {
        store_32_be(i, buf);
        accumulator_add_event(st, buf, 4);
    }
    assert(st->pool_index == 12);
    assert(st->pool0_bytes == 8);

    /* Exercise accumulator reseeds. */
    accumulator_reseed(st);
    generator_output(st, buf, 32);
    display(buf, 32);
    accumulator_reseed(st);
    generator_output(st, buf, 32);
    display(buf, 32);
    accumulator_reseed(st);
    generator_output(st, buf, 32);
    display(buf, 32);
    for (i = 0; i < 1000; i++)
        accumulator_reseed(st);
    assert(st->reseed_count == 1003);
    generator_output(st, buf, 32);
    display(buf, 32);

    head_tail_test(st);
    return 0;
}

#else /* FORTUNA */

int
main()
{
    return 0;
}

#endif /* FORTUNA */
