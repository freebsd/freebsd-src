/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: snprintf.c,v 1.13 1997/05/25 02:00:31 assar Exp $");
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <roken.h>

/*
 * Common state
 */

struct state {
  char *str;
  char *s;
  char *theend;
  size_t sz;
  size_t max_sz;
  int (*append_char)(struct state *, char);
  int (*reserve)(struct state *, size_t);
  /* XXX - methods */
};

static int
sn_reserve (struct state *state, size_t n)
{
  return state->s + n > state->theend;
}

static int
sn_append_char (struct state *state, char c)
{
  if (sn_reserve (state, 1)) {
    *state->s++ = '\0';
    return 1;
  } else {
    *state->s++ = c;
    return 0;
  }
}

static int
as_reserve (struct state *state, size_t n)
{
  while (state->s + n > state->theend) {
    int off = state->s - state->str;
    char *tmp;

    if (state->max_sz && state->sz >= state->max_sz)
      return 1;

    if (state->max_sz)
      state->sz = min(state->max_sz, state->sz*2);
    else
      state->sz *= 2;
    tmp = realloc (state->str, state->sz);
    if (tmp == NULL)
      return 1;
    state->str = tmp;
    state->s = state->str + off;
    state->theend = state->str + state->sz - 1;
  }
  return 0;
}

static int
as_append_char (struct state *state, char c)
{
  if(as_reserve (state, 1))
    return 1;
  else {
    *state->s++ = c;
    return 0;
  }
}

static int
append_number (struct state *state,
	       unsigned long num, unsigned base, char *rep,
	       int width, int zerop, int minusp)
{
  int i, len;

  len = 0;
  if (num == 0) {
    ++len;
    if((*state->append_char) (state, '0'))
      return 1;
  }
  while (num > 0) {
    ++len;
    if ((*state->append_char) (state, rep[num % base]))
      return 1;
    num /= base;
  }
  if (minusp) {
    ++len;
    if ((*state->append_char) (state, '-'))
      return 1;
  }

  for (i = 0; i < len / 2; ++i) {
    char c;

    c = state->s[-i-1];
    state->s[-i-1] = state->s[-len+i];
    state->s[-len+i] = c;
  }

  if (width > len) {
    if ((*state->reserve) (state, width - len))
      return 1;

#ifdef HAVE_MEMMOVE
    memmove (state->s + width - 2 * len, state->s - len, len);
#else
    bcopy (state->s - len, state->s + width - 2 * len, len);
#endif
    for (i = 0; i < width - len; ++i)
      state->s[-len+i] = (zerop ? '0' : ' ');
    state->s += width - len;

  }
  return 0;
}

static int
append_string (struct state *state,
	       char *arg,
	       int prec)
{
  if (prec) {
    while (*arg && prec--)
      if ((*state->append_char) (state, *arg++))
	return 1;
  } else {
    while (*arg)
      if ((*state->append_char) (state, *arg++))
	return 1;
  }
  return 0;
}

/*
 * This can't be made into a function...
 */

#define PARSE_INT_FORMAT(res, arg, unsig) \
if (long_flag) \
     res = va_arg(arg, unsig long); \
else if (short_flag) \
     res = va_arg(arg, unsig short); \
else \
     res = va_arg(arg, unsig int)

/*
 * zyxprintf - return 0 or -1
 */

static int
xyzprintf (struct state *state, const char *format, va_list ap)
{
  char c;

  while((c = *format++)) {
    if (c == '%') {
      int zerop      = 0;
      int width      = 0;
      int prec       = 0;
      int long_flag  = 0;
      int short_flag = 0;

      c = *format++;

      /* flags */
      if (c == '0') {
	zerop = 1;
	c = *format++;
      }

      /* width */
      if (isdigit(c))
	do {
	  width = width * 10 + c - '0';
	  c = *format++;
	} while(isdigit(c));
      else if(c == '*') {
	width = va_arg(ap, int);
	c = *format++;
      }

      /* precision */
      if (c == '.') {
	c = *format++;
	if (isdigit(c))
	  do {
	    prec = prec * 10 + c - '0';
	    c = *format++;
	  } while(isdigit(c));
	else if (c == '*') {
	  prec = va_arg(ap, int);
	  c = *format++;
	}
      }

      /* size */

      if (c == 'h') {
	short_flag = 1;
	c = *format++;
      } else if (c == 'l') {
	long_flag = 1;
	c = *format++;
      }

      switch (c) {
      case 'c' :
	if ((*state->append_char)(state, (unsigned char)va_arg(ap, int)))
	  return -1;
	break;
      case 's' :
	if (append_string(state,
			  va_arg(ap, char*),
			  prec))
	  return -1;
	break;
      case 'd' :
      case 'i' : {
	long arg;
	unsigned long num;
	int minusp = 0;

	PARSE_INT_FORMAT(arg, ap, );

	if (arg < 0) {
	  minusp = 1;
	  num = -arg;
	} else
	  num = arg;

	if (append_number (state, num, 10, "0123456789",
			   width, zerop, minusp))
	  return -1;
	break;
      }
      case 'u' : {
	unsigned long arg;

	PARSE_INT_FORMAT(arg, ap, unsigned);

	if (append_number (state, arg, 10, "0123456789",
			   width, zerop, 0))
	  return -1;
	break;
      }
      case 'o' : {
	unsigned long arg;

	PARSE_INT_FORMAT(arg, ap, unsigned);

	if (append_number (state, arg, 010, "01234567",
			   width, zerop, 0))
	  return -1;
	break;
      }
      case 'x' : {
	unsigned long arg;

	PARSE_INT_FORMAT(arg, ap, unsigned);

	if (append_number (state, arg, 0x10, "0123456789abcdef",
			   width, zerop, 0))
	  return -1;
	break;
      }
      case 'X' :{
	unsigned long arg;

	PARSE_INT_FORMAT(arg, ap, unsigned);

	if (append_number (state, arg, 0x10, "0123456789ABCDEF",
			   width, zerop, 0))
	  return -1;
	break;
      }
      case 'p' : {
	unsigned long arg = (unsigned long)va_arg(ap, void*);

	if (append_number (state, arg, 0x10, "0123456789ABCDEF",
			   width, zerop, 0))
	  return -1;
	break;
      }
      case '%' :
	if ((*state->append_char)(state, c))
	  return -1;
	break;
      default :
	if (   (*state->append_char)(state, '%')
	    || (*state->append_char)(state, c))
	  return -1;
	break;
      }
    } else
      if ((*state->append_char) (state, c))
	return -1;
  }
  return 0;
}

#ifndef HAVE_SNPRINTF
int
snprintf (char *str, size_t sz, const char *format, ...)
{
  va_list args;
  int ret;

  va_start(args, format);
  ret = vsnprintf (str, sz, format, args);

#ifdef PARANOIA
  {
    int ret2;
    char *tmp;

    tmp = malloc (sz);
    if (tmp == NULL)
      abort ();

    ret2 = vsprintf (tmp, format, args);
    if (ret != ret2 || strcmp(str, tmp))
      abort ();
    free (tmp);
  }
#endif

  va_end(args);
  return ret;
}
#endif

#ifndef HAVE_ASPRINTF
int
asprintf (char **ret, const char *format, ...)
{
  va_list args;
  int val;

  va_start(args, format);
  val = vasprintf (ret, format, args);

#ifdef PARANOIA
  {
    int ret2;
    char *tmp;
    tmp = malloc (val + 1);
    if (tmp == NULL)
      abort ();

    ret2 = vsprintf (tmp, format, args);
    if (val != ret2 || strcmp(*ret, tmp))
      abort ();
    free (tmp);
  }
#endif

  va_end(args);
  return val;
}
#endif

#ifndef HAVE_ASNPRINTF
int
asnprintf (char **ret, size_t max_sz, const char *format, ...)
{
  va_list args;
  int val;

  va_start(args, format);
  val = vasnprintf (ret, max_sz, format, args);

#ifdef PARANOIA
  {
    int ret2;
    char *tmp;
    tmp = malloc (val + 1);
    if (tmp == NULL)
      abort ();

    ret2 = vsprintf (tmp, format, args);
    if (val != ret2 || strcmp(*ret, tmp))
      abort ();
    free (tmp);
  }
#endif

  va_end(args);
  return val;
}
#endif

#ifndef HAVE_VASPRINTF
int
vasprintf (char **ret, const char *format, va_list args)
{
  return vasnprintf (ret, 0, format, args);
}
#endif


#ifndef HAVE_VASNPRINTF
int
vasnprintf (char **ret, size_t max_sz, const char *format, va_list args)
{
  int st;
  size_t len;
  struct state state;

  state.max_sz = max_sz;
  if (max_sz)
    state.sz   = min(1, max_sz);
  else
    state.sz   = 1;
  state.str    = malloc(state.sz);
  if (state.str == NULL) {
    *ret = NULL;
    return -1;
  }
  state.s = state.str;
  state.theend = state.s + state.sz - 1;
  state.append_char = as_append_char;
  state.reserve     = as_reserve;

  st = xyzprintf (&state, format, args);
  if (st) {
    free (state.str);
    *ret = NULL;
    return -1;
  } else {
    char *tmp;

    *state.s = '\0';
    len = state.s - state.str;
    tmp = realloc (state.str, len+1);
    if (state.str == NULL) {
      free (state.str);
      *ret = NULL;
      return -1;
    }
    *ret = tmp;
    return len;
  }
}
#endif

#ifndef HAVE_VSNPRINTF
int
vsnprintf (char *str, size_t sz, const char *format, va_list args)
{
  struct state state;
  int ret;

  state.max_sz = 0;
  state.sz     = sz;
  state.str    = str;
  state.s      = str;
  state.theend = str + sz - 1;
  state.append_char = sn_append_char;
  state.reserve     = sn_reserve;

  ret = xyzprintf (&state, format, args);
  *state.s = '\0';
  if (ret)
    return sz;
  else
    return state.s - state.str;
}
#endif

