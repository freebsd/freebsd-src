/*-
 * Copyright (c) 1988 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)asc_ebc.c	4.2 (Berkeley) 4/26/91";
#endif /* not lint */

/*
 * Ascii<->Ebcdic translation tables.
 */

#include "asc_ebc.h"

unsigned char asc_ebc[NASCII]	= {

/* 00 */   0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
/* 08 */   0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
/* 10 */   0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
/* 18 */   0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
/* 20 */   0x40,  0x5A,  0x7F,  0x7B,  0x5B,  0x6C,  0x50,  0x7D,
/* 28 */   0x4D,  0x5D,  0x5C,  0x4E,  0x6B,  0x60,  0x4B,  0x61,
/* 30 */   0xF0,  0xF1,  0xF2,  0xF3,  0xF4,  0xF5,  0xF6,  0xF7,
/* 38 */   0xF8,  0xF9,  0x7A,  0x5E,  0x4C,  0x7E,  0x6E,  0x6F,
/* 40 */   0x7C,  0xC1,  0xC2,  0xC3,  0xC4,  0xC5,  0xC6,  0xC7,
/* 48 */   0xC8,  0xC9,  0xD1,  0xD2,  0xD3,  0xD4,  0xD5,  0xD6,
/* 50 */   0xD7,  0xD8,  0xD9,  0xE2,  0xE3,  0xE4,  0xE5,  0xE6,
/* 58 */   0xE7,  0xE8,  0xE9,  0xAD,  0xE0,  0xBD,  0x5F,  0x6D,
/* 60 */   0x79,  0x81,  0x82,  0x83,  0x84,  0x85,  0x86,  0x87,
/* 68 */   0x88,  0x89,  0x91,  0x92,  0x93,  0x94,  0x95,  0x96,
/* 70 */   0x97,  0x98,  0x99,  0xA2,  0xA3,  0xA4,  0xA5,  0xA6,
/* 78 */   0xA7,  0xA8,  0xA9,  0xC0,  0x4F,  0xD0,  0xA1,  0x00,

};

/*
 * ebcdic to ascii translation tables
 */

unsigned char	ebc_asc[NEBC] = {
/* 00 */   ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 08 */   ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 10 */   ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 18 */   ' ',  ' ',  ' ',  ' ',  '*',  ' ',  ';',  ' ',
/* 20 */   ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 28 */   ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 30 */   ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 38 */   ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 40 */   ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',

/* 48 */   ' ',  ' ', 
#if	!defined(MSDOS)
        /* 4A */       '\\',
#else	/* !defined(MSDOS) */
        /* 4A */       '\233',		/* PC cent sign */
#endif	/* !defined(MSDOS) */
        /* 4B */              '.',  '<',  '(',  '+',  '|',

/* 50 */   '&',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 58 */   ' ',  ' ',  '!',  '$',  '*',  ')',  ';',  '^',
/* 60 */   '-',  '/',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 68 */   ' ',  ' ',  '|',  ',',  '%',  '_',  '>',  '?',
/* 70 */   ' ',  '^',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 78 */   ' ',  '`',  ':',  '#',  '@', '\'',  '=',  '"',
/* 80 */   ' ',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
/* 88 */   'h',  'i',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* 90 */   ' ',  'j',  'k',  'l',  'm',  'n',  'o',  'p',
/* 98 */   'q',  'r',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* A0 */   ' ',  '~',  's',  't',  'u',  'v',  'w',  'x',
/* A8 */   'y',  'z',  ' ',  ' ',  ' ',  '[',  ' ',  ' ',
/* B0 */   ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* B8 */   ' ',  ' ',  ' ',  ' ',  ' ',  ']',  ' ',  ' ',
/* C0 */   '{',  'A',  'B',  'C',  'D',  'E',  'F',  'G',
/* C8 */   'H',  'I',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* D0 */   '}',  'J',  'K',  'L',  'M',  'N',  'O',  'P',
/* D8 */   'Q',  'R',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* E0 */  '\\',  ' ',  'S',  'T',  'U',  'V',  'W',  'X',
/* E8 */   'Y',  'Z',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
/* F0 */   '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
/* F8 */   '8',  '9',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
};
