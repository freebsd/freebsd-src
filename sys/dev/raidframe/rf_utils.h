/*	$FreeBSD$ */
/*	$NetBSD: rf_utils.h,v 1.4 1999/08/13 03:26:55 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/***************************************
 *
 * rf_utils.c -- header file for utils.c
 *
 ***************************************/


#ifndef _RF__RF_UTILS_H_
#define _RF__RF_UTILS_H_

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_threadstuff.h>

char   *rf_find_non_white(char *p);
char   *rf_find_white(char *p);
RF_RowCol_t **rf_make_2d_array(int b, int k, RF_AllocListElem_t * allocList);
RF_RowCol_t *rf_make_1d_array(int c, RF_AllocListElem_t * allocList);
void    rf_free_2d_array(RF_RowCol_t ** a, int b, int k);
void    rf_free_1d_array(RF_RowCol_t * a, int n);
int     rf_gcd(int m, int n);
int     rf_atoi(char *p);
int     rf_htoi(char *p);

#define RF_USEC_PER_SEC 1000000
#define RF_TIMEVAL_TO_US(_t_) (((_t_).tv_sec) \
                * RF_USEC_PER_SEC + (_t_).tv_usec)

#define RF_TIMEVAL_DIFF(_start_,_end_,_diff_) { \
	if ((_end_)->tv_usec < (_start_)->tv_usec) { \
		(_diff_)->tv_usec = ((_end_)->tv_usec + RF_USEC_PER_SEC) \
				- (_start_)->tv_usec; \
		(_diff_)->tv_sec = ((_end_)->tv_sec-1) - (_start_)->tv_sec; \
	} \
	else { \
		(_diff_)->tv_usec = (_end_)->tv_usec - (_start_)->tv_usec; \
		(_diff_)->tv_sec  = (_end_)->tv_sec  - (_start_)->tv_sec; \
	} \
}

#endif				/* !_RF__RF_UTILS_H_ */
