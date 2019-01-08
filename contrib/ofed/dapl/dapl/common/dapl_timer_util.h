/*
 * Copyright (c) 2002-2003, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
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
 * HEADER: dapl_timer_util.h
 *
 * PURPOSE: DAPL timer management
 * Description: support for dapl_timer.h
 *
 * $Id:$
 **********************************************************************/

void dapls_timer_init ( void );
void dapls_timer_release( void );

DAT_RETURN dapls_timer_set (
	IN  DAPL_OS_TIMER		*timer,
	IN  void			(*func)	(uintptr_t),
	IN  void			*data,
	IN  DAPL_OS_TIMEVAL	 	expires );

void dapls_timer_cancel (
	IN  DAPL_OS_TIMER		*timer);
