// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "guess.h"

void guess(const char *s, const font_params &param, char_metric *metric)
{
  int &height = metric->height;
  int &depth = metric->depth;

  metric->ic = 0;
  metric->left_ic = 0;
  metric->sk = 0;
  height = 0;
  depth = 0;
  if (s[0] == '\0' || (s[1] != '\0' && s[2] != '\0'))
    goto do_default;
#define HASH(c1, c2) (((unsigned char)(c1) << 8) | (unsigned char)(c2))
  switch (HASH(s[0], s[1])) {
  default:
  do_default:
    if (metric->type & 01)
      depth = param.desc_depth;
    if (metric->type & 02)
      height = param.asc_height;
    else
      height = param.x_height;
    break;
  case HASH('\\', '|'):
  case HASH('\\', '^'):
  case HASH('\\', '&'):
    // these have zero height and depth
    break;
  case HASH('f', 0):
    height = param.asc_height;
    if (param.italic)
      depth = param.desc_depth;
    break;
  case HASH('a', 0):
  case HASH('c', 0):
  case HASH('e', 0):
  case HASH('m', 0):
  case HASH('n', 0):
  case HASH('o', 0):
  case HASH('r', 0):
  case HASH('s', 0):
  case HASH('u', 0):
  case HASH('v', 0):
  case HASH('w', 0):
  case HASH('x', 0):
  case HASH('z', 0):
    height = param.x_height;
    break;
  case HASH('i', 0):
    height = param.x_height;
    break;
  case HASH('b', 0):
  case HASH('d', 0):
  case HASH('h', 0):
  case HASH('k', 0):
  case HASH('l', 0):
  case HASH('F', 'i'):
  case HASH('F', 'l'):
  case HASH('f', 'f'):
  case HASH('f', 'i'):
  case HASH('f', 'l'):
    height = param.asc_height;
    break;
  case HASH('t', 0):
    height = param.asc_height;
    break;
  case HASH('g', 0):
  case HASH('p', 0):
  case HASH('q', 0):
  case HASH('y', 0):
    height = param.x_height;
    depth = param.desc_depth;
    break;
  case HASH('j', 0):
    height = param.x_height;
    depth = param.desc_depth;
    break;
  case HASH('A', 0):
  case HASH('B', 0):
  case HASH('C', 0):
  case HASH('D', 0):
  case HASH('E', 0):
  case HASH('F', 0):
  case HASH('G', 0):
  case HASH('H', 0):
  case HASH('I', 0):
  case HASH('J', 0):
  case HASH('K', 0):
  case HASH('L', 0):
  case HASH('M', 0):
  case HASH('N', 0):
  case HASH('O', 0):
  case HASH('P', 0):
  case HASH('Q', 0):
  case HASH('R', 0):
  case HASH('S', 0):
  case HASH('T', 0):
  case HASH('U', 0):
  case HASH('V', 0):
  case HASH('W', 0):
  case HASH('X', 0):
  case HASH('Y', 0):
  case HASH('Z', 0):
    height = param.cap_height;
    break;
  case HASH('*', 'A'):
  case HASH('*', 'B'):
  case HASH('*', 'C'):
  case HASH('*', 'D'):
  case HASH('*', 'E'):
  case HASH('*', 'F'):
  case HASH('*', 'G'):
  case HASH('*', 'H'):
  case HASH('*', 'I'):
  case HASH('*', 'K'):
  case HASH('*', 'L'):
  case HASH('*', 'M'):
  case HASH('*', 'N'):
  case HASH('*', 'O'):
  case HASH('*', 'P'):
  case HASH('*', 'Q'):
  case HASH('*', 'R'):
  case HASH('*', 'S'):
  case HASH('*', 'T'):
  case HASH('*', 'U'):
  case HASH('*', 'W'):
  case HASH('*', 'X'):
  case HASH('*', 'Y'):
  case HASH('*', 'Z'):
    height = param.cap_height;
    break;
  case HASH('0', 0):
  case HASH('1', 0):
  case HASH('2', 0):
  case HASH('3', 0):
  case HASH('4', 0):
  case HASH('5', 0):
  case HASH('6', 0):
  case HASH('7', 0):
  case HASH('8', 0):
  case HASH('9', 0):
  case HASH('1', '2'):
  case HASH('1', '4'):
  case HASH('3', '4'):
    height = param.fig_height;
    break;
  case HASH('(', 0):
  case HASH(')', 0):
  case HASH('[', 0):
  case HASH(']', 0):
  case HASH('{', 0):
  case HASH('}', 0):
    height = param.body_height;
    depth = param.body_depth;
    break;
  case HASH('i', 's'):
    height = (param.em*3)/4;
    depth = param.em/4;
    break;
  case HASH('*', 'a'):
  case HASH('*', 'e'):
  case HASH('*', 'i'):
  case HASH('*', 'k'):
  case HASH('*', 'n'):
  case HASH('*', 'o'):
  case HASH('*', 'p'):
  case HASH('*', 's'):
  case HASH('*', 't'):
  case HASH('*', 'u'):
  case HASH('*', 'w'):
    height = param.x_height;
    break;
  case HASH('*', 'd'):
  case HASH('*', 'l'):
    height = param.asc_height;
    break;
  case HASH('*', 'g'):
  case HASH('*', 'h'):
  case HASH('*', 'm'):
  case HASH('*', 'r'):
  case HASH('*', 'x'):
  case HASH('*', 'y'):
    height = param.x_height;
    depth = param.desc_depth;
    break;
  case HASH('*', 'b'):
  case HASH('*', 'c'):
  case HASH('*', 'f'):
  case HASH('*', 'q'):
  case HASH('*', 'z'):
    height = param.asc_height;
    depth = param.desc_depth;
    break;
  case HASH('t', 's'):
    height = param.x_height;
    depth = param.desc_depth;
    break;
  case HASH('!', 0):
  case HASH('?', 0):
  case HASH('"', 0):
  case HASH('#', 0):
  case HASH('$', 0):
  case HASH('%', 0):
  case HASH('&', 0):
  case HASH('*', 0):
  case HASH('+', 0):
    height = param.asc_height;
    break;
  case HASH('`', 0):
  case HASH('\'', 0):
    height = param.asc_height;
    break;
  case HASH('~', 0):
  case HASH('^', 0):
  case HASH('a', 'a'):
  case HASH('g', 'a'):
    height = param.asc_height;
    break;
  case HASH('r', 'u'):
  case HASH('.', 0):
    break;
  case HASH(',', 0):
    depth = param.comma_depth;
    break;
  case HASH('m', 'i'):
  case HASH('-', 0):
  case HASH('h', 'y'):
  case HASH('e', 'm'):
    height = param.x_height;
    break;
  case HASH(':', 0):
    height = param.x_height;
    break;
  case HASH(';', 0):
    height = param.x_height;
    depth = param.comma_depth;
    break;
  case HASH('=', 0):
  case HASH('e', 'q'):
    height = param.x_height;
    break;
  case HASH('<', 0):
  case HASH('>', 0):
  case HASH('>', '='):
  case HASH('<', '='):
  case HASH('@', 0):
  case HASH('/', 0):
  case HASH('|', 0):
  case HASH('\\', 0):
    height = param.asc_height;
    break;
  case HASH('_', 0):
  case HASH('u', 'l'):
  case HASH('\\', '_'):
    depth = param.em/4;
    break;
  case HASH('r', 'n'):
    height = (param.em*3)/4;
    break;
  case HASH('s', 'r'):
    height = (param.em*3)/4;
    depth = param.em/4;
    break;
  case HASH('b', 'u'):
  case HASH('s', 'q'):
  case HASH('d', 'e'):
  case HASH('d', 'g'):
  case HASH('f', 'm'):
  case HASH('c', 't'):
  case HASH('r', 'g'):
  case HASH('c', 'o'):
  case HASH('p', 'l'):
  case HASH('*', '*'):
  case HASH('s', 'c'):
  case HASH('s', 'l'):
  case HASH('=', '='):
  case HASH('~', '='):
  case HASH('a', 'p'):
  case HASH('!', '='):
  case HASH('-', '>'):
  case HASH('<', '-'):
  case HASH('u', 'a'):
  case HASH('d', 'a'):
  case HASH('m', 'u'):
  case HASH('d', 'i'):
  case HASH('+', '-'):
  case HASH('c', 'u'):
  case HASH('c', 'a'):
  case HASH('s', 'b'):
  case HASH('s', 'p'):
  case HASH('i', 'b'):
  case HASH('i', 'p'):
  case HASH('i', 'f'):
  case HASH('p', 'd'):
  case HASH('g', 'r'):
  case HASH('n', 'o'):
  case HASH('p', 't'):
  case HASH('e', 's'):
  case HASH('m', 'o'):
  case HASH('b', 'r'):
  case HASH('d', 'd'):
  case HASH('r', 'h'):
  case HASH('l', 'h'):
  case HASH('o', 'r'):
  case HASH('c', 'i'):
    height = param.asc_height;
    break;
  case HASH('l', 't'):
  case HASH('l', 'b'):
  case HASH('r', 't'):
  case HASH('r', 'b'):
  case HASH('l', 'k'):
  case HASH('r', 'k'):
  case HASH('b', 'v'):
  case HASH('l', 'f'):
  case HASH('r', 'f'):
  case HASH('l', 'c'):
  case HASH('r', 'c'):
    height = (param.em*3)/4;
    depth = param.em/4;
    break;
#if 0
  case HASH('%', '0'):
  case HASH('-', '+'):
  case HASH('-', 'D'):
  case HASH('-', 'd'):
  case HASH('-', 'd'):
  case HASH('-', 'h'):
  case HASH('.', 'i'):
  case HASH('.', 'j'):
  case HASH('/', 'L'):
  case HASH('/', 'O'):
  case HASH('/', 'l'):
  case HASH('/', 'o'):
  case HASH('=', '~'):
  case HASH('A', 'E'):
  case HASH('A', 'h'):
  case HASH('A', 'N'):
  case HASH('C', 's'):
  case HASH('D', 'o'):
  case HASH('F', 'c'):
  case HASH('F', 'o'):
  case HASH('I', 'J'):
  case HASH('I', 'm'):
  case HASH('O', 'E'):
  case HASH('O', 'f'):
  case HASH('O', 'K'):
  case HASH('O', 'm'):
  case HASH('O', 'R'):
  case HASH('P', 'o'):
  case HASH('R', 'e'):
  case HASH('S', '1'):
  case HASH('S', '2'):
  case HASH('S', '3'):
  case HASH('T', 'P'):
  case HASH('T', 'p'):
  case HASH('Y', 'e'):
  case HASH('\\', '-'):
  case HASH('a', '"'):
  case HASH('a', '-'):
  case HASH('a', '.'):
  case HASH('a', '^'):
  case HASH('a', 'b'):
  case HASH('a', 'c'):
  case HASH('a', 'd'):
  case HASH('a', 'e'):
  case HASH('a', 'h'):
  case HASH('a', 'o'):
  case HASH('a', 't'):
  case HASH('a', '~'):
  case HASH('b', 'a'):
  case HASH('b', 'b'):
  case HASH('b', 's'):
  case HASH('c', '*'):
  case HASH('c', '+'):
  case HASH('f', '/'):
  case HASH('f', 'a'):
  case HASH('f', 'c'):
  case HASH('f', 'o'):
  case HASH('h', 'a'):
  case HASH('h', 'o'):
  case HASH('i', 'j'):
  case HASH('l', 'A'):
  case HASH('l', 'B'):
  case HASH('l', 'C'):
  case HASH('m', 'd'):
  case HASH('n', 'c'):
  case HASH('n', 'e'):
  case HASH('n', 'm'):
  case HASH('o', 'A'):
  case HASH('o', 'a'):
  case HASH('o', 'e'):
  case HASH('o', 'q'):
  case HASH('p', 'l'):
  case HASH('p', 'p'):
  case HASH('p', 's'):
  case HASH('r', '!'):
  case HASH('r', '?'):
  case HASH('r', 'A'):
  case HASH('r', 'B'):
  case HASH('r', 'C'):
  case HASH('r', 's'):
  case HASH('s', 'h'):
  case HASH('s', 's'):
  case HASH('t', 'e'):
  case HASH('t', 'f'):
  case HASH('t', 'i'):
  case HASH('t', 'm'):
  case HASH('~', '~'):
  case HASH('v', 'S'):
  case HASH('v', 'Z'):
  case HASH('v', 's'):
  case HASH('v', 'z'):
  case HASH('^', 'A'):
  case HASH('^', 'E'):
  case HASH('^', 'I'):
  case HASH('^', 'O'):
  case HASH('^', 'U'):
  case HASH('^', 'a'):
  case HASH('^', 'e'):
  case HASH('^', 'i'):
  case HASH('^', 'o'):
  case HASH('^', 'u'):
  case HASH('`', 'A'):
  case HASH('`', 'E'):
  case HASH('`', 'I'):
  case HASH('`', 'O'):
  case HASH('`', 'U'):
  case HASH('`', 'a'):
  case HASH('`', 'e'):
  case HASH('`', 'i'):
  case HASH('`', 'o'):
  case HASH('`', 'u'):
  case HASH('~', 'A'):
  case HASH('~', 'N'):
  case HASH('~', 'O'):
  case HASH('~', 'a'):
  case HASH('~', 'n'):
  case HASH('~', 'o'):
  case HASH('\'', 'A'):
  case HASH('\'', 'C'):
  case HASH('\'', 'E'):
  case HASH('\'', 'I'):
  case HASH('\'', 'O'):
  case HASH('\'', 'U'):
  case HASH('\'', 'a'):
  case HASH('\'', 'c'):
  case HASH('\'', 'e'):
  case HASH('\'', 'i'):
  case HASH('\'', 'o'):
  case HASH('\'', 'u')
  case HASH(':', 'A'):
  case HASH(':', 'E'):
  case HASH(':', 'I'):
  case HASH(':', 'O'):
  case HASH(':', 'U'):
  case HASH(':', 'Y'):
  case HASH(':', 'a'):
  case HASH(':', 'e'):
  case HASH(':', 'i'):
  case HASH(':', 'o'):
  case HASH(':', 'u'):
  case HASH(':', 'y'):
  case HASH(',', 'C'):
  case HASH(',', 'c'):
#endif
  }
}
