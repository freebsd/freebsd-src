/*
 * Copyright (c) 1999, 2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: sfsasl.h,v 8.13.4.4 2000/07/18 18:44:51 gshapiro Exp $"
 */

#ifndef SFSASL_H
# define SFSASL_H

# if SFIO
#  include <sfio.h>
# endif /* SFIO */

# if SASL
#  if SFIO

/* sf discipline to add sasl */
typedef struct _sasldisc
{
	Sfdisc_t	disc;
	sasl_conn_t	*conn;
} Sasldisc_t;

extern int	sfdcsasl __P((Sfio_t *, Sfio_t *, sasl_conn_t *));

#  endif /* SFIO */
# endif /* SASL */

# if STARTTLS
#  if SFIO

/* sf discipline to add tls */
typedef struct _tlsdisc
{
	Sfdisc_t	disc;
	SSL		*con;
} Tlsdisc_t;

extern int	sfdctls __P((Sfio_t *, Sfio_t *, SSL *));

#  else /* SFIO */
#   if _FFR_TLS_TOREK

typedef struct tls_conn
{
	FILE		*fp;	/* original FILE * */
	SSL		*con;	/* SSL context */
} Tlsdisc_t;

extern int	sfdctls __P((FILE **, FILE **, SSL *));

#   endif /* _FFR_TLS_TOREK */
#  endif /* SFIO */
# endif /* STARTTLS */
#endif /* ! SFSASL_H */
