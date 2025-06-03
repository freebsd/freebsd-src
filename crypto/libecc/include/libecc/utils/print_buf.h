/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __PRINT_BUF_H__
#define __PRINT_BUF_H__

#include <libecc/words/words.h>

void buf_print(const char *msg, const u8 *buf, u16 buflen);

#endif /* __PRINT_BUF_H__ */
