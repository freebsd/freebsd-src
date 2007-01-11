/*-
 * Copyright (C) 2005 TAKAHASHI Yoshihiro. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pc98/cbus/pckbdtables.h,v 1.1 2005/05/12 13:39:31 nyan Exp $
 */

#ifndef KBD_DFLT_KEYMAP

#define NO_ACCENTCHARS

/* PC-9801 keymap by kuribo@isl.melco.co.jp */
static keymap_t key_map = { 0x80, {	/* PC98 keymap */
/*                                                         alt
 * scan                       cntrl          alt    alt   cntrl
 * code  base   shift  cntrl  shift   alt   shift  cntrl  shift    spcl flgs
 * ---------------------------------------------------------------------------
 */
{{/*00*/ 0x1B,  0x1B,  0x1B,  0x1B,  0x1B,  0x1B,   DBG,  0x1B  }, 0x02,0x00 },
{{/*01*/  '1',   '!',   '!',   '!',   '1',   '!',   '!',   '!'  }, 0x00,0x00 },
{{/*02*/  '2',  '\"',  0x1A,  0x1A,   '2',   '@',  0x00,  0x00  }, 0x00,0x00 },
{{/*03*/  '3',   '#',  0x1B,  0x1B,   '3',   '#',  0x1B,  0x1B  }, 0x00,0x00 },
{{/*04*/  '4',   '$',  0x1C,  0x1C,   '4',   '$',  0x1C,  0x1C  }, 0x00,0x00 },
{{/*05*/  '5',   '%',  0x1D,  0x1D,   '5',   '%',  0x1D,  0x1D  }, 0x00,0x00 },
{{/*06*/  '6',   '&',  0x1E,  0x1E,   '6',   '^',  0x1E,  0x1E  }, 0x00,0x00 },
{{/*07*/  '7',  '\'',  0x1F,  0x1F,   '7',   '&',   '&',   '&'  }, 0x00,0x00 },
{{/*08*/  '8',   '(',  0x7F,  0x7F,   '8',   '*',  0x08,  0x08  }, 0x00,0x00 },
{{/*09*/  '9',   ')',   '9',   '9',   '9',   '(',   '(',   '('  }, 0x00,0x00 },
{{/*0a*/  '0',   NOP,   '0',   '0',   '0',   ')',   ')',   ')'  }, 0x40,0x00 },
{{/*0b*/  '-',   '=',   '-',   '-',   '-',   '_',  0x1F,  0x1F  }, 0x00,0x00 },
{{/*0c*/  '^',   '`',  0x1E,  0x1E,   '=',   '+',   '+',   '+'  }, 0x00,0x00 },
{{/*0d*/ '\\',   '|',  0x1C,  0x1C,  '\\',   '|',  0x1C,  0x1C  }, 0x00,0x00 },
{{/*0e*/ 0x08,  0x08,  0x08,  0x08,  0x08,  0x08,  0x08,  0x08  }, 0x00,0x00 },
{{/*0f*/ '\t',  BTAB,  '\t',  BTAB,  '\t',  BTAB,  '\t',  BTAB  }, 0x55,0x00 },
{{/*10*/  'q',   'Q',  0x11,  0x11,   'q',   'Q',  0x11,  0x11  }, 0x00,0x01 },
{{/*11*/  'w',   'W',  0x17,  0x17,   'w',   'W',  0x17,  0x17  }, 0x00,0x01 },
{{/*12*/  'e',   'E',  0x05,  0x05,   'e',   'E',  0x05,  0x05  }, 0x00,0x01 },
{{/*13*/  'r',   'R',  0x12,  0x12,   'r',   'R',  0x12,  0x12  }, 0x00,0x01 },
{{/*14*/  't',   'T',  0x14,  0x14,   't',   'T',  0x14,  0x14  }, 0x00,0x01 },
{{/*15*/  'y',   'Y',  0x19,  0x19,   'y',   'Y',  0x19,  0x19  }, 0x00,0x01 },
{{/*16*/  'u',   'U',  0x15,  0x15,   'u',   'U',  0x15,  0x15  }, 0x00,0x01 },
{{/*17*/  'i',   'I',  0x09,  0x09,   'i',   'I',  0x09,  0x09  }, 0x00,0x01 },
{{/*18*/  'o',   'O',  0x0F,  0x0F,   'o',   'O',  0x0F,  0x0F  }, 0x00,0x01 },
{{/*19*/  'p',   'P',  0x10,  0x10,   'p',   'P',  0x10,  0x10  }, 0x00,0x01 },
{{/*1a*/  '@',   '~',  0x00,  0x00,   '[',   '{',  0x1B,  0x1B  }, 0x00,0x00 },
{{/*1b*/  '[',   '{',  0x1B,  0x1B,   ']',   '}',  0x1D,  0x1D  }, 0x00,0x00 },
{{/*1c*/ '\r',  '\r',  '\n',  '\n',  '\r',  '\r',  '\n',  '\n'  }, 0x00,0x00 },
{{/*1d*/  'a',   'A',  0x01,  0x01,   'a',   'A',  0x01,  0x01  }, 0x00,0x01 },
{{/*1e*/  's',   'S',  0x13,  0x13,   's',   'S',  0x13,  0x13  }, 0x00,0x01 },
{{/*1f*/  'd',   'D',  0x04,  0x04,   'd',   'D',  0x04,  0x04  }, 0x00,0x01 },
{{/*20*/  'f',   'F',  0x06,  0x06,   'f',   'F',  0x06,  0x06  }, 0x00,0x01 },
{{/*21*/  'g',   'G',  0x07,  0x07,   'g',   'G',  0x07,  0x07  }, 0x00,0x01 },
{{/*22*/  'h',   'H',  0x08,  0x08,   'h',   'H',  0x08,  0x08  }, 0x00,0x01 },
{{/*23*/  'j',   'J',  '\n',  '\n',   'j',   'J',  '\n',  '\n'  }, 0x00,0x01 },
{{/*24*/  'k',   'K',  0x0B,  0x0B,   'k',   'K',  0x0B,  0x0B  }, 0x00,0x01 },
{{/*25*/  'l',   'L',  0x0C,  0x0C,   'l',   'L',  0x0C,  0x0C  }, 0x00,0x01 },
{{/*26*/  ';',   '+',   ';',   ';',   ';',   ':',   ';',   ';'  }, 0x00,0x00 },
{{/*27*/  ':',   '*',   ':',   ':',  '\'',  '\"',  '\'',  '\''  }, 0x00,0x00 },
{{/*28*/  ']',   '}',  0x1D,  0x1D,   '`',   '~',   '~',   '~'  }, 0x00,0x00 },
{{/*29*/  'z',   'Z',  0x1A,  0x1A,   'z',   'Z',  0x1A,  0x1A  }, 0x00,0x01 },
{{/*2a*/  'x',   'X',  0x18,  0x18,   'x',   'X',  0x18,  0x18  }, 0x00,0x01 },
{{/*2b*/  'c',   'C',  0x03,  0x03,   'c',   'C',  0x03,  0x03  }, 0x00,0x01 },
{{/*2c*/  'v',   'V',  0x16,  0x16,   'v',   'V',  0x16,  0x16  }, 0x00,0x01 },
{{/*2d*/  'b',   'B',  0x02,  0x02,   'b',   'B',  0x02,  0x02  }, 0x00,0x01 },
{{/*2e*/  'n',   'N',  0x0E,  0x0E,   'n',   'N',  0x0E,  0x0E  }, 0x00,0x01 },
{{/*2f*/  'm',   'M',  '\r',  '\r',   'm',   'M',  '\r',  '\r'  }, 0x00,0x01 },
{{/*30*/  ',',   '<',   '<',   '<',   ',',   '<',   '<',   '<'  }, 0x00,0x00 },
{{/*31*/  '.',   '>',   '>',   '>',   '.',   '>',   '>',   '>'  }, 0x00,0x00 },
{{/*32*/  '/',   '?',  0x7F,  0x7F,   '/',   '?',  0x7F,  0x7F  }, 0x00,0x00 },
{{/*33*/  NOP,   '_',  0x1F,  0x1F,  '\\',   '|',  0x1C,  0x1C  }, 0x80,0x00 },
{{/*34*/  ' ',   ' ',  0x00,  0x00,   ' ',   ' ',  0x00,  0x00  }, 0x00,0x00 },
{{/*35*/ 0x1B,  0x1B,  0x1B,  0x1B,  0x1B,  0x1B,  0x1B,  0x1B  }, 0x00,0x00 },
{{/*36*/ F(59), F(59), F(59), F(59), F(59), F(59), F(59), F(59) }, 0xFF,0x00 },
{{/*37*/ F(51), F(51), F(51), F(51), F(51), F(51), F(51), F(51) }, 0xFF,0x00 },
{{/*38*/ F(60), F(60), F(60), F(60), F(60), F(60), F(60), F(60) }, 0xFF,0x00 },
{{/*39*/ 0x7F,  0x7F,  0x7F,  0x7F,  0x7F,  0x7F,   RBT,   RBT  }, 0x03,0x02 },
{{/*3a*/ F(50), F(50), F(50), F(50), F(50), F(50), F(50), F(50) }, 0xFF,0x00 },
{{/*3b*/ F(53), F(53), F(53), F(53), F(53), F(53), F(53), F(53) }, 0xFF,0x00 },
{{/*3c*/ F(55), F(55), F(55), F(55), F(55), F(55), F(55), F(55) }, 0xFF,0x00 },
{{/*3d*/ F(58), F(58), F(58), F(58), F(58), F(58), F(58), F(58) }, 0xFF,0x00 },
{{/*3e*/ F(49), F(49), F(49), F(49), F(49), F(49), F(49), F(49) }, 0xFF,0x00 },
{{/*3f*/ F(57), F(57), F(57), F(57), F(57), F(57), F(57), F(57) }, 0xFF,0x00 },
{{/*40*/  '-',   '-',   '-',   '-',   '-',   '-',   '-',   '-'  }, 0x00,0x00 },
{{/*41*/  '/',   '/',   '/',   '/',   '/',   '/',   '/',   '/'  }, 0x00,0x00 },
{{/*42*/  '7',   '7',   '7',   '7',   '7',   '7',   '7',   '7'  }, 0x00,0x00 },
{{/*43*/  '8',   '8',   '8',   '8',   '8',   '8',   '8',   '8'  }, 0x00,0x00 },
{{/*44*/  '9',   '9',   '9',   '9',   '9',   '9',   '9',   '9'  }, 0x00,0x00 },
{{/*45*/  '*',   '*',   '*',   '*',   '*',   '*',   '*',   '*'  }, 0x00,0x00 },
{{/*46*/  '4',   '4',   '4',   '4',   '4',   '4',   '4',   '4'  }, 0x00,0x00 },
{{/*47*/  '5',   '5',   '5',   '5',   '5',   '5',   '5',   '5'  }, 0x00,0x00 },
{{/*48*/  '6',   '6',   '6',   '6',   '6',   '6',   '6',   '6'  }, 0x00,0x00 },
{{/*49*/  '+',   '+',   '+',   '+',   '+',   '+',   '+',   '+'  }, 0x00,0x00 },
{{/*4a*/  '1',   '1',   '1',   '1',   '1',   '1',   '1',   '1'  }, 0x00,0x00 },
{{/*4b*/  '2',   '2',   '2',   '2',   '2',   '2',   '2',   '2'  }, 0x00,0x00 },
{{/*4c*/  '3',   '3',   '3',   '3',   '3',   '3',   '3',   '3'  }, 0x00,0x00 },
{{/*4d*/  '=',   '=',   '=',   '=',   '=',   '=',   '=',   '='  }, 0x00,0x00 },
{{/*4e*/  '0',   '0',   '0',   '0',   '0',   '0',   '0',   '0'  }, 0x00,0x00 },
{{/*4f*/  ',',   ',',   ',',   ',',   ',',   ',',   ',',   ','  }, 0x00,0x00 },
{{/*50*/  '.',   '.',   '.',   '.',   '.',   '.',   '.',   '.'  }, 0x00,0x00 },
{{/*51*/ META,  META,  META,  META,  META,  META,  META,  META  }, 0xFF,0x00 },
{{/*52*/ F(11), F(23), F(35), F(47), S(11), S(11), S(11), S(11) }, 0xFF,0x00 },
{{/*53*/ F(12), F(24), F(36), F(48), S(12), S(12), S(12), S(12) }, 0xFF,0x00 },
{{/*54*/  SLK,   SLK,   SLK,   SLK,   SLK,   SLK,   SLK,   SLK  }, 0xFF,0x00 },
{{/*55*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*56*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*57*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*58*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*59*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*5a*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*5b*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*5c*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*5d*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*5e*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*5f*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*60*/  SLK,  SPSC,   SLK,  SPSC,  SUSP,   NOP,  SUSP,   NOP  }, 0xFF,0x00 },
{{/*61*/ NEXT,  NEXT,   DBG,   DBG,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*62*/ F( 1), F(13), F(25), F(37), S( 1), S( 1), S( 1), S( 1) }, 0xFF,0x00 },
{{/*63*/ F( 2), F(14), F(26), F(38), S( 2), S( 2), S( 2), S( 2) }, 0xFF,0x00 },
{{/*64*/ F( 3), F(15), F(27), F(39), S( 3), S( 3), S( 3), S( 3) }, 0xFF,0x00 },
{{/*65*/ F( 4), F(16), F(28), F(40), S( 4), S( 4), S( 4), S( 4) }, 0xFF,0x00 },
{{/*66*/ F( 5), F(17), F(29), F(41), S( 5), S( 5), S( 5), S( 5) }, 0xFF,0x00 },
{{/*67*/ F( 6), F(18), F(30), F(42), S( 6), S( 6), S( 6), S( 6) }, 0xFF,0x00 },
{{/*68*/ F( 7), F(19), F(31), F(43), S( 7), S( 7), S( 7), S( 7) }, 0xFF,0x00 },
{{/*69*/ F( 8), F(20), F(32), F(44), S( 8), S( 8), S( 8), S( 8) }, 0xFF,0x00 },
{{/*6a*/ F( 9), F(21), F(33), F(45), S( 9), S( 9), S( 9), S( 9) }, 0xFF,0x00 },
{{/*6b*/ F(10), F(22), F(34), F(46), S(10), S(10), S(10), S(10) }, 0xFF,0x00 },
{{/*6c*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*6d*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*6e*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*6f*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*70*/  LSH,   LSH,   LSH,   LSH,   LSH,   LSH,   LSH,   LSH  }, 0xFF,0x00 },
{{/*71*/  CLK,   CLK,   CLK,   CLK,   CLK,   CLK,   CLK,   CLK  }, 0xFF,0x00 },
{{/*72*/ LALT,  LALT,  LALT,  LALT,  LALT,  LALT,  LALT,  LALT  }, 0xFF,0x00 },
{{/*73*/ LALT,  LALT,  LALT,  LALT,  LALT,  LALT,  LALT,  LALT  }, 0xFF,0x00 },
{{/*74*/ LCTR,  LCTR,  LCTR,  LCTR,  LCTR,  LCTR,  LCTR,  LCTR  }, 0xFF,0x00 },
{{/*75*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*76*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*77*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*78*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*79*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*7a*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*7b*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*7c*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*7d*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*7e*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
{{/*7f*/  NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP,   NOP  }, 0xFF,0x00 },
} };

static accentmap_t accent_map = { 0,		/* empty accent map */
  {
    { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, 
    { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 },
  }
};

#endif /* !KBD_DFLT_KEYMAP */

static fkeytab_t fkey_tab[96] = {
/* 01-04 */	{"\033[M", 3}, {"\033[N", 3}, {"\033[O", 3}, {"\033[P", 3},
/* 05-08 */	{"\033[Q", 3}, {"\033[R", 3}, {"\033[S", 3}, {"\033[T", 3},
/* 09-12 */	{"\033[U", 3}, {"\033[V", 3}, {"\033[W", 3}, {"\033[X", 3},
/* 13-16 */	{"\033[Y", 3}, {"\033[Z", 3}, {"\033[a", 3}, {"\033[b", 3},
/* 17-20 */	{"\033[c", 3}, {"\033[d", 3}, {"\033[e", 3}, {"\033[f", 3},
/* 21-24 */	{"\033[g", 3}, {"\033[h", 3}, {"\033[i", 3}, {"\033[j", 3},
/* 25-28 */	{"\033[k", 3}, {"\033[l", 3}, {"\033[m", 3}, {"\033[n", 3},
/* 29-32 */	{"\033[o", 3}, {"\033[p", 3}, {"\033[q", 3}, {"\033[r", 3},
/* 33-36 */	{"\033[s", 3}, {"\033[t", 3}, {"\033[u", 3}, {"\033[v", 3},
/* 37-40 */	{"\033[w", 3}, {"\033[x", 3}, {"\033[y", 3}, {"\033[z", 3},
/* 41-44 */	{"\033[@", 3}, {"\033[[", 3}, {"\033[\\",3}, {"\033[]", 3},
/* 45-48 */     {"\033[^", 3}, {"\033[_", 3}, {"\033[`", 3}, {"\033[{", 3},
/* 49-52 */	{"\033[H", 3}, {"\033[A", 3}, {"\033[I", 3}, {"-"     , 1},
/* 53-56 */	{"\033[D", 3}, {"\033[E", 3}, {"\033[C", 3}, {"+"     , 1},
/* 57-60 */	{"\033[F", 3}, {"\033[B", 3}, {"\033[G", 3}, {"\033[L", 3},
/* 61-64 */	{"\177", 1},   {"\033[J", 3}, {"\033[~", 3}, {"\033[}", 3},
/* 65-68 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 69-72 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 73-76 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 77-80 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 81-84 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 85-88 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 89-92 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 93-96 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}
};
