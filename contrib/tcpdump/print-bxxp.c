/*
 * Copyright (C) 2000, Richard Sharpe
 *
 * This software may be distributed either under the terms of the 
 * BSD-style licence that accompanies tcpdump or under the GNU GPL 
 * version 2 or later.
 *
 * print-bxxp.c
 *
 */

#ifndef lint
static const char rcsid[] =
  "@(#) $Header: /tcpdump/master/tcpdump/print-bxxp.c,v 1.3 2000/10/05 04:10:01 itojun Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "interface.h"
#include "extract.h"

/* Check for a string but not go beyond length
 * Return TRUE on match, FALSE otherwise
 * 
 * Looks at the first few chars up to tl1 ...
 */

int l_strnstart(const u_char *, u_int, const u_char *, u_int);

int
l_strnstart(register const u_char *tstr1, register u_int tl1,
	  register const u_char *str2, register u_int l2)
{

  if (tl1 > l2)
    return 0;
  
  return (strncmp(tstr1, str2, tl1) == 0 ? 1 : 0);

}

void
bxxp_print(register const u_char *bp, register u_int length)
{

  if (l_strnstart("REQ ", 4, bp, length)) { /* A REQuest */

    printf(" BXXP REQ");

  } 
  else if (l_strnstart("RSP ", 4, bp, length)) {

    printf(" BXXP RSP");

  }
  else if (l_strnstart("SEQ ", 4, bp, length)) {

    printf(" BXXP SEQ");

  }
  else if (l_strnstart("END", 4, bp, length)) {

    printf(" BXXP END");

  }
  else 
    printf(" BXXP (payload or undecoded)");

}
