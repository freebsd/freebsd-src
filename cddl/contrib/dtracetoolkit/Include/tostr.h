/*
 * tostr.h - DTrace To-String include file.
 *
 * $Id: tostr.h 36 2007-09-15 06:51:18Z brendan $
 *
 * COPYRIGHT: Copyright (c) 2007 Brendan Gregg.
 *
 * CDDL HEADER START
 *
 *  The contents of this file are subject to the terms of the
 *  Common Development and Distribution License, Version 1.0 only
 *  (the "License").  You may not use this file except in compliance
 *  with the License.
 *
 *  You can obtain a copy of the license at Docs/cddl1.txt
 *  or http://www.opensolaris.org/os/licensing.
 *  See the License for the specific language governing permissions
 *  and limitations under the License.
 *
 * CDDL HEADER END
 *
 * 16-Sep-2007	Brendan Gregg	Created this.
 */

/*
 * NUM_TO_STR(n) - takes a number and returns a string with a prefix,
 *	intended to fit withen 6 chars.
 *
 *	Input		Output
 *	0		0
 *	1		1
 *	10		10
 *	999		999
 *	1000		1.0K
 *	1100		1.1K
 *	10000		10.0K
 *	999999		999.0K
 * 	1000000		1.0M
 * 	10000000	10.0M
 * 	999999999	999.9M
 */
#define	NUM_TO_STR(n)							\
	n >= 1000000 ?							\
	strjoin(strjoin(strjoin(lltostr(n / 1000000), "."), 		\
	lltostr((n % 1000000) / 100000)), "M") : n >= 1000 ?		\
	strjoin(strjoin(strjoin(lltostr(n / 1000), "."), 		\
	lltostr((n % 1000) / 100)), "K") : lltostr(n)

/*
 * BYTES_TO_STR(n) - takes a byte count and returns a string with a prefix,
 *	intended to fit withen 6 chars.
 *
 *	Input		Output
 *	0		0
 *	1		1
 *	10		10
 *	999		0.9K
 *	1000		0.9K
 *	1024		1.0K
 *	10240		10.0K
 *	1000000		976.5K
 * 	1048576		1.0M
 * 	1000000000	953.6M
 */
#define	BYTES_TO_STR(n)							\
	n >= 1024000 ?							\
	strjoin(strjoin(strjoin(lltostr(n / 1048576), "."), 		\
	lltostr((n % 1048576) / 104858)), "M") : n >= 1000 ?		\
	strjoin(strjoin(strjoin(lltostr(n / 1024), "."), 		\
	lltostr((n % 1024) / 103)), "K") : lltostr(n)

/*
 * US_TO_STR(n) - takes microseconds and returns a string with a prefix,
 *	intended to fit withen 6 chars.
 *
 *	Input		Output
 *	0		0
 *	1		1u
 *	10		10u
 *	999		999u
 *	1000		1.0m
 *	1100		1.1m
 *	10000		10.0m
 *	999999		999.0m
 */
#define	US_TO_STR(n)							\
	n == 0 ? "0" : n >= 1000 ?					\
	strjoin(lltostr(n / 1000), "m") : strjoin(lltostr(n), "u")

