// -*- C++ -*-

/* <groff_src_dir>/src/libs/libgroff/color.cpp

Last update: 13 Apr 2003

Copyright (C) 2001, 2002, 2003 Free Software Foundation, Inc.
    Written by Gaius Mulley <gaius@glam.ac.uk>

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

#include "color.h"
#include "cset.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include "errarg.h"
#include "error.h"

static inline unsigned int
min(const unsigned int a, const unsigned int b)
{
  if (a < b)
    return a;
  else
    return b;
}

color *color::free_list = 0;

void *color::operator new(size_t n)
{
  assert(n == sizeof(color));
  if (!free_list) {
    const int BLOCK = 128;
    free_list = (color *)new char[sizeof(color)*BLOCK];
    for (int i = 0; i < BLOCK - 1; i++)
      free_list[i].next = free_list + i + 1;
    free_list[BLOCK-1].next = 0;
  }
  color *p = free_list;
  free_list = (color *)(free_list->next);
  p->next = 0;
  return p;
}

void color::operator delete(void *p)
{
  if (p) {
    ((color *)p)->next = free_list;
    free_list = (color *)p;
  }
}

color::color(const color * const c)
{
  scheme = c->scheme;
  components[0] = c->components[0];
  components[1] = c->components[1];
  components[2] = c->components[2];
  components[3] = c->components[3];
}

color::~color()
{
}

int color::operator==(const color & c) const
{
  if (scheme != c.scheme)
    return 0;
  switch (scheme) {
  case DEFAULT:
    break;
  case RGB:
    if (Red != c.Red || Green != c.Green || Blue != c.Blue)
      return 0;
    break;
  case CMYK:
    if (Cyan != c.Cyan || Magenta != c.Magenta
	|| Yellow != c.Yellow || Black != c.Black)
      return 0;
    break;
  case GRAY:
    if (Gray != c.Gray)
      return 0;
    break;
  case CMY:
    if (Cyan != c.Cyan || Magenta != c.Magenta || Yellow != c.Yellow)
      return 0;
    break;
  }
  return 1;
}

int color::operator!=(const color & c) const
{
  return !(*this == c);
}

color_scheme color::get_components(unsigned int *c) const
{
#if 0
  if (sizeof (c) < sizeof (unsigned int) * 4)
    fatal("argument is not big enough to store 4 color components");
#endif
  c[0] = components[0];
  c[1] = components[1];
  c[2] = components[2];
  c[3] = components[3];
  return scheme;
}

void color::set_default()
{
  scheme = DEFAULT;
}

// (0, 0, 0) is black

void color::set_rgb(const unsigned int r, const unsigned int g,
		    const unsigned int b)
{
  scheme = RGB;
  Red = min(MAX_COLOR_VAL, r);
  Green = min(MAX_COLOR_VAL, g);
  Blue = min(MAX_COLOR_VAL, b);
}

// (0, 0, 0) is white

void color::set_cmy(const unsigned int c, const unsigned int m,
		    const unsigned int y)
{
  scheme = CMY;
  Cyan = min(MAX_COLOR_VAL, c);
  Magenta = min(MAX_COLOR_VAL, m);
  Yellow = min(MAX_COLOR_VAL, y);
}

// (0, 0, 0, 0) is white

void color::set_cmyk(const unsigned int c, const unsigned int m,
		     const unsigned int y, const unsigned int k)
{
  scheme = CMYK;
  Cyan = min(MAX_COLOR_VAL, c);
  Magenta = min(MAX_COLOR_VAL, m);
  Yellow = min(MAX_COLOR_VAL, y);
  Black = min(MAX_COLOR_VAL, k);
}

// (0) is black

void color::set_gray(const unsigned int g)
{
  scheme = GRAY;
  Gray = min(MAX_COLOR_VAL, g);
}

/*
 *  atoh - computes the decimal value of a hexadecimal number string.
 *         `length' characters of `s' are read.  Returns 1 if successful.
 */

static int atoh(unsigned int *result,
		const char * const s, const size_t length)
{
  size_t i = 0;
  unsigned int val = 0;
  while ((i < length) && csxdigit(s[i])) {
    if (csdigit(s[i]))
      val = val*0x10 + (s[i]-'0');
    else if (csupper(s[i]))
      val = val*0x10 + (s[i]-'A') + 10;
    else
      val = val*0x10 + (s[i]-'a') + 10;
    i++;
  }
  if (i != length)
    return 0;
  *result = val;
  return 1;
}

/*
 *  read_encoding - set color from a hexadecimal color string.
 *
 *  Use color scheme `cs' to parse `n' color components from string `s'.
 *  Returns 1 if successful.
 */

int color::read_encoding(const color_scheme cs, const char * const s,
			 const size_t n)
{
  size_t hex_length = 2;
  scheme = cs;
  char *p = (char *) s;
  p++;
  if (*p == '#') {
    hex_length = 4;
    p++;
  }
  for (size_t i = 0; i < n; i++) {
    if (!atoh(&(components[i]), p, hex_length))
      return 0;
    if (hex_length == 2)
      components[i] *= 0x101;	// scale up -- 0xff should become 0xffff
    p += hex_length;
  }
  return 1;
}

int color::read_rgb(const char * const s)
{
  return read_encoding(RGB, s, 3);
}

int color::read_cmy(const char * const s)
{
  return read_encoding(CMY, s, 3);
}

int color::read_cmyk(const char * const s)
{
  return read_encoding(CMYK, s, 4);
}

int color::read_gray(const char * const s)
{
  return read_encoding(GRAY, s, 1);
}

void
color::get_rgb(unsigned int *r, unsigned int *g, unsigned int *b) const
{
  switch (scheme) {
  case RGB:
    *r = Red;
    *g = Green;
    *b = Blue;
    break;
  case CMY:
    *r = MAX_COLOR_VAL - Cyan;
    *g = MAX_COLOR_VAL - Magenta;
    *b = MAX_COLOR_VAL - Yellow;
    break;
  case CMYK:
    *r = MAX_COLOR_VAL
	 - min(MAX_COLOR_VAL,
	       Cyan * (MAX_COLOR_VAL - Black) / MAX_COLOR_VAL + Black);
    *g = MAX_COLOR_VAL
	 - min(MAX_COLOR_VAL,
	       Magenta * (MAX_COLOR_VAL - Black) / MAX_COLOR_VAL + Black);
    *b = MAX_COLOR_VAL
	 - min(MAX_COLOR_VAL,
	       Yellow * (MAX_COLOR_VAL - Black) / MAX_COLOR_VAL + Black);
    break;
  case GRAY:
    *r = *g = *b = Gray;
    break;
  default:
    assert(0);
    break;
  }
}

void
color::get_cmy(unsigned int *c, unsigned int *m, unsigned int *y) const
{
  switch (scheme) {
  case RGB:
    *c = MAX_COLOR_VAL - Red;
    *m = MAX_COLOR_VAL - Green;
    *y = MAX_COLOR_VAL - Blue;
    break;
  case CMY:
    *c = Cyan;
    *m = Magenta;
    *y = Yellow;
    break;
  case CMYK:
    *c = min(MAX_COLOR_VAL,
	     Cyan * (MAX_COLOR_VAL - Black) / MAX_COLOR_VAL + Black);
    *m = min(MAX_COLOR_VAL,
	     Magenta * (MAX_COLOR_VAL - Black) / MAX_COLOR_VAL + Black);
    *y = min(MAX_COLOR_VAL,
	     Yellow * (MAX_COLOR_VAL - Black) / MAX_COLOR_VAL + Black);
    break;
  case GRAY:
    *c = *m = *y = MAX_COLOR_VAL - Gray;
    break;
  default:
    assert(0);
    break;
  }
}

void color::get_cmyk(unsigned int *c, unsigned int *m,
		     unsigned int *y, unsigned int *k) const
{
  switch (scheme) {
  case RGB:
    *k = min(MAX_COLOR_VAL - Red,
	     min(MAX_COLOR_VAL - Green, MAX_COLOR_VAL - Blue));
    if (MAX_COLOR_VAL == *k) {
      *c = MAX_COLOR_VAL;
      *m = MAX_COLOR_VAL;
      *y = MAX_COLOR_VAL;
    }
    else {
      *c = (MAX_COLOR_VAL * (MAX_COLOR_VAL - Red - *k))
	   / (MAX_COLOR_VAL - *k);
      *m = (MAX_COLOR_VAL * (MAX_COLOR_VAL - Green - *k))
	   / (MAX_COLOR_VAL - *k);
      *y = (MAX_COLOR_VAL * (MAX_COLOR_VAL - Blue - *k))
	   / (MAX_COLOR_VAL - *k);
    }
    break;
  case CMY:
    *k = min(Cyan, min(Magenta, Yellow));
    if (MAX_COLOR_VAL == *k) {
      *c = MAX_COLOR_VAL;
      *m = MAX_COLOR_VAL;
      *y = MAX_COLOR_VAL;
    }
    else {
      *c = (MAX_COLOR_VAL * (Cyan - *k)) / (MAX_COLOR_VAL - *k);
      *m = (MAX_COLOR_VAL * (Magenta - *k)) / (MAX_COLOR_VAL - *k);
      *y = (MAX_COLOR_VAL * (Yellow - *k)) / (MAX_COLOR_VAL - *k);
    }
    break;
  case CMYK:
    *c = Cyan;
    *m = Magenta;
    *y = Yellow;
    *k = Black;
    break;
  case GRAY:
    *c = *m = *y = 0;
    *k = MAX_COLOR_VAL - Gray;
    break;
  default:
    assert(0);
    break;
  }
}

// we use `0.222r + 0.707g + 0.071b' (this is the ITU standard)
// as an approximation for gray

void color::get_gray(unsigned int *g) const
{
  switch (scheme) {
  case RGB:
    *g = (222*Red + 707*Green + 71*Blue) / 1000;
    break;
  case CMY:
    *g = MAX_COLOR_VAL - (222*Cyan + 707*Magenta + 71*Yellow) / 1000;
    break;
  case CMYK:
    *g = (MAX_COLOR_VAL - (222*Cyan + 707*Magenta + 71*Yellow) / 1000)
	 * (MAX_COLOR_VAL - Black);
    break;
  case GRAY:
    *g = Gray;
    break;
  default:
    assert(0);
    break;
  }
}

char *color::print_color()
{
  char *s = new char[30];
  switch (scheme) {
  case DEFAULT:
    sprintf(s, "default");
    break;
  case RGB:
    sprintf(s, "rgb %.2ff %.2ff %.2ff",
	    double(Red) / MAX_COLOR_VAL,
	    double(Green) / MAX_COLOR_VAL,
	    double(Blue) / MAX_COLOR_VAL);
    break;
  case CMY:
    sprintf(s, "cmy %.2ff %.2ff %.2ff",
	    double(Cyan) / MAX_COLOR_VAL,
	    double(Magenta) / MAX_COLOR_VAL,
	    double(Yellow) / MAX_COLOR_VAL);
    break;
  case CMYK:
    sprintf(s, "cmyk %.2ff %.2ff %.2ff %.2ff",
	    double(Cyan) / MAX_COLOR_VAL,
	    double(Magenta) / MAX_COLOR_VAL,
	    double(Yellow) / MAX_COLOR_VAL,
	    double(Black) / MAX_COLOR_VAL);
    break;
  case GRAY:
    sprintf(s, "gray %.2ff",
	    double(Gray) / MAX_COLOR_VAL);
    break;
  }
  return s;
}

color default_color;
