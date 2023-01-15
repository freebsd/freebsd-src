/*
 * Copyright (c) 2000 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: sm_os_openbsd.h,v 1.8 2013-11-22 20:51:34 ca Exp $
 */

/*
**  sm_os_openbsd.h -- platform definitions for OpenBSD
**
**  Note: this header file cannot be called OpenBSD.h
**  because <sys/param.h> defines the macro OpenBSD.
*/

#define SM_OS_NAME	"openbsd"

/* 
**  Temporary HACK for newer icu4c versions which include stdbool.h:
**  pretend that it is already included
**  otherwise compilation will break because bool is then
**  redefined between the prototype declaration and
**  the function definition, e.g.,
**	lowercase.c: error: conflicting types for 'asciistr'
**	../../include/sm/ixlen.h:29:13: note: previous declaration is here
*/

#if USE_EAI && !SM_CONF_STDBOOL_H
# define _STDBOOL_H_	1
#endif

#define SM_CONF_SYS_CDEFS_H	1
#ifndef SM_CONF_SHM
# define SM_CONF_SHM	1
#endif
#ifndef SM_CONF_SEM
# define SM_CONF_SEM	1
#endif
#ifndef SM_CONF_MSG
# define SM_CONF_MSG	1
#endif
