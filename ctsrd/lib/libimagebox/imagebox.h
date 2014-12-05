/*-
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __IMAGEBOX_H__
#define __IMAGEBOX_H__

enum sbtype {
	SB_NONE,
	SB_CAPSICUM,
	SB_CHERI
};

struct iboxstate {
	enum sbtype		 sb;
	uint32_t		 width;
	uint32_t		 height;
	volatile uint32_t	 valid_rows;
	volatile uint32_t	 passes_remaining;
	volatile uint32_t	 error;
	volatile uint32_t	*buffer;
	volatile uint32_t	 times[4];

	void			*private;
};

extern int ibox_verbose;

void iboxstate_free(struct iboxstate *ps);

uint32_t iboxstate_get_dtime(struct iboxstate *is);
uint32_t iboxstate_get_ttime(struct iboxstate *is);

struct iboxstate* png_read_start(int pfd, uint32_t maxw, uint32_t maxh,
				 enum sbtype);
int png_read_finish(struct iboxstate *ps);

#endif
