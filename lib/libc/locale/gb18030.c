/*
 * Copyright (c) 2003
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is contributed to Robin Hu <huxw@knight.6test.edu.cn>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <rune.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

rune_t	_GB18030_sgetrune(const char *, size_t, char const **);
int	_GB18030_sputrune(rune_t, char *, size_t, char **);

int
_GB18030_init(rl)
	_RuneLocale *rl;
{
	rl->sgetrune = _GB18030_sgetrune;
	rl->sputrune = _GB18030_sputrune;
	_CurrentRuneLocale = rl;
	__mb_cur_max = 4;
	return (0);
}

static inline int
_gb18030_check_string(s_, n)
     const char* s_;
     int n;
{  
  const unsigned char* s = s_;
  if ((s[0]>=0x81&&s[0]<=0xfe)) {
    if (n<2) goto bad_string;
    if ((s[1]>=0x40&&s[1]<=0x7e)||(s[1]>=0x80&&s[1]<=0xfe))
      return 2;
    if ((s[1]>=0x30&&s[1]<=0x39)) {
      if (n<4) goto bad_string;
      if ((s[2]>=0x81&&s[2]<=0xfe) && (s[3]>=0x30&&s[3]<=0x39))
        return 4;
      else
        goto bad_string;
    }
  } else {
    return 1;
  }
 bad_string:
  return -1;
}

static inline int
_gb18030_check_rune(r)
     rune_t r;
{
  if (r&0xff000000) {
      return 4;
  }
  if (r&0xff00) {
      return 2;
  }
  return 1;
}

rune_t
_GB18030_sgetrune(string, n, result)
	const char *string;
	size_t n;
	char const **result;
{
  rune_t rune = 0;
  int len;

  len = _gb18030_check_string(string, n);

  if (len == -1) {
    if (result)
      *result = string;
    return (_INVALID_RUNE);
  }
  
  while (--len >= 0)
    rune = (rune << 8) | ((u_int)(*string++) & 0xff);

  rune &= 0x7fffffff;
  if (result)
    *result = string;
  return rune;
}

int
_GB18030_sputrune(c, string, n, result)
	rune_t c;
	char *string, **result;
	size_t n;
{
  int len;
  len = _gb18030_check_rune(c);

  switch (len) {
  case 1:
    if (n >= 1) {  
      *string = c & 0xff;
      if (result)
        *result = string + 1;
      return (1);
    }
    break;
  case 2:
    if (n >= 2) {
      string[0] = (c >> 8) & 0xff;
      string[1] = c & 0xff;
      if (result)
        *result = string + 2;
      return (2);
    }
    break;
  case 4:
    if (n >= 4) {
      string[0] = ((c >>24) & 0xff) | 0x80;
      string[1] = (c >>16) & 0xff;
      string[2] = (c >>8)  & 0xff;
      string[3] = c & 0xff;
      if (result)
        *result = string + 4;
      return (4);
    }
    break;
  default:
    break;
  }
  if (result)
    *result = string;
  return (0);
}
