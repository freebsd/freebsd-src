/* $OpenBSD: deattack.h,v 1.11 2015/01/19 19:52:16 markus Exp $ */

/*
 * Cryptographic attack detector for ssh - Header file
 *
 * Copyright (c) 1998 CORE SDI S.A., Buenos Aires, Argentina.
 *
 * All rights reserved. Redistribution and use in source and binary
 * forms, with or without modification, are permitted provided that
 * this copyright notice is retained.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES ARE DISCLAIMED. IN NO EVENT SHALL CORE SDI S.A. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY OR
 * CONSEQUENTIAL DAMAGES RESULTING FROM THE USE OR MISUSE OF THIS
 * SOFTWARE.
 *
 * Ariel Futoransky <futo@core-sdi.com>
 * <http://www.core-sdi.com>
 */

#ifndef _DEATTACK_H
#define _DEATTACK_H

/* Return codes */
#define DEATTACK_OK		0
#define DEATTACK_DETECTED	1
#define DEATTACK_DOS_DETECTED	2
#define DEATTACK_ERROR		3

struct deattack_ctx {
	u_int16_t *h;
	u_int32_t n;
};

void	 deattack_init(struct deattack_ctx *);
int	 detect_attack(struct deattack_ctx *, const u_char *, u_int32_t);
#endif
