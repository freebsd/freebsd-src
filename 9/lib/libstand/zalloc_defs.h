/*
 * This module derived from code donated to the FreeBSD Project by 
 * Matthew Dillon <dillon@backplane.com>
 *
 * Copyright (c) 1998 The FreeBSD Project
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

/*
 * DEFS.H
 */

#define USEGUARD		/* use stard/end guard bytes */
#define USEENDGUARD
#define DMALLOCDEBUG		/* add debugging code to gather stats */
#define ZALLOCDEBUG

#include <sys/stdint.h>
#include "stand.h"
#include "zalloc_mem.h"

#define Library extern

/*
 * block extension for sbrk()
 */

#define BLKEXTEND	(4 * 1024)
#define BLKEXTENDMASK	(BLKEXTEND - 1)

/*
 * required malloc alignment.  Just hardwire to 16.
 *
 * Note: if we implement a more sophisticated realloc, we should ensure that
 * MALLOCALIGN is at least as large as MemNode.
 */

typedef struct Guard {
    size_t	ga_Bytes;
    size_t	ga_Magic;	/* must be at least 32 bits */
} Guard;

#define MALLOCALIGN	16
#define GAMAGIC		0x55FF44FD
#define GAFREE		0x5F54F4DF

#include "zalloc_protos.h"
