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
RCSID("$Id: base64.c,v 1.7 1997/04/01 08:18:16 joda Exp $");
#endif

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "base64.h"

static char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int POS(char c)
{
  if(c == '=') return 64;
  if(isupper(c))
    return c - 'A';
  if(islower(c))
    return c - 'a' + 26;
  if(isdigit(c))
    return c - '0' + 52;
  if(c == '+')
    return 62;
  if(c == '/')
    return 63;
  return -1;
}

char *base64_encode(const void *buf, int size)
{
  char *str = (char*)malloc((size+3)*4/3+1);
  char *p=str;
  unsigned char *q = (unsigned char*)buf;
  int i;
  int c;
  i=0;
  while(i<size){
    c=q[i++];
    c*=256;
    if(i<size)
      c+=q[i];
    i++;
    c*=256;
    if(i<size)
      c+=q[i];
    i++;
    p[0]=base64[(c&0x00fc0000) >> 18];
    p[1]=base64[(c&0x0003f000) >> 12];
    p[2]=base64[(c&0x00000fc0) >> 6];
    p[3]=base64[(c&0x0000003f) >> 0];
    if(i>size)
      p[3]='=';
    if(i>size+1)
      p[2]='=';
    p+=4;
  }
  *p=0;
  return str;
}

/* convert string in s to binary data. s should be a multiple of 4
 * bytes long. data should be at least len(s) * 3 / 4 bytes long.
 * returns 
 */
int base64_decode(char *s, void *data)
{
  char *p;
  unsigned char *q;
  int n[4];

  if(strlen(s) % 4)
    return -1;
  q=(unsigned char*)data;
  for(p=s; *p; p+=4){
    n[0] = POS(p[0]);
    n[1] = POS(p[1]);
    n[2] = POS(p[2]);
    n[3] = POS(p[3]);
    if((n[0] | n[1] | n[2] | n[3]) < 0)
      return -1;

    if(n[0] == 64 || n[1] == 64)
      return -1;
    if(n[2] == 64 && n[3] < 64)
      return -1;
    q[0] = (n[0] << 2) + (n[1] >> 4);
    if(n[2] < 64){
      q[1] = ((n[1] & 15) << 4) + (n[2] >> 2);
    }
    if(n[3] < 64){
      q[2] = ((n[2] & 3) << 6) + n[3];
    }
    q+=3;
  }
  q -= (n[2] == 64) + (n[3] == 64);
  return q - (unsigned char*)data;
}

#ifdef TEST
int main(int argc, char **argv)
{
  char str[128];
  char buf[128];
  char *p;
  printf("base64_encode(\"%s\") = \"%s\"\n", argv[1], 
	 p=base64_encode(argv[1], strlen(argv[1])));
  printf("base64_decode(\"%s\") = %d", p, base64_decode(p, buf));
  printf(" (\"%s\")\n", buf);
  printf("base64_decode(\"%s\") = %d", argv[1], base64_decode(argv[1], buf));
  printf(" (\"%s\")\n", buf);
}
#endif
