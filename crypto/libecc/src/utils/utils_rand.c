/*
 *  Copyright (C) 2023 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/utils/utils_rand.h>

/* Unsafe random source:
 * Initial seeding is performed using good entropy, then
 * a congruential linear system is used.
 */
static u64 seed = 0;
int get_unsafe_random(unsigned char *buf, u16 len)
{
        int ret;
        u64 a, b;
        u16 i, j;
        a = (u64)2862933555777941757;
        b = (u64)3037000493;

        if(seed == 0){
                ret = get_random((u8*)&seed, sizeof(seed));
                if(ret){
                        ret = -1;
                        goto err;
                }
        }

        i = 0;
        while(i < len){
                /* Use a congruential linear generator */
                seed = ((a * seed) + b);

                for(j = 0; j < sizeof(seed); j++){
                        if((i + j) < len){
                                buf[i + j] = (u8)((seed >> (j * 8)) & 0xff);
                        }
                }
                i = (u16)(i + sizeof(seed));
        }

        ret = 0;

err:
        return ret;
}
