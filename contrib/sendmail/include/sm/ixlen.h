/*
 * Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#ifndef _SM_IXLEN_H
# define _SM_IXLEN_H 1

#define SM_IS_MQ(ch) (((ch) & 0377) == METAQUOTE)

#if _FFR_8BITENVADDR
# define XLENDECL bool mq=false;	\
	int xlen = 0;
# define XLENRESET mq=false, xlen = 0
# define XLEN(c) do { \
		if (mq) { ++xlen; mq=false; } \
		else if (SM_IS_MQ(c)) mq=true; \
		else ++xlen; \
	} while (0)

extern int ilenx __P((const char *));
extern int xleni __P((const char *));

# if USE_EAI
extern bool	asciistr __P((const char *));
extern bool	asciinstr __P((const char *, size_t));
extern int	uxtext_unquote __P((const char *, char *, int));
extern char	*sm_lowercase  __P((const char *));
extern bool	utf8_valid  __P((const char *, size_t));
# endif

#else /* _FFR_8BITENVADDR */
# define XLENDECL int xlen = 0;
# define XLENRESET xlen = 0
# define XLEN(c) ++xlen
# define ilenx(str) strlen(str)
# define xleni(str) strlen(str)
#endif /* _FFR_8BITENVADDR */

#endif /* ! _SM_IXLEN_H */
