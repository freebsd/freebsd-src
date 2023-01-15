/*
 * Copyright (c) 2006, 2020 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

/*
**  SENDMAIL.H -- MTA-specific definitions for sendmail.
*/

#ifndef _SM_SENDMAIL_H
# define _SM_SENDMAIL_H 1

#include <sm/rpool.h>

/* "out of band" indicator */
#define METAQUOTE	((unsigned char)0377)	/* quotes the next octet */
#define SM_MM_QUOTE(ch) (((ch) & 0377) == METAQUOTE || (((ch) & 0340) == 0200))

extern int	dequote_internal_chars __P((char *, char *, int));
#if SM_HEAP_CHECK > 2
extern char	*quote_internal_chars_tagged __P((char *, char *, int *, SM_RPOOL_T *, char *, int, int));
# define quote_internal_chars(ibp, obp, bsp, rpool) quote_internal_chars_tagged(ibp, obp, bsp, rpool, "quote_internal_chars:" __FILE__, __LINE__, SmHeapGroup)
#else
extern char	*quote_internal_chars __P((char *, char *, int *, SM_RPOOL_T *));
# define quote_internal_chars_tagged(ibp, obp, bsp, rpool, file, line, group) quote_internal_chars(ibp, obp, bsp, rpool)
#endif
extern char	*str2prt __P((char *));

extern char	*makelower __P((char *));
#if USE_EAI
extern bool	sm_strcaseeq __P((const char *, const char *));
extern bool	sm_strncaseeq __P((const char *, const char *, size_t));
# define SM_STRCASEEQ(a, b)	sm_strcaseeq((a), (b))
# define SM_STRNCASEEQ(a, b, n)	sm_strncaseeq((a), (b), (n))
#else
# define SM_STRCASEEQ(a, b)	(sm_strcasecmp((a), (b)) == 0)
# define SM_STRNCASEEQ(a, b, n)	(sm_strncasecmp((a), (b), (n)) == 0)
#endif

#endif /* ! _SM_SENDMAIL_H */
