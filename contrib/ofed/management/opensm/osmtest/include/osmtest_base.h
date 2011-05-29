/*
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 * 	Declaration of osmtest_t.
 *	This object represents the OSMTest Test object.
 *
 */
#ifndef _OSMTEST_BASE_H_
#define _OSMTEST_BASE_H_

#ifndef __WIN__
#include <limits.h>
#else
#include <vendor/winosm_common.h>
#endif

#define OSMTEST_MAX_LINE_LEN	120
#ifdef WIN32
#define OSMTEST_FILE_PATH_MAX	4096
#else
#define OSMTEST_FILE_PATH_MAX	PATH_MAX
#endif

#define STRESS_SMALL_RMPP_THR 100000
/*
    Take long times when quering big clusters (over 40 nodes) , an average of : 0.25 sec for query
    each query receives 1000 records
*/
#define STRESS_LARGE_RMPP_THR 4000
#define STRESS_LARGE_PR_RMPP_THR 20000

extern const char *const p_file;

#endif				/* _OSMTEST_BASE_H_ */
