/*
 * Copyright (c) 1999, 2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: sfsasl.h,v 1.1.1.2 2002/02/17 21:56:41 gshapiro Exp $"
 */

#ifndef SFSASL_H
# define SFSASL_H

#if SASL
extern int	sfdcsasl __P((SM_FILE_T **, SM_FILE_T **, sasl_conn_t *));
#endif /* SASL */

# if STARTTLS
extern int	sfdctls __P((SM_FILE_T **, SM_FILE_T **, SSL *));
# endif /* STARTTLS */

#endif /* ! SFSASL_H */
