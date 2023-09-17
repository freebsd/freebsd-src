/*
 * Copyright (c) 2017-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PTI_DISP_DEFAULT_H
#define PTI_DISP_DEFAULT_H

#include <stdint.h>


static const uint8_t disp_default[4][4][8] = {
	/* Effective Addressing Mode: ptem_unknown. */ {
		/* MOD: 0 */ {
			/* RM: 0 */	0,
			/* RM: 1 */	0,
			/* RM: 2 */	0,
			/* RM: 3 */	0,
			/* RM: 4 */	0,
			/* RM: 5 */	0,
			/* RM: 6 */	0,
			/* RM: 7 */	0
		},
		/* MOD: 1 */ {
			/* RM: 0 */	0,
			/* RM: 1 */	0,
			/* RM: 2 */	0,
			/* RM: 3 */	0,
			/* RM: 4 */	0,
			/* RM: 5 */	0,
			/* RM: 6 */	0,
			/* RM: 7 */	0
		},
		/* MOD: 2 */ {
			/* RM: 0 */	0,
			/* RM: 1 */	0,
			/* RM: 2 */	0,
			/* RM: 3 */	0,
			/* RM: 4 */	0,
			/* RM: 5 */	0,
			/* RM: 6 */	0,
			/* RM: 7 */	0
		},
		/* MOD: 3 */ {
			/* RM: 0 */	0,
			/* RM: 1 */	0,
			/* RM: 2 */	0,
			/* RM: 3 */	0,
			/* RM: 4 */	0,
			/* RM: 5 */	0,
			/* RM: 6 */	0,
			/* RM: 7 */	0
		}
	},

	/* Effective Addressing Mode: ptem_16bit. */ {
		/* MOD: 0 */ {
			/* RM: 0 */	0,
			/* RM: 1 */	0,
			/* RM: 2 */	0,
			/* RM: 3 */	0,
			/* RM: 4 */	0,
			/* RM: 5 */	0,
			/* RM: 6 */	2,
			/* RM: 7 */	0
		},
		/* MOD: 1 */ {
			/* RM: 0 */	1,
			/* RM: 1 */	1,
			/* RM: 2 */	1,
			/* RM: 3 */	1,
			/* RM: 4 */	1,
			/* RM: 5 */	1,
			/* RM: 6 */	1,
			/* RM: 7 */	1
		},
		/* MOD: 2 */ {
			/* RM: 0 */	2,
			/* RM: 1 */	2,
			/* RM: 2 */	2,
			/* RM: 3 */	2,
			/* RM: 4 */	2,
			/* RM: 5 */	2,
			/* RM: 6 */	2,
			/* RM: 7 */	2
		},
		/* MOD: 3 */ {
			/* RM: 0 */	0,
			/* RM: 1 */	0,
			/* RM: 2 */	0,
			/* RM: 3 */	0,
			/* RM: 4 */	0,
			/* RM: 5 */	0,
			/* RM: 6 */	0,
			/* RM: 7 */	0
		}
	},

	/* Effective Addressing Mode: ptem_32bit. */ {
		/* MOD: 0 */ {
			/* RM: 0 */	0,
			/* RM: 1 */	0,
			/* RM: 2 */	0,
			/* RM: 3 */	0,
			/* RM: 4 */	0,
			/* RM: 5 */	4,
			/* RM: 6 */	0,
			/* RM: 7 */	0
		},
		/* MOD: 1 */ {
			/* RM: 0 */	1,
			/* RM: 1 */	1,
			/* RM: 2 */	1,
			/* RM: 3 */	1,
			/* RM: 4 */	1,
			/* RM: 5 */	1,
			/* RM: 6 */	1,
			/* RM: 7 */	1
		},
		/* MOD: 2 */ {
			/* RM: 0 */	4,
			/* RM: 1 */	4,
			/* RM: 2 */	4,
			/* RM: 3 */	4,
			/* RM: 4 */	4,
			/* RM: 5 */	4,
			/* RM: 6 */	4,
			/* RM: 7 */	4
		},
		/* MOD: 3 */ {
			/* RM: 0 */	0,
			/* RM: 1 */	0,
			/* RM: 2 */	0,
			/* RM: 3 */	0,
			/* RM: 4 */	0,
			/* RM: 5 */	0,
			/* RM: 6 */	0,
			/* RM: 7 */	0
		}
	},

	/* Effective Addressing Mode: ptem_64bit. */ {
		/* MOD: 0 */ {
			/* RM: 0 */	0,
			/* RM: 1 */	0,
			/* RM: 2 */	0,
			/* RM: 3 */	0,
			/* RM: 4 */	0,
			/* RM: 5 */	4,
			/* RM: 6 */	0,
			/* RM: 7 */	0
		},
		/* MOD: 1 */ {
			/* RM: 0 */	1,
			/* RM: 1 */	1,
			/* RM: 2 */	1,
			/* RM: 3 */	1,
			/* RM: 4 */	1,
			/* RM: 5 */	1,
			/* RM: 6 */	1,
			/* RM: 7 */	1
		},
		/* MOD: 2 */ {
			/* RM: 0 */	4,
			/* RM: 1 */	4,
			/* RM: 2 */	4,
			/* RM: 3 */	4,
			/* RM: 4 */	4,
			/* RM: 5 */	4,
			/* RM: 6 */	4,
			/* RM: 7 */	4
		},
		/* MOD: 3 */ {
			/* RM: 0 */	0,
			/* RM: 1 */	0,
			/* RM: 2 */	0,
			/* RM: 3 */	0,
			/* RM: 4 */	0,
			/* RM: 5 */	0,
			/* RM: 6 */	0,
			/* RM: 7 */	0
		}
	}
};

#endif /* PTI_DISP_DEFAULT_H */
