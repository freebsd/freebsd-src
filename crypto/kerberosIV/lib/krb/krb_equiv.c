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

/*
 * int krb_equiv(u_int32_t ipaddr_a, u_int32_t ipaddr_b);
 *
 * Given two IP adresses return true if they match
 * or are considered to belong to the same host.
 *
 * For example if /etc/krb.equiv looks like
 *
 *    130.237.223.3   192.16.126.3    # alv alv1
 *    130.237.223.4   192.16.126.4    # byse byse1
 *    130.237.228.152 192.16.126.9    # topsy topsy1
 *
 * krb_equiv(alv, alv1) would return true but
 * krb_equiv(alv, byse1) would not.
 *
 * A comment starts with an '#' and ends with '\n'.
 *
 */
#include "krb_locl.h"

RCSID("$Id: krb_equiv.c,v 1.13 1997/04/01 08:18:33 joda Exp $");

int krb_ignore_ip_address = 0;

int
krb_equiv(u_int32_t a, u_int32_t b)
{
  FILE *fil;
  char line[256];
  int hit_a, hit_b;
  int iscomment;
  
  if (a == b)			/* trivial match, also the common case */
    return 1;
  
  if (krb_ignore_ip_address)
    return 1;			/* if we have decided not to compare */

  a = ntohl(a);
  b = ntohl(b);

  fil = fopen(KRB_EQUIV, "r");
  if (fil == NULL)		/* open failed */
    return 0;
  
  hit_a = hit_b = 0;
  iscomment = 0;
  while (fgets(line, sizeof(line)-1, fil) != NULL) /* for each line */
    {
      char *t = line;
      int len = strlen(t);
      
      /* for each item on this line */
      while (*t != 0)		/* more addresses on this line? */
	if (*t == '\n') {
	  iscomment = hit_a = hit_b = 0;
	  break;
	} else if (iscomment)
	  t = line + len - 1;
	else if (*t == '#') {		/* rest is comment */
	  iscomment = 1;
	  ++t;
	} else if (*t == '\\' ) /* continuation */
	  break;
	else if (isspace(*t))	/* skip space */
	  t++;
	else if (isdigit(*t))	/* an address? */
	  {
	    u_int32_t tmp;
	    u_int32_t tmpa, tmpb, tmpc, tmpd;
	    
	    sscanf(t, "%d.%d.%d.%d", &tmpa, &tmpb, &tmpc, &tmpd);
	    tmp = (tmpa << 24) | (tmpb << 16) | (tmpc << 8) | tmpd;

	    while (*t == '.' || isdigit(*t)) /* done with this address */
	      t++;

	    if (tmp != -1) {	/* an address (and not broadcast) */
	      u_int32_t mask = (u_int32_t)~0;

	      if (*t == '/') {
		++t;
		mask <<= 32 - atoi(t);

		while(isdigit(*t))
		  ++t;
	      }

	      if ((tmp & mask) == (a & mask))
		hit_a = 1;
	      if ((tmp & mask) == (b & mask))
		hit_b = 1;
	      if (hit_a && hit_b) {
		fclose(fil);
		return 1;
	      }
	    }
	  }
	else
	  ++t;		/* garbage on this line, skip it */

    }

  fclose(fil);
  return 0;
}
