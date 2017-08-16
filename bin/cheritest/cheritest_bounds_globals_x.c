/*-
 * Copyright (c) 2017 Robert N. M. Watson
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

#include <sys/cdefs.h>

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

/*
 * This file contains a set of sized objects whose bounds are tested in other
 * compilation units.  Given incorrect or insufficient type information at the
 * C level, dynamic sizes should still be correct on capabilities taken to
 * these globals.
 */

/* Integer types. */
extern uint8_t		 extern_global_uint8;
uint8_t			 extern_global_uint8;

extern uint16_t		 extern_global_uint16;
uint16_t		 extern_global_uint16;

extern uint32_t		 extern_global_uint32;
uint32_t		 extern_global_uint32;

extern uint64_t		 extern_global_uint64;
uint64_t		 extern_global_uint64;

/* Odd-sized arrays. */
extern uint8_t		 extern_global_array1[1];
uint8_t			 extern_global_array1[1];

extern uint8_t		 extern_global_array7[7];
uint8_t			 extern_global_array7[7];

extern uint8_t		 extern_global_array65537[65537];
uint8_t			 extern_global_array65537[65537];

/* Power-of-two-sized arrays. */
extern uint8_t		 extern_global_array16[16];
uint8_t			 extern_global_array16[16];

extern uint8_t		 extern_global_array256[256];
uint8_t			 extern_global_array256[256];

extern uint8_t		 extern_global_array65536[65536];
uint8_t			 extern_global_array65536[65536];
