/*
 * 
 * xmalloc.h
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Mon Mar 20 22:09:17 1995 ylo
 * 
 * Versions of malloc and friends that check their results, and never return
 * failure (they call fatal if they encounter an error).
 * 
 */

/* RCSID("$Id: xmalloc.h,v 1.2 1999/11/24 00:26:04 deraadt Exp $"); */

#ifndef XMALLOC_H
#define XMALLOC_H

/* Like malloc, but calls fatal() if out of memory. */
void   *xmalloc(size_t size);

/* Like realloc, but calls fatal() if out of memory. */
void   *xrealloc(void *ptr, size_t new_size);

/* Frees memory allocated using xmalloc or xrealloc. */
void    xfree(void *ptr);

/* Allocates memory using xmalloc, and copies the string into that memory. */
char   *xstrdup(const char *str);

#endif				/* XMALLOC_H */
