/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

/**********************************************************************
 *
 * MODULE: dat_osd.c
 *
 * PURPOSE: Operating System Dependent layer
 * Description:
 *	Provide OS dependent functions with a canonical DAPL
 *	interface. Designed to be portable and hide OS specific quirks
 *	of common functions.
 *
 * $Id: dat_osd.c,v 1.14 2005/04/25 17:29:41 jlentini Exp $
 **********************************************************************/

#include "dat_osd.h"


/*********************************************************************
 *                                                                   *
 * Constants                                                         *
 *                                                                   *
 *********************************************************************/

#define DAT_DBG_TYPE_ENV 	"DAT_DBG_TYPE"
#define DAT_DBG_DEST_ENV 	"DAT_DBG_DEST"


/*********************************************************************
 *                                                                   *
 * Enumerations                                                      *
 *                                                                   *
 *********************************************************************/

typedef int 			DAT_OS_DBG_DEST;

typedef enum
{
    DAT_OS_DBG_DEST_STDOUT  		= 0x1,
    DAT_OS_DBG_DEST_SYSLOG  		= 0x2,
    DAT_OS_DBG_DEST_ALL  		= 0x3
} DAT_OS_DBG_DEST_TYPE;


/*********************************************************************
 *                                                                   *
 * Global Variables                                                  *
 *                                                                   *
 *********************************************************************/

static DAT_OS_DBG_TYPE_VAL 	g_dbg_type = DAT_OS_DBG_TYPE_ERROR;
static DAT_OS_DBG_DEST 		g_dbg_dest = DAT_OS_DBG_DEST_STDOUT;


/***********************************************************************
 * Function: dat_os_dbg_init
 ***********************************************************************/

void
dat_os_dbg_init ( void )
{
    char *dbg_type;
    char *dbg_dest;

    if ( NULL != (dbg_type = dat_os_getenv (DAT_DBG_TYPE_ENV)) )
    {
	g_dbg_type = dat_os_strtol (dbg_type, NULL, 0);
    }

    if ( NULL != (dbg_dest = dat_os_getenv (DAT_DBG_DEST_ENV)) )
    {
	g_dbg_dest = dat_os_strtol (dbg_dest, NULL, 0);
    }
}


/***********************************************************************
 * Function: dat_os_dbg_print
 ***********************************************************************/

void
dat_os_dbg_print (
    DAT_OS_DBG_TYPE_VAL		type,
    const char *		fmt,
    ...)
{
    if (type & g_dbg_type)
    {
	va_list args;

	if ( DAT_OS_DBG_DEST_STDOUT & g_dbg_dest )
	{
	    va_start (args, fmt);
	    vfprintf (stdout, fmt, args);
	    fflush (stdout);
	    va_end (args);
	}

	if ( DAT_OS_DBG_DEST_SYSLOG & g_dbg_dest )
	{
	    va_start (args, fmt);
	    vsyslog (LOG_USER | LOG_DEBUG, fmt, args);
	    va_end (args);
	}
    }
}


/***********************************************************************
 * Function: dat_os_library_load
 ***********************************************************************/

DAT_RETURN
dat_os_library_load (
    const char 			*library_path,
    DAT_OS_LIBRARY_HANDLE 	*library_handle_ptr)
{
    DAT_OS_LIBRARY_HANDLE       library_handle;

    if ( NULL != (library_handle = dlopen (library_path, RTLD_NOW | RTLD_GLOBAL)) )
    {
	if ( NULL != library_handle_ptr )
	{
	    *library_handle_ptr = library_handle;
	}

	return DAT_SUCCESS;
    }
    else
    {
	dat_os_dbg_print (DAT_OS_DBG_TYPE_ERROR,
			 "DAT: library load failure: %s\n",
			 dlerror ());
	return DAT_INTERNAL_ERROR;
    }
}


/***********************************************************************
 * Function: dat_os_library_unload
 ***********************************************************************/

DAT_RETURN
dat_os_library_unload (
    const DAT_OS_LIBRARY_HANDLE library_handle)
{
    if ( 0 != dlclose (library_handle) )
    {
	return DAT_INTERNAL_ERROR;
    }
    else
    {
	return DAT_SUCCESS;
    }
}
