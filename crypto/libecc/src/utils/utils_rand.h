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
#ifndef __UTILS_RAND_H__
#define __UTILS_RAND_H__

#include <libecc/words/words.h>
#include "../external_deps/rand.h"

/* WARNING: use with care, this is useful when "fast" but somehow unsafe
 * random must be provided.
 */
ATTRIBUTE_WARN_UNUSED_RET int get_unsafe_random(unsigned char *buf, u16 len);

#endif /* __UTILS_RAND_H__ */
