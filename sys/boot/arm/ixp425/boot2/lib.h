/*-
 * Copyright (c) 2008 John Hay.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef ARM_BOOT_LIB_H
#define ARM_BOOT_LIB_H

#include <sys/cdefs.h>
#include <sys/param.h>

void DELAY(int);

int getc(int);
void putchar(int);
void xputchar(int);
void putstr(const char *);
void puthex8(u_int8_t);
void puthexlist(const u_int8_t *, int);
void printf(const char *fmt,...);

void bzero(void *, size_t);
char *strcpy(char *to, const char *from);
int strcmp(const char *to, const char *from);
int p_strlen(const char *);
int p_memcmp(const char *, const char *, unsigned);
void *memchr(const void *, int, size_t);
void memcpy(void *to, const void *from, unsigned size);
void *memmem(const void *, size_t, const void *, size_t);
void p_memset(char *buffer, char value, int size);

#define strlen p_strlen
#define memcmp p_memcmp
#define memset p_memset

u_int16_t swap16(u_int16_t);
u_int32_t swap32(u_int32_t);

const char *board_init(void);
void clr_board(void);
int avila_read(char*, unsigned, unsigned);

#endif /* !ARM_BOOT_LIB_H */
