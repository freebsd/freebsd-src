/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char rcsid[] =
"@(#) $Header: /tcpdump/master/tcpdump/print-lcp.c,v 1.9 2000/10/06 04:23:12 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */
#include "ppp.h"

/* Codes */
enum { 
  LCP_CONFREQ = 1,
  LCP_CONFACK = 2,
  LCP_CONFNAK = 3,
  LCP_CONFREJ = 4,
  LCP_TERMREQ = 5,
  LCP_TERMACK = 6,
  LCP_CODEREJ = 7,
  LCP_PROTREJ = 8,
  LCP_ECHOREQ = 9,
  LCP_ECHOREP = 10,
  LCP_DISCARD = 11
};

static struct tok lcpcode2str[] = {
  { LCP_CONFREQ, "ConfReq" },
  { LCP_CONFACK, "ConfAck" },
  { LCP_CONFNAK, "ConfNak" },
  { LCP_CONFREJ, "ConfRej" },
  { LCP_TERMREQ, "TermReq" },
  { LCP_TERMACK, "TermAck" },
  { LCP_CODEREJ, "CodeRej" },
  { LCP_PROTREJ, "ProtRej" },
  { LCP_ECHOREQ, "EchoReq" },
  { LCP_ECHOREP, "EchoRep" },
  { LCP_DISCARD, "Discard" },
  { 0, NULL }
};


enum {
  LCP_RESERVED  = 0,
  LCP_MRU 	= 1,
  LCP_ASYNCMAP  = 2,
  LCP_AUTHPROTO = 3,
  LCP_QUALPROTO = 4,
  LCP_MAGICNUM  = 5,
  LCP_PCOMP 	= 7,
  LCP_ACFCOMP 	= 8,
  LCP_CALLBACK  = 13
};

static struct tok lcpoption2str[] = {
  { LCP_RESERVED, "reserved"},
  { LCP_MRU, "mru"},
  { LCP_ASYNCMAP, "asyncmap"},
  { LCP_AUTHPROTO, "auth"},
  { LCP_QUALPROTO, "qual"},
  { LCP_MAGICNUM, "magic"},
  { LCP_PCOMP, "pcomp"},
  { LCP_ACFCOMP, "acfcomp"},
  { LCP_CALLBACK, "callback"},
  { 0, NULL }
};

static struct tok lcpauth2str[] = {
  {0xc023, "PAP"},
  {0xc223, "CHAP"},
  { 0, NULL }
};

static struct tok lcpqual2str[] = {
  {0xc025, "LQR"},
  { 0, NULL }
};

static struct tok lcpchap2str[] = {
  {0x05, "MD5"},
  {0x80, "MS"},
  { 0, NULL }
};

void
lcp_print(register const u_char *bp, u_int length)
{
  u_short lcp_code, lcp_id, lcp_length;
  const u_char *lcp_data;

  lcp_data = bp+4;
	
  if (snapend < lcp_data) {
    printf(" [LCP|]");
    return;
  }
  
  lcp_code  = bp[0];
  lcp_id    = bp[1];
  lcp_length = EXTRACT_16BITS(bp+2);

  printf("LCP %s id=0x%x", tok2str(lcpcode2str, "LCP-#%d", lcp_code), lcp_id);
  
  switch (lcp_code) {
  case LCP_CONFREQ:
  case LCP_CONFACK:
  case LCP_CONFNAK:
  case LCP_CONFREJ:
    /* Print Options */
    {
      u_char lcpopt_type, lcpopt_length;
      const u_char *p=lcp_data;
      while (p+2 < lcp_data+lcp_length && p+2 < snapend) {
	lcpopt_type = p[0];
	lcpopt_length = p[1];
	p+=2;
	printf(" <%s ",tok2str(lcpoption2str, "option-#%d", lcpopt_type));
	if (lcpopt_length)
	  switch (lcpopt_type) {
	  case LCP_MRU:
	    if (snapend < p+2) return;
	    printf("%d",ntohs(*(u_short*)p));
	    if (lcpopt_length != 4) printf(" len=%d!",lcpopt_length);
	    break;
	  case LCP_AUTHPROTO:
	    if (snapend < p+2) return;
	    printf("%s",tok2str(lcpauth2str, "AUTH-%#x", ntohs(*(u_short*)p)));
	    if (lcpopt_length < 4) printf(" len=%d!",lcpopt_length);
	    if (lcpopt_length >= 5 && p < snapend) 
	      printf(" %s",tok2str(lcpchap2str, "%#x", p[0]));
	    break;
	  case LCP_QUALPROTO:
	    if (snapend < p+2) return;
	    printf("%s",tok2str(lcpqual2str, "QUAL-%#x", ntohs(*(u_short*)p)));
	    if (lcpopt_length < 4) printf(" len=%d!",lcpopt_length);
	    /* Print data field of auth? */
	    break;
	  case LCP_ASYNCMAP:
	  case LCP_MAGICNUM:
	    if (snapend < p+4) return;
	    printf("%#x", (unsigned)ntohl(*(u_long*)p));
	    if (lcpopt_length != 6) printf(" len=%d!",lcpopt_length);
	    break;
	  case LCP_PCOMP:
	  case LCP_ACFCOMP:
	  case LCP_RESERVED:
	    if (lcpopt_length != 2) printf(" len=%d!",lcpopt_length);
	    break;
	  default:
	    if (lcpopt_length != 2) printf(" len=%d",lcpopt_length);
	    break;
	  }
	printf(">");
	p+=lcpopt_length-2;
      }
    }
    break;
  case LCP_ECHOREQ:
  case LCP_ECHOREP:
  case LCP_DISCARD:
    if (snapend < lcp_data+4) return;
    printf(" magic=%#x", (unsigned)ntohl(*(u_long *) lcp_data));
    lcp_data +=4;
    break;
  case LCP_PROTREJ:
    if (snapend < lcp_data+2) return;
    printf(" prot=%s", tok2str(ppptype2str, "PROT-%#x", ntohs(*(u_short *) lcp_data)));
    /* TODO print rejected packet too ? */
    break;
  case LCP_CODEREJ:
    if (snapend < lcp_data+4) return;
    printf(" ");
    lcp_print(lcp_data, (lcp_length+lcp_data > snapend ? snapend-lcp_data : lcp_length));
    break;
  case LCP_TERMREQ:
  case LCP_TERMACK:
    break;
  default:
    break;
  }
  
  return;
}
