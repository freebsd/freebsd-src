/*
 * Copyright (c) 1999, 2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: sfsasl.h,v 8.17 2000/09/19 21:30:49 ca Exp $"
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
