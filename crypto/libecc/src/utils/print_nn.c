/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/utils/print_nn.h>

/* Print out given nn, prepending msg to the output */
void nn_print(const char *msg, nn_src_t a)
{
	int ret, w;

	ret = nn_check_initialized(a); EG(ret, err);
	MUST_HAVE(msg != NULL, ret, err);

	ext_printf("%s (%d words, i.e. %d bits): 0x", msg, a->wlen,
		   a->wlen * WORD_BYTES * 8);

	for (w = a->wlen - 1; w >= 0; w--) {
		ext_printf(PRINTF_WORD_HEX_FMT, a->val[w]);
	}

	ext_printf("\n");

err:
	return;
}
