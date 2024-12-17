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
#include <libecc/utils/print_buf.h>
#include <libecc/external_deps/print.h>

/* Print the buffer of a given size */
void buf_print(const char *msg, const u8 *buf, u16 buflen)
{
	u32 i;

	if ((buf == NULL) || (msg == NULL)) {
		goto err;
	}

	ext_printf("%s: ", msg);
	for (i = 0; i < (u32)buflen; i++) {
		ext_printf("%02x", buf[i]);
	}
	ext_printf("\n");

err:
	return;
}
