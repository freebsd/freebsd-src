/*
  Copyright 2001 Free Software Foundation, Inc.

  This file is part of GNU Binutils.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This program generates z8k-opc.h */

#include <stdio.h>
#include "sysdep.h"

#define BYTE_INFO_LEN 10

struct op
{
  char *flags;
  int cycles;
  char type;
  char *bits;
  char *name;
  char *flavor;
};

#define iswhite(x) ((x) == ' ' || (x) == '\t')
struct op opt[] =
{
  "------", 10, 8, "0000 1110 imm8", "ext0e imm8", 0,
  "------", 10, 8, "0000 1111 imm8", "ext0f imm8", 0,
  "------", 10, 8, "1000 1110 imm8", "ext8e imm8", 0,
  "------", 10, 8, "1000 1111 imm8", "ext8f imm8", 0,

  "------", 10, 8, "0011 0110 imm8", "rsvd36", 0,
  "------", 10, 8, "0011 1000 imm8", "rsvd38", 0,
  "------", 10, 8, "0111 1000 imm8", "rsvd78", 0,
  "------", 10, 8, "0111 1110 imm8", "rsvd7e", 0,

  "------", 10, 8, "1001 1101 imm8", "rsvd9d", 0,
  "------", 10, 8, "1001 1111 imm8", "rsvd9f", 0,

  "------", 10, 8, "1011 1001 imm8", "rsvdb9", 0,
  "------", 10, 8, "1011 1111 imm8", "rsvdbf", 0,

  "---V--", 11, 16, "1011 1011 ssN0 1001 0000 rrrr ddN0 1000", "ldd @rd,@rs,rr", 0,
  "---V--", 11, 16, "1011 1011 ssN0 1001 0000 rrrr ddN0 0000", "lddr @rd,@rs,rr", 0,
  "---V--", 11, 8, "1011 1010 ssN0 1001 0000 rrrr ddN0 0000", "lddrb @rd,@rs,rr", 0,
  "---V--", 11, 16, "1011 1011 ssN0 0001 0000 rrrr ddN0 0000", "ldir @rd,@rs,rr", 0,
  "CZSV--", 11, 16, "1011 1011 ssN0 0000 0000 rrrr dddd cccc", "cpi rd,@rs,rr,cc", 0,
  "CZSV--", 11, 16, "1011 1011 ssN0 0100 0000 rrrr dddd cccc", "cpir rd,@rs,rr,cc", 0,
  "CZSV--", 11, 16, "1011 1011 ssN0 1100 0000 rrrr dddd cccc", "cpdr rd,@rs,rr,cc", 0,
  "---V--", 11, 16, "1011 1011 ssN0 0001 0000 rrrr ddN0 1000", "ldi @rd,@rs,rr", 0,
  "CZSV--", 11, 16, "1011 1011 ssN0 1000 0000 rrrr dddd cccc", "cpd rd,@rs,rr,cc", 0,
  "---V--", 11, 8, "1011 1010 ssN0 0001 0000 rrrr ddN0 0000", "ldirb @rd,@rs,rr", 0,
  "---V--", 11, 8, "1011 1010 ssN0 1001 0000 rrrr ddN0 1000", "lddb @rd,@rs,rr", 0,
  "---V--", 11, 8, "1011 1010 ssN0 0001 0000 rrrr ddN0 1000", "ldib @rd,@rs,rr", 0,
  "CZSV--", 11, 8, "1011 1010 ssN0 1000 0000 rrrr dddd cccc", "cpdb rbd,@rs,rr,cc", 0,
  "CZSV--", 11, 8, "1011 1010 ssN0 1100 0000 rrrr dddd cccc", "cpdrb rbd,@rs,rr,cc", 0,
  "CZSV--", 11, 8, "1011 1010 ssN0 0000 0000 rrrr dddd cccc", "cpib rbd,@rs,rr,cc", 0,
  "CZSV--", 11, 8, "1011 1010 ssN0 0100 0000 rrrr dddd cccc", "cpirb rbd,@rs,rr,cc", 0,
  "CZSV--", 11, 16, "1011 1011 ssN0 1010 0000 rrrr ddN0 cccc", "cpsd @rd,@rs,rr,cc", 0,
  "CZSV--", 11, 8, "1011 1010 ssN0 1010 0000 rrrr ddN0 cccc", "cpsdb @rd,@rs,rr,cc", 0,
  "CZSV--", 11, 16, "1011 1011 ssN0 1110 0000 rrrr ddN0 cccc", "cpsdr @rd,@rs,rr,cc", 0,
  "CZSV--", 11, 8, "1011 1010 ssN0 1110 0000 rrrr ddN0 cccc", "cpsdrb @rd,@rs,rr,cc", 0,
  "CZSV--", 11, 16, "1011 1011 ssN0 0010 0000 rrrr ddN0 cccc", "cpsi @rd,@rs,rr,cc", 0,
  "CZSV--", 11, 8, "1011 1010 ssN0 0010 0000 rrrr ddN0 cccc", "cpsib @rd,@rs,rr,cc", 0,
  "CZSV--", 11, 16, "1011 1011 ssN0 0110 0000 rrrr ddN0 cccc", "cpsir @rd,@rs,rr,cc", 0,
  "CZSV--", 11, 8, "1011 1010 ssN0 0110 0000 rrrr ddN0 cccc", "cpsirb @rd,@rs,rr,cc", 0,

  "------", 2, 8, "0011 0110 0000 0000", "bpt", 0,
  "CZSV--", 5, 16, "1011 0101 ssss dddd", "adc rd,rs", 0,
  "CZSVDH", 5, 8, "1011 0100 ssss dddd", "adcb rbd,rbs", 0,
  "CZSV--", 7, 16, "0000 0001 ssN0 dddd", "add rd,@rs", 0,
  "CZSV--", 9, 16, "0100 0001 0000 dddd address_src", "add rd,address_src", 0,
  "CZSV--", 10, 16, "0100 0001 ssN0 dddd address_src", "add rd,address_src(rs)", 0,
  "CZSV--", 7, 16, "0000 0001 0000 dddd imm16", "add rd,imm16", 0,
  "CZSV--", 4, 16, "1000 0001 ssss dddd", "add rd,rs", 0,
  "CZSVDH", 7, 8, "0000 0000 ssN0 dddd", "addb rbd,@rs", 0,
  "CZSVDH", 9, 8, "0100 0000 0000 dddd address_src", "addb rbd,address_src", 0,
  "CZSVDH", 10, 8, "0100 0000 ssN0 dddd address_src", "addb rbd,address_src(rs)", 0,
  "CZSVDH", 7, 8, "0000 0000 0000 dddd imm8 imm8", "addb rbd,imm8", 0,
  "CZSVDH", 4, 8, "1000 0000 ssss dddd", "addb rbd,rbs", 0,
  "CZSV--", 14, 32, "0001 0110 ssN0 dddd", "addl rrd,@rs", 0,
  "CZSV--", 15, 32, "0101 0110 0000 dddd address_src", "addl rrd,address_src", 0,
  "CZSV--", 16, 32, "0101 0110 ssN0 dddd address_src", "addl rrd,address_src(rs)", 0,
  "CZSV--", 14, 32, "0001 0110 0000 dddd imm32", "addl rrd,imm32", 0,
  "CZSV--", 8, 32, "1001 0110 ssss dddd", "addl rrd,rrs", 0,

  "-ZS---", 7, 16, "0000 0111 ssN0 dddd", "and rd,@rs", 0,
  "-ZS---", 9, 16, "0100 0111 0000 dddd address_src", "and rd,address_src", 0,
  "-ZS---", 10, 16, "0100 0111 ssN0 dddd address_src", "and rd,address_src(rs)", 0,
  "-ZS---", 7, 16, "0000 0111 0000 dddd imm16", "and rd,imm16", 0,
  "-ZS---", 4, 16, "1000 0111 ssss dddd", "and rd,rs", 0,
  "-ZSP--", 7, 8, "0000 0110 ssN0 dddd", "andb rbd,@rs", 0,
  "-ZSP--", 9, 8, "0100 0110 0000 dddd address_src", "andb rbd,address_src", 0,
  "-ZSP--", 10, 8, "0100 0110 ssN0 dddd address_src", "andb rbd,address_src(rs)", 0,
  "-ZSP--", 7, 8, "0000 0110 0000 dddd imm8 imm8", "andb rbd,imm8", 0,
  "-ZSP--", 4, 8, "1000 0110 ssss dddd", "andb rbd,rbs", 0,

  "-Z----", 8, 16, "0010 0111 ddN0 imm4", "bit @rd,imm4", 0,
  "-Z----", 11, 16, "0110 0111 ddN0 imm4 address_dst", "bit address_dst(rd),imm4", 0,
  "-Z----", 10, 16, "0110 0111 0000 imm4 address_dst", "bit address_dst,imm4", 0,
  "-Z----", 4, 16, "1010 0111 dddd imm4", "bit rd,imm4", 0,
  "-Z----", 10, 16, "0010 0111 0000 ssss 0000 dddd 0000 0000", "bit rd,rs", 0,

  "-Z----", 8, 8, "0010 0110 ddN0 imm4", "bitb @rd,imm4", 0,
  "-Z----", 11, 8, "0110 0110 ddN0 imm4 address_dst", "bitb address_dst(rd),imm4", 0,
  "-Z----", 10, 8, "0110 0110 0000 imm4 address_dst", "bitb address_dst,imm4", 0,
  "-Z----", 4, 8, "1010 0110 dddd imm4", "bitb rbd,imm4", 0,
  "-Z----", 10, 8, "0010 0110 0000 ssss 0000 dddd 0000 0000", "bitb rbd,rs", 0,

  "------", 10, 32, "0001 1111 ddN0 0000", "call @rd", 0,
  "------", 12, 32, "0101 1111 0000 0000 address_dst", "call address_dst", 0,
  "------", 13, 32, "0101 1111 ddN0 0000 address_dst", "call address_dst(rd)", 0,
  "------", 10, 16, "1101 disp12", "calr disp12", 0,

  "------", 8, 16, "0000 1101 ddN0 1000", "clr @rd", 0,
  "------", 11, 16, "0100 1101 0000 1000 address_dst", "clr address_dst", 0,
  "------", 12, 16, "0100 1101 ddN0 1000 address_dst", "clr address_dst(rd)", 0,
  "------", 7, 16, "1000 1101 dddd 1000", "clr rd", 0,
  "------", 8, 8, "0000 1100 ddN0 1000", "clrb @rd", 0,
  "------", 11, 8, "0100 1100 0000 1000 address_dst", "clrb address_dst", 0,
  "------", 12, 8, "0100 1100 ddN0 1000 address_dst", "clrb address_dst(rd)", 0,
  "------", 7, 8, "1000 1100 dddd 1000", "clrb rbd", 0,
  "-ZS---", 12, 16, "0000 1101 ddN0 0000", "com @rd", 0,
  "-ZS---", 15, 16, "0100 1101 0000 0000 address_dst", "com address_dst", 0,
  "-ZS---", 16, 16, "0100 1101 ddN0 0000 address_dst", "com address_dst(rd)", 0,
  "-ZS---", 7, 16, "1000 1101 dddd 0000", "com rd", 0,
  "-ZSP--", 12, 8, "0000 1100 ddN0 0000", "comb @rd", 0,
  "-ZSP--", 15, 8, "0100 1100 0000 0000 address_dst", "comb address_dst", 0,
  "-ZSP--", 16, 8, "0100 1100 ddN0 0000 address_dst", "comb address_dst(rd)", 0,
  "-ZSP--", 7, 8, "1000 1100 dddd 0000", "comb rbd", 0,
  "CZSP--", 7, 16, "1000 1101 flags 0101", "comflg flags", 0,

  "CZSV--", 11, 16, "0000 1101 ddN0 0001 imm16", "cp @rd,imm16", 0,
  "CZSV--", 15, 16, "0100 1101 ddN0 0001 address_dst imm16", "cp address_dst(rd),imm16", 0,
  "CZSV--", 14, 16, "0100 1101 0000 0001 address_dst imm16", "cp address_dst,imm16", 0,

  "CZSV--", 7, 16, "0000 1011 ssN0 dddd", "cp rd,@rs", 0,
  "CZSV--", 9, 16, "0100 1011 0000 dddd address_src", "cp rd,address_src", 0,
  "CZSV--", 10, 16, "0100 1011 ssN0 dddd address_src", "cp rd,address_src(rs)", 0,
  "CZSV--", 7, 16, "0000 1011 0000 dddd imm16", "cp rd,imm16", 0,
  "CZSV--", 4, 16, "1000 1011 ssss dddd", "cp rd,rs", 0,

  "CZSV--", 11, 8, "0000 1100 ddN0 0001 imm8 imm8", "cpb @rd,imm8", 0,
  "CZSV--", 15, 8, "0100 1100 ddN0 0001 address_dst imm8 imm8", "cpb address_dst(rd),imm8", 0,
  "CZSV--", 14, 8, "0100 1100 0000 0001 address_dst imm8 imm8", "cpb address_dst,imm8", 0,
  "CZSV--", 7, 8, "0000 1010 ssN0 dddd", "cpb rbd,@rs", 0,
  "CZSV--", 9, 8, "0100 1010 0000 dddd address_src", "cpb rbd,address_src", 0,
  "CZSV--", 10, 8, "0100 1010 ssN0 dddd address_src", "cpb rbd,address_src(rs)", 0,
  "CZSV--", 7, 8, "0000 1010 0000 dddd imm8 imm8", "cpb rbd,imm8", 0,
  "CZSV--", 4, 8, "1000 1010 ssss dddd", "cpb rbd,rbs", 0,

  "CZSV--", 14, 32, "0001 0000 ssN0 dddd", "cpl rrd,@rs", 0,
  "CZSV--", 15, 32, "0101 0000 0000 dddd address_src", "cpl rrd,address_src", 0,
  "CZSV--", 16, 32, "0101 0000 ssN0 dddd address_src", "cpl rrd,address_src(rs)", 0,
  "CZSV--", 14, 32, "0001 0000 0000 dddd imm32", "cpl rrd,imm32", 0,
  "CZSV--", 8, 32, "1001 0000 ssss dddd", "cpl rrd,rrs", 0,

  "CZS---", 5, 8, "1011 0000 dddd 0000", "dab rbd", 0,
  "------", 11, 16, "1111 dddd 0disp7", "dbjnz rbd,disp7", 0,
  "-ZSV--", 11, 16, "0010 1011 ddN0 imm4m1", "dec @rd,imm4m1", 0,
  "-ZSV--", 14, 16, "0110 1011 ddN0 imm4m1 address_dst", "dec address_dst(rd),imm4m1", 0,
  "-ZSV--", 13, 16, "0110 1011 0000 imm4m1 address_dst", "dec address_dst,imm4m1", 0,
  "-ZSV--", 4, 16, "1010 1011 dddd imm4m1", "dec rd,imm4m1", 0,
  "-ZSV--", 11, 8, "0010 1010 ddN0 imm4m1", "decb @rd,imm4m1", 0,
  "-ZSV--", 14, 8, "0110 1010 ddN0 imm4m1 address_dst", "decb address_dst(rd),imm4m1", 0,
  "-ZSV--", 13, 8, "0110 1010 0000 imm4m1 address_dst", "decb address_dst,imm4m1", 0,
  "-ZSV--", 4, 8, "1010 1010 dddd imm4m1", "decb rbd,imm4m1", 0,

  "------", 7, 16, "0111 1100 0000 00ii", "di i2", 0,
  "CZSV--", 107, 16, "0001 1011 ssN0 dddd", "div rrd,@rs", 0,
  "CZSV--", 107, 16, "0101 1011 0000 dddd address_src", "div rrd,address_src", 0,
  "CZSV--", 107, 16, "0101 1011 ssN0 dddd address_src", "div rrd,address_src(rs)", 0,
  "CZSV--", 107, 16, "0001 1011 0000 dddd imm16", "div rrd,imm16", 0,
  "CZSV--", 107, 16, "1001 1011 ssss dddd", "div rrd,rs", 0,
  "CZSV--", 744, 32, "0001 1010 ssN0 dddd", "divl rqd,@rs", 0,
  "CZSV--", 745, 32, "0101 1010 0000 dddd address_src", "divl rqd,address_src", 0,
  "CZSV--", 746, 32, "0101 1010 ssN0 dddd address_src", "divl rqd,address_src(rs)", 0,
  "CZSV--", 744, 32, "0001 1010 0000 dddd imm32", "divl rqd,imm32", 0,
  "CZSV--", 744, 32, "1001 1010 ssss dddd", "divl rqd,rrs", 0,

  "------", 11, 16, "1111 dddd 1disp7", "djnz rd,disp7", 0,
  "------", 7, 16, "0111 1100 0000 01ii", "ei i2", 0,
  "------", 6, 16, "1010 1101 ssss dddd", "ex rd,rs", 0,
  "------", 12, 16, "0010 1101 ssN0 dddd", "ex rd,@rs", 0,
  "------", 15, 16, "0110 1101 0000 dddd address_src", "ex rd,address_src", 0,
  "------", 16, 16, "0110 1101 ssN0 dddd address_src", "ex rd,address_src(rs)", 0,

  "------", 12, 8, "0010 1100 ssN0 dddd", "exb rbd,@rs", 0,
  "------", 15, 8, "0110 1100 0000 dddd address_src", "exb rbd,address_src", 0,
  "------", 16, 8, "0110 1100 ssN0 dddd address_src", "exb rbd,address_src(rs)", 0,
  "------", 6, 8, "1010 1100 ssss dddd", "exb rbd,rbs", 0,

  "------", 11, 16, "1011 0001 dddd 1010", "exts rrd", 0,
  "------", 11, 8, "1011 0001 dddd 0000", "extsb rd", 0,
  "------", 11, 32, "1011 0001 dddd 0111", "extsl rqd", 0,

  "------", 8, 16, "0111 1010 0000 0000", "halt", 0,
  "------", 10, 16, "0011 1101 ssN0 dddd", "in rd,@rs", 0,
  "------", 12, 16, "0011 1101 dddd 0100 imm16", "in rd,imm16", 0,
  "------", 12, 8, "0011 1100 ssN0 dddd", "inb rbd,@rs", 0,
  "------", 10, 8, "0011 1010 dddd 0100 imm16", "inb rbd,imm16", 0,
  "-ZSV--", 11, 16, "0010 1001 ddN0 imm4m1", "inc @rd,imm4m1", 0,
  "-ZSV--", 14, 16, "0110 1001 ddN0 imm4m1 address_dst", "inc address_dst(rd),imm4m1", 0,
  "-ZSV--", 13, 16, "0110 1001 0000 imm4m1 address_dst", "inc address_dst,imm4m1", 0,
  "-ZSV--", 4, 16, "1010 1001 dddd imm4m1", "inc rd,imm4m1", 0,
  "-ZSV--", 11, 8, "0010 1000 ddN0 imm4m1", "incb @rd,imm4m1", 0,
  "-ZSV--", 14, 8, "0110 1000 ddN0 imm4m1 address_dst", "incb address_dst(rd),imm4m1", 0,
  "-ZSV--", 13, 8, "0110 1000 0000 imm4m1 address_dst", "incb address_dst,imm4m1", 0,
  "-ZSV--", 4, 8, "1010 1000 dddd imm4m1", "incb rbd,imm4m1", 0,
  "---V--", 21, 16, "0011 1011 ssN0 1000 0000 aaaa ddN0 1000", "ind @rd,@rs,ra", 0,
  "---V--", 21, 8, "0011 1010 ssN0 1000 0000 aaaa ddN0 1000", "indb @rd,@rs,rba", 0,
  "---V--", 21, 8, "0011 1010 ssN0 0000 0000 aaaa ddN0 1000", "inib @rd,@rs,ra", 0,
  "---V--", 21, 16, "0011 1010 ssN0 0000 0000 aaaa ddN0 0000", "inibr @rd,@rs,ra", 0,
  "CZSVDH", 13, 16, "0111 1011 0000 0000", "iret", 0,
  "------", 10, 16, "0001 1110 ddN0 cccc", "jp cc,@rd", 0,
  "------", 7, 16, "0101 1110 0000 cccc address_dst", "jp cc,address_dst", 0,
  "------", 8, 16, "0101 1110 ddN0 cccc address_dst", "jp cc,address_dst(rd)", 0,
  "------", 6, 16, "1110 cccc disp8", "jr cc,disp8", 0,

  "------", 7, 16, "0000 1101 ddN0 0101 imm16", "ld @rd,imm16", 0,
  "------", 8, 16, "0010 1111 ddN0 ssss", "ld @rd,rs", 0,
  "------", 15, 16, "0100 1101 ddN0 0101 address_dst imm16", "ld address_dst(rd),imm16", 0,
  "------", 12, 16, "0110 1111 ddN0 ssss address_dst", "ld address_dst(rd),rs", 0,
  "------", 14, 16, "0100 1101 0000 0101 address_dst imm16", "ld address_dst,imm16", 0,
  "------", 11, 16, "0110 1111 0000 ssss address_dst", "ld address_dst,rs", 0,
  "------", 14, 16, "0011 0011 ddN0 ssss imm16", "ld rd(imm16),rs", 0,
  "------", 14, 16, "0111 0011 ddN0 ssss 0000 xxxx 0000 0000", "ld rd(rx),rs", 0,
  "------", 7, 16, "0010 0001 ssN0 dddd", "ld rd,@rs", 0,
  "------", 9, 16, "0110 0001 0000 dddd address_src", "ld rd,address_src", 0,
  "------", 10, 16, "0110 0001 ssN0 dddd address_src", "ld rd,address_src(rs)", 0,
  "------", 7, 16, "0010 0001 0000 dddd imm16", "ld rd,imm16", 0,
  "------", 3, 16, "1010 0001 ssss dddd", "ld rd,rs", 0,
  "------", 14, 16, "0011 0001 ssN0 dddd imm16", "ld rd,rs(imm16)", 0,
  "------", 14, 16, "0111 0001 ssN0 dddd 0000 xxxx 0000 0000", "ld rd,rs(rx)", 0,

  "------", 7, 8, "0000 1100 ddN0 0101 imm8 imm8", "ldb @rd,imm8", 0,
  "------", 8, 8, "0010 1110 ddN0 ssss", "ldb @rd,rbs", 0,
  "------", 15, 8, "0100 1100 ddN0 0101 address_dst imm8 imm8", "ldb address_dst(rd),imm8", 0,
  "------", 12, 8, "0110 1110 ddN0 ssss address_dst", "ldb address_dst(rd),rbs", 0,
  "------", 14, 8, "0100 1100 0000 0101 address_dst imm8 imm8", "ldb address_dst,imm8", 0,
  "------", 11, 8, "0110 1110 0000 ssss address_dst", "ldb address_dst,rbs", 0,
  "------", 14, 8, "0011 0010 ddN0 ssss imm16", "ldb rd(imm16),rbs", 0,
  "------", 14, 8, "0111 0010 ddN0 ssss 0000 xxxx 0000 0000", "ldb rd(rx),rbs", 0,
  "------", 7, 8, "0010 0000 ssN0 dddd", "ldb rbd,@rs", 0,
  "------", 9, 8, "0110 0000 0000 dddd address_src", "ldb rbd,address_src", 0,
  "------", 10, 8, "0110 0000 ssN0 dddd address_src", "ldb rbd,address_src(rs)", 0,
  "------", 5, 8, "1100 dddd imm8", "ldb rbd,imm8", 0,
  "------", 3, 8, "1010 0000 ssss dddd", "ldb rbd,rbs", 0,
  "------", 14, 8, "0011 0000 ssN0 dddd imm16", "ldb rbd,rs(imm16)", 0,
  "------", 14, 8, "0111 0000 ssN0 dddd 0000 xxxx 0000 0000", "ldb rbd,rs(rx)", 0,

  "------", 11, 32, "0001 1101 ddN0 ssss", "ldl @rd,rrs", 0,
  "------", 14, 32, "0101 1101 ddN0 ssss address_dst", "ldl address_dst(rd),rrs", 0,
  "------", 15, 32, "0101 1101 0000 ssss address_dst", "ldl address_dst,rrs", 0,
  "------", 17, 32, "0011 0111 ddN0 ssss imm16", "ldl rd(imm16),rrs", 0,
  "------", 17, 32, "0111 0111 ddN0 ssss 0000 xxxx 0000 0000", "ldl rd(rx),rrs", 0,
  "------", 11, 32, "0001 0100 ssN0 dddd", "ldl rrd,@rs", 0,
  "------", 12, 32, "0101 0100 0000 dddd address_src", "ldl rrd,address_src", 0,
  "------", 13, 32, "0101 0100 ssN0 dddd address_src", "ldl rrd,address_src(rs)", 0,
  "------", 11, 32, "0001 0100 0000 dddd imm32", "ldl rrd,imm32", 0,
  "------", 5, 32, "1001 0100 ssss dddd", "ldl rrd,rrs", 0,
  "------", 17, 32, "0011 0101 ssN0 dddd imm16", "ldl rrd,rs(imm16)", 0,
  "------", 17, 32, "0111 0101 ssN0 dddd 0000 xxxx 0000 0000", "ldl rrd,rs(rx)", 0,

  "------", 12, 16, "0111 0110 0000 dddd address_src", "lda prd,address_src", 0,
  "------", 13, 16, "0111 0110 ssN0 dddd address_src", "lda prd,address_src(rs)", 0,
  "------", 15, 16, "0011 0100 ssN0 dddd imm16", "lda prd,rs(imm16)", 0,
  "------", 15, 16, "0111 0100 ssN0 dddd 0000 xxxx 0000 0000", "lda prd,rs(rx)", 0,
  "------", 15, 16, "0011 0100 0000 dddd disp16", "ldar prd,disp16", 0,
  "------", 7, 32, "0111 1101 ssss 1ccc", "ldctl ctrl,rs", 0,
  "------", 7, 32, "0111 1101 dddd 0ccc", "ldctl rd,ctrl", 0,

  "------", 5, 16, "1011 1101 dddd imm4", "ldk rd,imm4", 0,

  "------", 11, 16, "0001 1100 ddN0 1001 0000 ssss 0000 nminus1", "ldm @rd,rs,n", 0,
  "------", 15, 16, "0101 1100 ddN0 1001 0000 ssss 0000 nminus1 address_dst", "ldm address_dst(rd),rs,n", 0,
  "------", 14, 16, "0101 1100 0000 1001 0000 ssss 0000 nminus1 address_dst", "ldm address_dst,rs,n", 0,
  "------", 11, 16, "0001 1100 ssN0 0001 0000 dddd 0000 nminus1", "ldm rd,@rs,n", 0,
  "------", 15, 16, "0101 1100 ssN0 0001 0000 dddd 0000 nminus1 address_src", "ldm rd,address_src(rs),n", 0,
  "------", 14, 16, "0101 1100 0000 0001 0000 dddd 0000 nminus1 address_src", "ldm rd,address_src,n", 0,

  "CZSVDH", 12, 16, "0011 1001 ssN0 0000", "ldps @rs", 0,
  "CZSVDH", 16, 16, "0111 1001 0000 0000 address_src", "ldps address_src", 0,
  "CZSVDH", 17, 16, "0111 1001 ssN0 0000 address_src", "ldps address_src(rs)", 0,

  "------", 14, 16, "0011 0011 0000 ssss disp16", "ldr disp16,rs", 0,
  "------", 14, 16, "0011 0001 0000 dddd disp16", "ldr rd,disp16", 0,
  "------", 14, 8, "0011 0010 0000 ssss disp16", "ldrb disp16,rbs", 0,
  "------", 14, 8, "0011 0000 0000 dddd disp16", "ldrb rbd,disp16", 0,
  "------", 17, 32, "0011 0111 0000 ssss disp16", "ldrl disp16,rrs", 0,
  "------", 17, 32, "0011 0101 0000 dddd disp16", "ldrl rrd,disp16", 0,

  "CZS---", 7, 16, "0111 1011 0000 1010", "mbit", 0,
  "-ZS---", 12, 16, "0111 1011 dddd 1101", "mreq rd", 0,
  "------", 5, 16, "0111 1011 0000 1001", "mres", 0,
  "------", 5, 16, "0111 1011 0000 1000", "mset", 0,

  "CZSV--", 70, 16, "0001 1001 ssN0 dddd", "mult rrd,@rs", 0,
  "CZSV--", 70, 16, "0101 1001 0000 dddd address_src", "mult rrd,address_src", 0,
  "CZSV--", 70, 16, "0101 1001 ssN0 dddd address_src", "mult rrd,address_src(rs)", 0,
  "CZSV--", 70, 16, "0001 1001 0000 dddd imm16", "mult rrd,imm16", 0,
  "CZSV--", 70, 16, "1001 1001 ssss dddd", "mult rrd,rs", 0,
  "CZSV--", 282, 32, "0001 1000 ssN0 dddd", "multl rqd,@rs", 0,
  "CZSV--", 282, 32, "0101 1000 0000 dddd address_src", "multl rqd,address_src", 0,
  "CZSV--", 282, 32, "0101 1000 ssN0 dddd address_src", "multl rqd,address_src(rs)", 0,
  "CZSV--", 282, 32, "0001 1000 0000 dddd imm32", "multl rqd,imm32", 0,
  "CZSV--", 282, 32, "1001 1000 ssss dddd", "multl rqd,rrs", 0,
  "CZSV--", 12, 16, "0000 1101 ddN0 0010", "neg @rd", 0,
  "CZSV--", 15, 16, "0100 1101 0000 0010 address_dst", "neg address_dst", 0,
  "CZSV--", 16, 16, "0100 1101 ddN0 0010 address_dst", "neg address_dst(rd)", 0,
  "CZSV--", 7, 16, "1000 1101 dddd 0010", "neg rd", 0,
  "CZSV--", 12, 8, "0000 1100 ddN0 0010", "negb @rd", 0,
  "CZSV--", 15, 8, "0100 1100 0000 0010 address_dst", "negb address_dst", 0,
  "CZSV--", 16, 8, "0100 1100 ddN0 0010 address_dst", "negb address_dst(rd)", 0,
  "CZSV--", 7, 8, "1000 1100 dddd 0010", "negb rbd", 0,

  "------", 7, 16, "1000 1101 0000 0111", "nop", 0,

  "CZS---", 7, 16, "0000 0101 ssN0 dddd", "or rd,@rs", 0,
  "CZS---", 9, 16, "0100 0101 0000 dddd address_src", "or rd,address_src", 0,
  "CZS---", 10, 16, "0100 0101 ssN0 dddd address_src", "or rd,address_src(rs)", 0,
  "CZS---", 7, 16, "0000 0101 0000 dddd imm16", "or rd,imm16", 0,
  "CZS---", 4, 16, "1000 0101 ssss dddd", "or rd,rs", 0,

  "CZSP--", 7, 8, "0000 0100 ssN0 dddd", "orb rbd,@rs", 0,
  "CZSP--", 9, 8, "0100 0100 0000 dddd address_src", "orb rbd,address_src", 0,
  "CZSP--", 10, 8, "0100 0100 ssN0 dddd address_src", "orb rbd,address_src(rs)", 0,
  "CZSP--", 7, 8, "0000 0100 0000 dddd imm8 imm8", "orb rbd,imm8", 0,
  "CZSP--", 4, 8, "1000 0100 ssss dddd", "orb rbd,rbs", 0,

  "---V--", 0, 16, "0011 1111 ddN0 ssss", "out @rd,rs", 0,
  "---V--", 0, 16, "0011 1011 ssss 0110 imm16", "out imm16,rs", 0,
  "---V--", 0, 8, "0011 1110 ddN0 ssss", "outb @rd,rbs", 0,
  "---V--", 0, 8, "0011 1010 ssss 0110 imm16", "outb imm16,rbs", 0,
  "---V--", 0, 16, "0011 1011 ssN0 1010 0000 aaaa ddN0 1000", "outd @rd,@rs,ra", 0,
  "---V--", 0, 16, "0011 1010 ssN0 1010 0000 aaaa ddN0 1000", "outdb @rd,@rs,rba", 0,
  "---V--", 0, 16, "0011 1011 ssN0 0010 0000 aaaa ddN0 1000", "outi @rd,@rs,ra", 0,
  "---V--", 0, 16, "0011 1010 ssN0 0010 0000 aaaa ddN0 1000", "outib @rd,@rs,ra", 0,
  "---V--", 0, 16, "0011 1010 ssN0 0010 0000 aaaa ddN0 0000", "outibr @rd,@rs,ra", 0,

  "------", 12, 16, "0001 0111 ssN0 ddN0", "pop @rd,@rs", 0,
  "------", 16, 16, "0101 0111 ssN0 ddN0 address_dst", "pop address_dst(rd),@rs", 0,
  "------", 16, 16, "0101 0111 ssN0 0000 address_dst", "pop address_dst,@rs", 0,
  "------", 8, 16, "1001 0111 ssN0 dddd", "pop rd,@rs", 0,

  "------", 19, 32, "0001 0101 ssN0 ddN0", "popl @rd,@rs", 0,
  "------", 23, 32, "0101 0101 ssN0 ddN0 address_dst", "popl address_dst(rd),@rs", 0,
  "------", 23, 32, "0101 0101 ssN0 0000 address_dst", "popl address_dst,@rs", 0,
  "------", 12, 32, "1001 0101 ssN0 dddd", "popl rrd,@rs", 0,

  "------", 13, 16, "0001 0011 ddN0 ssN0", "push @rd,@rs", 0,
  "------", 14, 16, "0101 0011 ddN0 0000 address_src", "push @rd,address_src", 0,
  "------", 14, 16, "0101 0011 ddN0 ssN0 address_src", "push @rd,address_src(rs)", 0,
  "------", 12, 16, "0000 1101 ddN0 1001 imm16", "push @rd,imm16", 0,
  "------", 9, 16, "1001 0011 ddN0 ssss", "push @rd,rs", 0,

  "------", 20, 32, "0001 0001 ddN0 ssN0", "pushl @rd,@rs", 0,
  "------", 21, 32, "0101 0001 ddN0 ssN0 address_src", "pushl @rd,address_src(rs)", 0,
  "------", 21, 32, "0101 0001 ddN0 0000 address_src", "pushl @rd,address_src", 0,
  "------", 12, 32, "1001 0001 ddN0 ssss", "pushl @rd,rrs", 0,

  "------", 11, 16, "0010 0011 ddN0 imm4", "res @rd,imm4", 0,
  "------", 14, 16, "0110 0011 ddN0 imm4 address_dst", "res address_dst(rd),imm4", 0,
  "------", 13, 16, "0110 0011 0000 imm4 address_dst", "res address_dst,imm4", 0,
  "------", 4, 16, "1010 0011 dddd imm4", "res rd,imm4", 0,
  "------", 10, 16, "0010 0011 0000 ssss 0000 dddd 0000 0000", "res rd,rs", 0,

  "------", 11, 8, "0010 0010 ddN0 imm4", "resb @rd,imm4", 0,
  "------", 14, 8, "0110 0010 ddN0 imm4 address_dst", "resb address_dst(rd),imm4", 0,
  "------", 13, 8, "0110 0010 0000 imm4 address_dst", "resb address_dst,imm4", 0,
  "------", 4, 8, "1010 0010 dddd imm4", "resb rbd,imm4", 0,
  "------", 10, 8, "0010 0010 0000 ssss 0000 dddd 0000 0000", "resb rbd,rs", 0,

  "CZSV--", 7, 16, "1000 1101 flags 0011", "resflg flags", 0,
  "------", 10, 16, "1001 1110 0000 cccc", "ret cc", 0,

  "CZSV--", 6, 16, "1011 0011 dddd 00I0", "rl rd,imm1or2", 0,
  "CZSV--", 6, 8, "1011 0010 dddd 00I0", "rlb rbd,imm1or2", 0,
  "CZSV--", 6, 16, "1011 0011 dddd 10I0", "rlc rd,imm1or2", 0,

  "-Z----", 9, 8, "1011 0010 dddd 10I0", "rlcb rbd,imm1or2", 0,
  "-Z----", 9, 8, "1011 1110 aaaa bbbb", "rldb rbb,rba", 0,

  "CZSV--", 6, 16, "1011 0011 dddd 01I0", "rr rd,imm1or2", 0,
  "CZSV--", 6, 8, "1011 0010 dddd 01I0", "rrb rbd,imm1or2", 0,
  "CZSV--", 6, 16, "1011 0011 dddd 11I0", "rrc rd,imm1or2", 0,

  "-Z----", 9, 8, "1011 0010 dddd 11I0", "rrcb rbd,imm1or2", 0,
  "-Z----", 9, 8, "1011 1100 aaaa bbbb", "rrdb rbb,rba", 0,
  "CZSV--", 5, 16, "1011 0111 ssss dddd", "sbc rd,rs", 0,
  "CZSVDH", 5, 8, "1011 0110 ssss dddd", "sbcb rbd,rbs", 0,

  "CZSVDH", 33, 8, "0111 1111 imm8", "sc imm8", 0,

  "CZSV--", 15, 16, "1011 0011 dddd 1011 0000 ssss 0000 0000", "sda rd,rs", 0,
  "CZSV--", 15, 8, "1011 0010 dddd 1011 0000 ssss 0000 0000", "sdab rbd,rs", 0,
  "CZSV--", 15, 32, "1011 0011 dddd 1111 0000 ssss 0000 0000", "sdal rrd,rs", 0,

  "CZS---", 15, 16, "1011 0011 dddd 0011 0000 ssss 0000 0000", "sdl rd,rs", 0,
  "CZS---", 15, 8, "1011 0010 dddd 0011 0000 ssss 0000 0000", "sdlb rbd,rs", 0,
  "CZS---", 15, 32, "1011 0011 dddd 0111 0000 ssss 0000 0000", "sdll rrd,rs", 0,

  "------", 11, 16, "0010 0101 ddN0 imm4", "set @rd,imm4", 0,
  "------", 14, 16, "0110 0101 ddN0 imm4 address_dst", "set address_dst(rd),imm4", 0,
  "------", 13, 16, "0110 0101 0000 imm4 address_dst", "set address_dst,imm4", 0,
  "------", 4, 16, "1010 0101 dddd imm4", "set rd,imm4", 0,
  "------", 10, 16, "0010 0101 0000 ssss 0000 dddd 0000 0000", "set rd,rs", 0,
  "------", 11, 8, "0010 0100 ddN0 imm4", "setb @rd,imm4", 0,
  "------", 14, 8, "0110 0100 ddN0 imm4 address_dst", "setb address_dst(rd),imm4", 0,
  "------", 13, 8, "0110 0100 0000 imm4 address_dst", "setb address_dst,imm4", 0,
  "------", 4, 8, "1010 0100 dddd imm4", "setb rbd,imm4", 0,
  "------", 10, 8, "0010 0100 0000 ssss 0000 dddd 0000 0000", "setb rbd,rs", 0,

  "CZSV--", 7, 16, "1000 1101 flags 0001", "setflg flags", 0,

  "------", 0, 8, "0011 1010 dddd 0101 imm16", "sinb rbd,imm16", 0,
  "------", 0, 8, "0011 1011 dddd 0101 imm16", "sin rd,imm16", 0,
  "------", 0, 16, "0011 1011 ssN0 1000 0001 aaaa ddN0 1000", "sind @rd,@rs,ra", 0,
  "------", 0, 8, "0011 1010 ssN0 1000 0001 aaaa ddN0 1000", "sindb @rd,@rs,rba", 0,
  "------", 0, 8, "0011 1010 ssN0 0001 0000 aaaa ddN0 1000", "sinib @rd,@rs,ra", 0,
  "------", 0, 16, "0011 1010 ssN0 0001 0000 aaaa ddN0 0000", "sinibr @rd,@rs,ra", 0,

  "CZSV--", 13, 16, "1011 0011 dddd 1001 0000 0000 imm8", "sla rd,imm8", 0,
  "CZSV--", 13, 8, "1011 0010 dddd 1001  0000 0000 imm8", "slab rbd,imm8", 0,
  "CZSV--", 13, 32, "1011 0011 dddd 1101 0000 0000 imm8", "slal rrd,imm8", 0,

  "CZS---", 13, 16, "1011 0011 dddd 0001 0000 0000 imm8", "sll rd,imm8", 0,
  "CZS---", 13, 8, "1011 0010 dddd 0001  0000 0000 imm8", "sllb rbd,imm8", 0,
  "CZS---", 13, 32, "1011 0011 dddd 0101 0000 0000 imm8", "slll rrd,imm8", 0,

  "------", 0, 16, "0011 1011 ssss 0111 imm16", "sout imm16,rs", 0,
  "------", 0, 8, "0011 1010 ssss 0111 imm16", "soutb imm16,rbs", 0,
  "------", 0, 16, "0011 1011 ssN0 1011 0000 aaaa ddN0 1000", "soutd @rd,@rs,ra", 0,
  "------", 0, 8, "0011 1010 ssN0 1011 0000 aaaa ddN0 1000", "soutdb @rd,@rs,rba", 0,
  "------", 0, 8, "0011 1010 ssN0 0011 0000 aaaa ddN0 1000", "soutib @rd,@rs,ra", 0,
  "------", 0, 16, "0011 1010 ssN0 0011 0000 aaaa ddN0 0000", "soutibr @rd,@rs,ra", 0,

  "CZSV--", 13, 16, "1011 0011 dddd 1001 1111 1111 nim8", "sra rd,imm8", 0,
  "CZSV--", 13, 8, "1011 0010 dddd 1001 0000 0000 nim8", "srab rbd,imm8", 0,
  "CZSV--", 13, 32, "1011 0011 dddd 1101 1111 1111 nim8", "sral rrd,imm8", 0,

  "CZSV--", 13, 16, "1011 0011 dddd 0001 1111 1111 nim8", "srl rd,imm8", 0,
  "CZSV--", 13, 8, "1011 0010 dddd 0001 0000 0000 nim8", "srlb rbd,imm8", 0,
  "CZSV--", 13, 32, "1011 0011 dddd 0101 1111 1111 nim8", "srll rrd,imm8", 0,

  "CZSV--", 7, 16, "0000 0011 ssN0 dddd", "sub rd,@rs", 0,
  "CZSV--", 9, 16, "0100 0011 0000 dddd address_src", "sub rd,address_src", 0,
  "CZSV--", 10, 16, "0100 0011 ssN0 dddd address_src", "sub rd,address_src(rs)", 0,
  "CZSV--", 7, 16, "0000 0011 0000 dddd imm16", "sub rd,imm16", 0,
  "CZSV--", 4, 16, "1000 0011 ssss dddd", "sub rd,rs", 0,

  "CZSVDH", 7, 8, "0000 0010 ssN0 dddd", "subb rbd,@rs", 0,
  "CZSVDH", 9, 8, "0100 0010 0000 dddd address_src", "subb rbd,address_src", 0,
  "CZSVDH", 10, 8, "0100 0010 ssN0 dddd address_src", "subb rbd,address_src(rs)", 0,
  "CZSVDH", 7, 8, "0000 0010 0000 dddd imm8 imm8", "subb rbd,imm8", 0,
  "CZSVDH", 4, 8, "1000 0010 ssss dddd", "subb rbd,rbs", 0,

  "CZSV--", 14, 32, "0001 0010 ssN0 dddd", "subl rrd,@rs", 0,
  "CZSV--", 15, 32, "0101 0010 0000 dddd address_src", "subl rrd,address_src", 0,
  "CZSV--", 16, 32, "0101 0010 ssN0 dddd address_src", "subl rrd,address_src(rs)", 0,
  "CZSV--", 14, 32, "0001 0010 0000 dddd imm32", "subl rrd,imm32", 0,
  "CZSV--", 8, 32, "1001 0010 ssss dddd", "subl rrd,rrs", 0,

  "------", 5, 16, "1010 1111 dddd cccc", "tcc cc,rd", 0,
  "------", 5, 8, "1010 1110 dddd cccc", "tccb cc,rbd", 0,

  "-ZS---", 8, 16, "0000 1101 ddN0 0100", "test @rd", 0,
  "------", 11, 16, "0100 1101 0000 0100 address_dst", "test address_dst", 0,
  "------", 12, 16, "0100 1101 ddN0 0100 address_dst", "test address_dst(rd)", 0,
  "------", 7, 16, "1000 1101 dddd 0100", "test rd", 0,

  "-ZSP--", 8, 8, "0000 1100 ddN0 0100", "testb @rd", 0,
  "-ZSP--", 11, 8, "0100 1100 0000 0100 address_dst", "testb address_dst", 0,
  "-ZSP--", 12, 8, "0100 1100 ddN0 0100 address_dst", "testb address_dst(rd)", 0,
  "-ZSP--", 7, 8, "1000 1100 dddd 0100", "testb rbd", 0,

  "-ZS---", 13, 32, "0001 1100 ddN0 1000", "testl @rd", 0,
  "-ZS---", 16, 32, "0101 1100 0000 1000 address_dst", "testl address_dst", 0,
  "-ZS---", 17, 32, "0101 1100 ddN0 1000 address_dst", "testl address_dst(rd)", 0,
  "-ZS---", 13, 32, "1001 1100 dddd 1000", "testl rrd", 0,

  "-ZSV--", 25, 8, "1011 1000 ddN0 1000 0000 aaaa ssN0 0000", "trdb @rd,@rs,rba", 0,
  "-ZSV--", 25, 8, "1011 1000 ddN0 1100 0000 aaaa ssN0 0000", "trdrb @rd,@rs,rba", 0,
  "-ZSV--", 25, 8, "1011 1000 ddN0 0000 0000 rrrr ssN0 0000", "trib @rd,@rs,rbr", 0,
  "-ZSV--", 25, 8, "1011 1000 ddN0 0100 0000 rrrr ssN0 0000", "trirb @rd,@rs,rbr", 0,
  "-ZSV--", 25, 8, "1011 1000 aaN0 1010 0000 rrrr bbN0 0000", "trtdb @ra,@rb,rbr", 0,
  "-ZSV--", 25, 8, "1011 1000 aaN0 1110 0000 rrrr bbN0 1110", "trtdrb @ra,@rb,rbr", 0,
  "-ZSV--", 25, 8, "1011 1000 aaN0 0010 0000 rrrr bbN0 0000", "trtib @ra,@rb,rbr", 0,
  "-ZSV--", 25, 8, "1011 1000 aaN0 0110 0000 rrrr bbN0 1110", "trtirb @ra,@rb,rbr", 0,
  "-ZSV--", 25, 8, "1011 1000 aaN0 1010 0000 rrrr bbN0 0000", "trtrb @ra,@rb,rbr", 0,

  "--S---", 11, 16, "0000 1101 ddN0 0110", "tset @rd", 0,
  "--S---", 14, 16, "0100 1101 0000 0110 address_dst", "tset address_dst", 0,
  "--S---", 15, 16, "0100 1101 ddN0 0110 address_dst", "tset address_dst(rd)", 0,
  "--S---", 7, 16, "1000 1101 dddd 0110", "tset rd", 0,

  "--S---", 11, 8, "0000 1100 ddN0 0110", "tsetb @rd", 0,
  "--S---", 14, 8, "0100 1100 0000 0110 address_dst", "tsetb address_dst", 0,
  "--S---", 15, 8, "0100 1100 ddN0 0110 address_dst", "tsetb address_dst(rd)", 0,
  "--S---", 7, 8, "1000 1100 dddd 0110", "tsetb rbd", 0,

  "-ZS---", 7, 16, "0000 1001 ssN0 dddd", "xor rd,@rs", 0,
  "-ZS---", 9, 16, "0100 1001 0000 dddd address_src", "xor rd,address_src", 0,
  "-ZS---", 10, 16, "0100 1001 ssN0 dddd address_src", "xor rd,address_src(rs)", 0,
  "-ZS---", 7, 16, "0000 1001 0000 dddd imm16", "xor rd,imm16", 0,
  "-ZS---", 4, 16, "1000 1001 ssss dddd", "xor rd,rs", 0,

  "-ZSP--", 7, 8, "0000 1000 ssN0 dddd", "xorb rbd,@rs", 0,
  "-ZSP--", 9, 8, "0100 1000 0000 dddd address_src", "xorb rbd,address_src", 0,
  "-ZSP--", 10, 8, "0100 1000 ssN0 dddd address_src", "xorb rbd,address_src(rs)", 0,
  "-ZSP--", 7, 8, "0000 1000 0000 dddd imm8 imm8", "xorb rbd,imm8", 0,
  "-ZSP--", 4, 8, "1000 1000 ssss dddd", "xorb rbd,rbs", 0,

  "------", 7, 32, "1000 1100 dddd 0001", "ldctlb rbd,ctrl", 0,
  "CZSVDH", 7, 32, "1000 1100 ssss 1001", "ldctlb ctrl,rbs", 0,

  "*", 4, 8, "1000 1000 ssss dddd", "xorb rbd,rbs", 0,
  "*", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int
count ()
{
  struct op *p = opt;
  int r = 0;

  while (p->name)
    {
      r++;
      p++;
    }
  return r;

}

static
int
func (a, b)
     struct op *a;
     struct op *b;
{
  return strcmp ((a)->name, (b)->name);
}


/* opcode

 literal  0000 nnnn insert nnn into stream
 operand  0001 nnnn  insert operand reg nnn into stream
*/

struct tok_struct
{

  char *match;
  char *token;
  int length;
};

struct tok_struct args[] =
{

  {"address_src(rs)", "CLASS_X+(ARG_RS)",},
  {"address_dst(rd)", "CLASS_X+(ARG_RD)",},

  {"rs(imm16)", "CLASS_BA+(ARG_RS)",},
  {"rd(imm16)", "CLASS_BA+(ARG_RD)",},
  {"prd",       "CLASS_PR+(ARG_RD)",},
  {"address_src", "CLASS_DA+(ARG_SRC)",},
  {"address_dst", "CLASS_DA+(ARG_DST)",},
  {"rd(rx)", "CLASS_BX+(ARG_RD)",},
  {"rs(rx)", "CLASS_BX+(ARG_RS)",},

  {"disp16", "CLASS_DISP",},
  {"disp12", "CLASS_DISP",},
  {"disp7", "CLASS_DISP",},
  {"disp8", "CLASS_DISP",},
  {"flags", "CLASS_FLAGS",},

  {"imm16", "CLASS_IMM+(ARG_IMM16)",},
  {"imm1or2", "CLASS_IMM+(ARG_IMM1OR2)",},
  {"imm32", "CLASS_IMM+(ARG_IMM32)",},
  {"imm4m1", "CLASS_IMM +(ARG_IMM4M1)",},
  {"imm4", "CLASS_IMM +(ARG_IMM4)",},
  {"n", "CLASS_IMM + (ARG_IMMN)",},
  {"ctrl", "CLASS_CTRL",},
  {"rba", "CLASS_REG_BYTE+(ARG_RA)",},
  {"rbb", "CLASS_REG_BYTE+(ARG_RB)",},
  {"rbd", "CLASS_REG_BYTE+(ARG_RD)",},
  {"rbs", "CLASS_REG_BYTE+(ARG_RS)",},
  {"rbr", "CLASS_REG_BYTE+(ARG_RR)",},

  {"rrd", "CLASS_REG_LONG+(ARG_RD)",},
  {"rrs", "CLASS_REG_LONG+(ARG_RS)",},

  {"rqd", "CLASS_REG_QUAD+(ARG_RD)",},

  {"rd", "CLASS_REG_WORD+(ARG_RD)",},
  {"rs", "CLASS_REG_WORD+(ARG_RS)",},

  {"@rd", "CLASS_IR+(ARG_RD)",},
  {"@ra", "CLASS_IR+(ARG_RA)",},
  {"@rb", "CLASS_IR+(ARG_RB)",},
  {"@rs", "CLASS_IR+(ARG_RS)",},

  {"imm8", "CLASS_IMM+(ARG_IMM8)",},
  {"i2", "CLASS_IMM+(ARG_IMM2)",},
  {"cc", "CLASS_CC",},

  {"rr", "CLASS_REG_WORD+(ARG_RR)",},
  {"ra", "CLASS_REG_WORD+(ARG_RA)",},
  {"rs", "CLASS_REG_WORD+(ARG_RS)",},

  {"1", "CLASS_IMM+(ARG_IMM_1)",},
  {"2", "CLASS_IMM+(ARG_IMM_2)",},

  0, 0
};

struct tok_struct toks[] =
{
  "0000", "CLASS_BIT+0", 1,
  "0001", "CLASS_BIT+1", 1,
  "0010", "CLASS_BIT+2", 1,
  "0011", "CLASS_BIT+3", 1,
  "0100", "CLASS_BIT+4", 1,
  "0101", "CLASS_BIT+5", 1,
  "0110", "CLASS_BIT+6", 1,
  "0111", "CLASS_BIT+7", 1,
  "1000", "CLASS_BIT+8", 1,
  "1001", "CLASS_BIT+9", 1,
  "1010", "CLASS_BIT+0xa", 1,
  "1011", "CLASS_BIT+0xb", 1,
  "1100", "CLASS_BIT+0xc", 1,
  "1101", "CLASS_BIT+0xd", 1,
  "1110", "CLASS_BIT+0xe", 1,
  "1111", "CLASS_BIT+0xf", 1,

  "00I0", "CLASS_BIT_1OR2+0", 1,
  "00I0", "CLASS_BIT_1OR2+1", 1,
  "00I0", "CLASS_BIT_1OR2+2", 1,
  "00I0", "CLASS_BIT_1OR2+3", 1,
  "01I0", "CLASS_BIT_1OR2+4", 1,
  "01I0", "CLASS_BIT_1OR2+5", 1,
  "01I0", "CLASS_BIT_1OR2+6", 1,
  "01I0", "CLASS_BIT_1OR2+7", 1,
  "10I0", "CLASS_BIT_1OR2+8", 1,
  "10I0", "CLASS_BIT_1OR2+9", 1,
  "10I0", "CLASS_BIT_1OR2+0xa", 1,
  "10I0", "CLASS_BIT_1OR2+0xb", 1,
  "11I0", "CLASS_BIT_1OR2+0xc", 1,
  "11I0", "CLASS_BIT_1OR2+0xd", 1,
  "11I0", "CLASS_BIT_1OR2+0xe", 1,
  "11I0", "CLASS_BIT_1OR2+0xf", 1,

  "ssss", "CLASS_REG+(ARG_RS)", 1,
  "dddd", "CLASS_REG+(ARG_RD)", 1,
  "aaaa", "CLASS_REG+(ARG_RA)", 1,
  "bbbb", "CLASS_REG+(ARG_RB)", 1,
  "rrrr", "CLASS_REG+(ARG_RR)", 1,

  "ssN0", "CLASS_REGN0+(ARG_RS)", 1,
  "ddN0", "CLASS_REGN0+(ARG_RD)", 1,
  "aaN0", "CLASS_REGN0+(ARG_RA)", 1,
  "bbN0", "CLASS_REGN0+(ARG_RB)", 1,
  "rrN0", "CLASS_REGN0+(ARG_RR)", 1,

  "cccc", "CLASS_CC", 1,
  "nnnn", "CLASS_IMM+(ARG_IMMN)", 1,
  "xxxx", "CLASS_REG+(ARG_RX)", 1,
  "xxN0", "CLASS_REGN0+(ARG_RX)", 1,
  "nminus1", "CLASS_IMM+(ARG_IMMNMINUS1)", 1,

  "disp16", "CLASS_DISP+(ARG_DISP16)", 4,
  "disp12", "CLASS_DISP+(ARG_DISP12)", 3,
  "flags", "CLASS_FLAGS", 1,
  "address_dst", "CLASS_ADDRESS+(ARG_DST)", 4,
  "address_src", "CLASS_ADDRESS+(ARG_SRC)", 4,
  "imm4m1", "CLASS_IMM+(ARG_IMM4M1)", 1,
  "imm4", "CLASS_IMM+(ARG_IMM4)", 1,

  "imm8", "CLASS_IMM+(ARG_IMM8)", 2,
  "imm16", "CLASS_IMM+(ARG_IMM16)", 4,
  "imm32", "CLASS_IMM+(ARG_IMM32)", 8,
  "nim8", "CLASS_IMM+(ARG_NIM8)", 2,
  "0ccc", "CLASS_0CCC", 1,
  "1ccc", "CLASS_1CCC", 1,
  "disp8", "CLASS_DISP8", 2,
  "0disp7", "CLASS_0DISP7", 2,
  "1disp7", "CLASS_1DISP7", 2,
  "01ii", "CLASS_01II", 1,
  "00ii", "CLASS_00II", 1,
  0, 0
};

char *
translate (table, x, length)
     struct tok_struct *table;
     char *x;
     int *length;
{

  int found;

  found = 0;
  while (table->match)
    {
      int l = strlen (table->match);

      if (strncmp (table->match, x, l) == 0)
	{
	  /* Got a hit */
	  printf ("%s", table->token);
	  *length += table->length;
	  return x + l;
	}

      table++;
    }
  fprintf (stderr, "Can't find %s\n", x);
  printf ("**** Can't find %s\n", x);
  while (*x)
    x++;
  return x;
}

void
chewbits (bits, length)
     char *bits;
     int *length;
{
  int n = 0;

  *length = 0;
  printf ("{");
  while (*bits)
    {
      while (*bits == ' ')
	{
	  bits++;
	}
      bits = translate (toks, bits, length);
      n++;
      printf (",");

    }
  while (n < BYTE_INFO_LEN - 1)
    {
      printf ("0,");
      n++;
    }
  printf ("}");
}


static
int
chewname (name)
     char *name;
{
  char *n;
  int nargs = 0;

  n = name;
  printf ("\"");
  while (*n && !iswhite (*n))
    {
      printf ("%c", *n);
      n++;
    }
  printf ("\",");		/* Scan the operands and make entires for
				   them -remember indirect things */

  n = name;
  printf ("OPC_");
  while (*n && !iswhite (*n))
    {
      printf ("%c", *n);
      n++;
    }
  printf (",0,{");

  while (*n)
    {
      int d;

      while (*n == ',' || iswhite (*n))
	n++;
      nargs++;
      n = translate (args, n, &d);
      printf (",");
    }
  if (nargs == 0)
    {
      printf ("0");
    }
  printf ("},");
  return nargs;
}

static 
void
sub (x, c)
     char *x;
     char c;
{
  while (*x)
    {
      if (x[0] == c && x[1] == c &&
	  x[2] == c && x[3] == c)
	{
	  x[2] = 'N';
	  x[3] = '0';
	}
      x++;
    }
}


#if 0
#define D(x) ((x) == '1' || (x) =='0')
#define M(y) (strncmp(y,x,4)==0)
printmangled (x)
     char *x;
{
  return;
  while (*x)
    {
      if (D (x[0]) && D (x[1]) && D (x[2]) && D (x[3]))
	{
	  printf ("XXXX");
	}
      else if (M ("ssss"))
	{
	  printf ("ssss");
	}
      else if (M ("dddd"))
	{
	  printf ("dddd");
	}
      else
	printf ("____");

      x += 4;

      if (x[0] == ' ')
	{
	  printf ("_");
	  x++;
	}
    }

}

#endif
/*#define WORK_TYPE*/
void
print_type (n)
     struct op *n;
{
#ifdef WORK_TYPE
  while (*s && !iswhite (*s))
    {
      l = *s;
      s++;
    }
  switch (l)
    {
    case 'l':
      printf ("32,");
      break;
    case 'b':
      printf ("8,");
      break;
    default:
      printf ("16,");
      break;
    }
#else
  printf ("%2d,", n->type);
#endif
}


void
internal ()
{
  int c = count ();
  struct op *new = (struct op *) xmalloc (sizeof (struct op) * c);
  struct op *p = opt;
  memcpy (new, p, c * sizeof (struct op));

  /* sort all names in table alphabetically */
  qsort (new, c, sizeof (struct op), func);

  p = new;
  while (p->flags[0] != '*')
  {
    /* If there are any @rs, sub the ssss into a ssn0,
       (rs), (ssn0)
       */
    int loop = 1;

    printf ("\"%s\",%2d, ", p->flags, p->cycles);
    while (loop)
    {
      char *s = p->name;

      loop = 0;
      while (*s)
      {
	if (s[0] == '@')
	{
	  char c;

	  /* skip the r and sub the string */
	  s++;
	  c = s[1];
	  sub (p->bits, c);
	}
	if (s[0] == '(' && s[3] == ')')
	{
	  sub (p->bits, s[2]);
	}
	if (s[0] == '(')
	{
	  sub (p->bits, s[-1]);
	}

	s++;
      }

    }
    print_type (p);
    printf ("\"%s\",\"%s\",0,\n", p->bits, p->name);
    p++;
  }
}

static 
void
gas ()
{
  int c = count ();
  struct op *p = opt;
  int idx = 0;
  char *oldname = "";
  struct op *new = (struct op *) xmalloc (sizeof (struct op) * c);

  memcpy (new, p, c * sizeof (struct op));

  /* sort all names in table alphabetically */
  qsort (new, c, sizeof (struct op), func);

  printf ("			/* THIS FILE IS AUTOMAGICALLY GENERATED, DON'T EDIT IT */\n");

  printf ("#define ARG_MASK 0x0f\n");

  printf ("#define ARG_SRC 0x01\n");
  printf ("#define ARG_DST 0x02\n");

  printf ("#define ARG_RS 0x01\n");
  printf ("#define ARG_RD 0x02\n");
  printf ("#define ARG_RA 0x03\n");
  printf ("#define ARG_RB 0x04\n");
  printf ("#define ARG_RR 0x05\n");
  printf ("#define ARG_RX 0x06\n");
  printf ("#define ARG_IMM4 0x01\n");
  printf ("#define ARG_IMM8 0x02\n");
  printf ("#define ARG_IMM16 0x03\n");
  printf ("#define ARG_IMM32 0x04\n");
  printf ("#define ARG_IMMN 0x05\n");
  printf ("#define ARG_IMMNMINUS1 0x05\n");
  printf ("#define ARG_IMM_1 0x06\n");
  printf ("#define ARG_IMM_2 0x07\n");
  printf ("#define ARG_DISP16 0x08\n");
  printf ("#define ARG_NIM8 0x09\n");
  printf ("#define ARG_IMM2 0x0a\n");
  printf ("#define ARG_IMM1OR2 0x0b\n");

  printf ("#define ARG_DISP12 0x0b\n");
  printf ("#define ARG_DISP8 0x0c\n");
  printf ("#define ARG_IMM4M1 0x0d\n");
  printf ("#define CLASS_MASK 0x1fff0\n");
  printf ("#define CLASS_X 0x10\n");
  printf ("#define CLASS_BA 0x20\n");
  printf ("#define CLASS_DA 0x30\n");
  printf ("#define CLASS_BX 0x40\n");
  printf ("#define CLASS_DISP 0x50\n");
  printf ("#define CLASS_IMM 0x60\n");
  printf ("#define CLASS_CC 0x70\n");
  printf ("#define CLASS_CTRL 0x80\n");
  printf ("#define CLASS_ADDRESS 0xd0\n");
  printf ("#define CLASS_0CCC 0xe0\n");
  printf ("#define CLASS_1CCC 0xf0\n");
  printf ("#define CLASS_0DISP7 0x100\n");
  printf ("#define CLASS_1DISP7 0x200\n");
  printf ("#define CLASS_01II 0x300\n");
  printf ("#define CLASS_00II 0x400\n");
  printf ("#define CLASS_BIT 0x500\n");
  printf ("#define CLASS_FLAGS 0x600\n");
  printf ("#define CLASS_IR 0x700\n");
  printf ("#define CLASS_DISP8 0x800\n");

  printf ("#define CLASS_BIT_1OR2 0x900\n");
  printf ("#define CLASS_REG 0x7000\n");
  printf ("#define CLASS_REG_BYTE 0x2000\n");
  printf ("#define CLASS_REG_WORD 0x3000\n");
  printf ("#define CLASS_REG_QUAD 0x4000\n");
  printf ("#define CLASS_REG_LONG 0x5000\n");
  printf ("#define CLASS_REGN0 0x8000\n");
  printf ("#define CLASS_PR 0x10000\n");

  printf ("#define OPC_adc 0\n");
  printf ("#define OPC_adcb 1\n");
  printf ("#define OPC_add 2\n");
  printf ("#define OPC_addb 3\n");
  printf ("#define OPC_addl 4\n");
  printf ("#define OPC_and 5\n");
  printf ("#define OPC_andb 6\n");
  printf ("#define OPC_bit 7\n");
  printf ("#define OPC_bitb 8\n");
  printf ("#define OPC_call 9\n");
  printf ("#define OPC_calr 10\n");
  printf ("#define OPC_clr 11\n");
  printf ("#define OPC_clrb 12\n");
  printf ("#define OPC_com 13\n");
  printf ("#define OPC_comb 14\n");
  printf ("#define OPC_comflg 15\n");
  printf ("#define OPC_cp 16\n");
  printf ("#define OPC_cpb 17\n");
  printf ("#define OPC_cpd 18\n");
  printf ("#define OPC_cpdb 19\n");
  printf ("#define OPC_cpdr 20\n");
  printf ("#define OPC_cpdrb 21\n");
  printf ("#define OPC_cpi 22\n");
  printf ("#define OPC_cpib 23\n");
  printf ("#define OPC_cpir 24\n");
  printf ("#define OPC_cpirb 25\n");
  printf ("#define OPC_cpl 26\n");
  printf ("#define OPC_cpsd 27\n");
  printf ("#define OPC_cpsdb 28\n");
  printf ("#define OPC_cpsdr 29\n");
  printf ("#define OPC_cpsdrb 30\n");
  printf ("#define OPC_cpsi 31\n");
  printf ("#define OPC_cpsib 32\n");
  printf ("#define OPC_cpsir 33\n");
  printf ("#define OPC_cpsirb 34\n");
  printf ("#define OPC_dab 35\n");
  printf ("#define OPC_dbjnz 36\n");
  printf ("#define OPC_dec 37\n");
  printf ("#define OPC_decb 38\n");
  printf ("#define OPC_di 39\n");
  printf ("#define OPC_div 40\n");
  printf ("#define OPC_divl 41\n");
  printf ("#define OPC_djnz 42\n");
  printf ("#define OPC_ei 43\n");
  printf ("#define OPC_ex 44\n");
  printf ("#define OPC_exb 45\n");
  printf ("#define OPC_exts 46\n");
  printf ("#define OPC_extsb 47\n");
  printf ("#define OPC_extsl 48\n");
  printf ("#define OPC_halt 49\n");
  printf ("#define OPC_in 50\n");
  printf ("#define OPC_inb 51\n");
  printf ("#define OPC_inc 52\n");
  printf ("#define OPC_incb 53\n");
  printf ("#define OPC_ind 54\n");
  printf ("#define OPC_indb 55\n");
  printf ("#define OPC_inib 56\n");
  printf ("#define OPC_inibr 57\n");
  printf ("#define OPC_iret 58\n");
  printf ("#define OPC_jp 59\n");
  printf ("#define OPC_jr 60\n");
  printf ("#define OPC_ld 61\n");
  printf ("#define OPC_lda 62\n");
  printf ("#define OPC_ldar 63\n");
  printf ("#define OPC_ldb 64\n");
  printf ("#define OPC_ldctl 65\n");
  printf ("#define OPC_ldir 66\n");
  printf ("#define OPC_ldirb 67\n");
  printf ("#define OPC_ldk 68\n");
  printf ("#define OPC_ldl 69\n");
  printf ("#define OPC_ldm 70\n");
  printf ("#define OPC_ldps 71\n");
  printf ("#define OPC_ldr 72\n");
  printf ("#define OPC_ldrb 73\n");
  printf ("#define OPC_ldrl 74\n");
  printf ("#define OPC_mbit 75\n");
  printf ("#define OPC_mreq 76\n");
  printf ("#define OPC_mres 77\n");
  printf ("#define OPC_mset 78\n");
  printf ("#define OPC_mult 79\n");
  printf ("#define OPC_multl 80\n");
  printf ("#define OPC_neg 81\n");
  printf ("#define OPC_negb 82\n");
  printf ("#define OPC_nop 83\n");
  printf ("#define OPC_or 84\n");
  printf ("#define OPC_orb 85\n");
  printf ("#define OPC_out 86\n");
  printf ("#define OPC_outb 87\n");
  printf ("#define OPC_outd 88\n");
  printf ("#define OPC_outdb 89\n");
  printf ("#define OPC_outib 90\n");
  printf ("#define OPC_outibr 91\n");
  printf ("#define OPC_pop 92\n");
  printf ("#define OPC_popl 93\n");
  printf ("#define OPC_push 94\n");
  printf ("#define OPC_pushl 95\n");
  printf ("#define OPC_res 96\n");
  printf ("#define OPC_resb 97\n");
  printf ("#define OPC_resflg 98\n");
  printf ("#define OPC_ret 99\n");
  printf ("#define OPC_rl 100\n");
  printf ("#define OPC_rlb 101\n");
  printf ("#define OPC_rlc 102\n");
  printf ("#define OPC_rlcb 103\n");
  printf ("#define OPC_rldb 104\n");
  printf ("#define OPC_rr 105\n");
  printf ("#define OPC_rrb 106\n");
  printf ("#define OPC_rrc 107\n");
  printf ("#define OPC_rrcb 108\n");
  printf ("#define OPC_rrdb 109\n");
  printf ("#define OPC_sbc 110\n");
  printf ("#define OPC_sbcb 111\n");
  printf ("#define OPC_sda 112\n");
  printf ("#define OPC_sdab 113\n");
  printf ("#define OPC_sdal 114\n");
  printf ("#define OPC_sdl 115\n");
  printf ("#define OPC_sdlb 116\n");
  printf ("#define OPC_sdll 117\n");
  printf ("#define OPC_set 118\n");
  printf ("#define OPC_setb 119\n");
  printf ("#define OPC_setflg 120\n");
  printf ("#define OPC_sinb 121\n");
  printf ("#define OPC_sind 122\n");
  printf ("#define OPC_sindb 123\n");
  printf ("#define OPC_sinib 124\n");
  printf ("#define OPC_sinibr 125\n");
  printf ("#define OPC_sla 126\n");
  printf ("#define OPC_slab 127\n");
  printf ("#define OPC_slal 128\n");
  printf ("#define OPC_sll 129\n");
  printf ("#define OPC_sllb 130\n");
  printf ("#define OPC_slll 131\n");
  printf ("#define OPC_sout 132\n");
  printf ("#define OPC_soutb 133\n");
  printf ("#define OPC_soutd 134\n");
  printf ("#define OPC_soutdb 135\n");
  printf ("#define OPC_soutib 136\n");
  printf ("#define OPC_soutibr 137\n");
  printf ("#define OPC_sra 138\n");
  printf ("#define OPC_srab 139\n");
  printf ("#define OPC_sral 140\n");
  printf ("#define OPC_srl 141\n");
  printf ("#define OPC_srlb 142\n");
  printf ("#define OPC_srll 143\n");
  printf ("#define OPC_sub 144\n");
  printf ("#define OPC_subb 145\n");
  printf ("#define OPC_subl 146\n");
  printf ("#define OPC_tcc 147\n");
  printf ("#define OPC_tccb 148\n");
  printf ("#define OPC_test 149\n");
  printf ("#define OPC_testb 150\n");
  printf ("#define OPC_testl 151\n");
  printf ("#define OPC_trdb 152\n");
  printf ("#define OPC_trdrb 153\n");
  printf ("#define OPC_trib 154\n");
  printf ("#define OPC_trirb 155\n");
  printf ("#define OPC_trtdrb 156\n");
  printf ("#define OPC_trtib 157\n");
  printf ("#define OPC_trtirb 158\n");
  printf ("#define OPC_trtrb 159\n");
  printf ("#define OPC_tset 160\n");
  printf ("#define OPC_tsetb 161\n");
  printf ("#define OPC_xor 162\n");
  printf ("#define OPC_xorb 163\n");

  printf ("#define OPC_ldd  164 \n");
  printf ("#define OPC_lddb  165 \n");
  printf ("#define OPC_lddr  166 \n");
  printf ("#define OPC_lddrb 167  \n");
  printf ("#define OPC_ldi  168 \n");
  printf ("#define OPC_ldib 169  \n");
  printf ("#define OPC_sc   170\n");
  printf ("#define OPC_bpt   171\n");
  printf ("#define OPC_ext0e 172\n");
  printf ("#define OPC_ext0f 172\n");
  printf ("#define OPC_ext8e 172\n");
  printf ("#define OPC_ext8f 172\n");
  printf ("#define OPC_rsvd36 172\n");
  printf ("#define OPC_rsvd38 172\n");
  printf ("#define OPC_rsvd78 172\n");
  printf ("#define OPC_rsvd7e 172\n");
  printf ("#define OPC_rsvd9d 172\n");
  printf ("#define OPC_rsvd9f 172\n");
  printf ("#define OPC_rsvdb9 172\n");
  printf ("#define OPC_rsvdbf 172\n");
  printf ("#define OPC_outi 173\n");
  printf ("#define OPC_ldctlb 174\n");
  printf ("#define OPC_sin 175\n");
  printf ("#define OPC_trtdb 176\n");
#if 0
  for (i = 0; toks[i].token; i++)
    printf ("#define %s\t0x%x\n", toks[i].token, i * 16);
#endif
  printf ("typedef struct {\n");

  printf ("#ifdef NICENAMES\n");
  printf ("char *nicename;\n");
  printf ("int type;\n");
  printf ("int cycles;\n");
  printf ("int flags;\n");
  printf ("#endif\n");
  printf ("char *name;\n");
  printf ("unsigned char opcode;\n");
  printf ("void (*func) PARAMS ((void));\n");
  printf ("unsigned int arg_info[4];\n");
  printf ("unsigned int byte_info[%d];\n", BYTE_INFO_LEN);
  printf ("int noperands;\n");
  printf ("int length;\n");
  printf ("int idx;\n");
  printf ("} opcode_entry_type;\n");
  printf ("#ifdef DEFINE_TABLE\n");
  printf ("opcode_entry_type z8k_table[] = {\n");

  while (new->flags && new->flags[0])
    {
      int nargs;
      int length;

      printf ("\n\n/* %s *** %s */\n", new->bits, new->name);
      printf ("{\n");

      printf ("#ifdef NICENAMES\n");
      printf ("\"%s\",%d,%d,\n", new->name, new->type, new->cycles);
      {
	int answer = 0;
	char *p = new->flags;

	while (*p)
	  {
	    answer <<= 1;

	    if (*p != '-')
	      answer |= 1;
	    p++;
	  }
	printf ("0x%02x,\n", answer);
      }

      printf ("#endif\n");

      nargs = chewname (new->name);

      printf ("\n\t");
      chewbits (new->bits, &length);
      length /= 2;
      if (length & 1)
	abort();

      printf (",%d,%d,%d", nargs, length, idx);
      idx++;
      oldname = new->name;
      printf ("},\n");
      new++;
    }
  printf ("\n/* end marker */\n");
  printf ("{\n#ifdef NICENAMES\nNULL,0,0,\n0,\n#endif\n");
  printf ("NULL,0,0,{0,0,0,0},{0,0,0,0,0,0,0,0,0,0},0,0,0}\n};\n");
  printf ("#endif\n");
}


int
main (ac, av)
     int ac;
     char **av;
{
  struct op *p = opt;

  if (ac == 2 && strcmp (av[1], "-t") == 0)
    {
      internal ();
    }
  else if (ac == 2 && strcmp (av[1], "-h") == 0)
    {
      while (p->name)
	{
	  printf ("%-25s\t%s\n", p->name, p->bits);
	  p++;
	}
    }

  else if (ac == 2 && strcmp (av[1], "-a") == 0)
    {
      gas ();
    }
  else if (ac == 2 && strcmp (av[1], "-d") == 0)
    {
      /*dis();*/
    }
  else
    {
      printf ("Usage: %s -t\n", av[0]);
      printf ("-t : generate new z8.c internal table\n");
      printf ("-a : generate new table for gas\n");
      printf ("-d : generate new table for disassemble\n");
      printf ("-h : generate new table for humans\n");
    }
  return 0;
}
