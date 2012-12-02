#!/usr/bin/ksh
/*
 * test.ksh - DTrace include file test script.
 *
 * $Id: test.ksh 36 2007-09-15 06:51:18Z brendan $
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

dtrace -CI . -s /dev/stdin << END

#include "tostr.h"
#include "time.h"

#pragma D option quiet
#pragma D option destructive

dtrace:::BEGIN
{
	i = 1;
	printf("\nNUM_TO_STR   %12d = %s\n", i, NUM_TO_STR(i));
	i = 1100;
	printf("NUM_TO_STR   %12d = %s\n", i, NUM_TO_STR(i));
	i = 1100000;
	printf("NUM_TO_STR   %12d = %s\n", i, NUM_TO_STR(i));
	i = 999999999;
	printf("NUM_TO_STR   %12d = %s\n", i, NUM_TO_STR(i));

	i = 1;
	printf("\nBYTES_TO_STR %12d = %s\n", i, BYTES_TO_STR(i));
	i = 1024;
	printf("BYTES_TO_STR %12d = %s\n", i, BYTES_TO_STR(i));
	i = 1000000;
	printf("BYTES_TO_STR %12d = %s\n", i, BYTES_TO_STR(i));
	i = 999999999;
	printf("BYTES_TO_STR %12d = %s\n", i, BYTES_TO_STR(i));

	i = 1;
	printf("\nUS_TO_STR    %12d = %s\n", i, US_TO_STR(i));
	i = 1100;
	printf("US_TO_STR    %12d = %s\n", i, US_TO_STR(i));
	i = 999999;
	printf("US_TO_STR    %12d = %s\n", i, US_TO_STR(i));

	printf("\nwalltimestamp : %Y\n", walltimestamp);
	printf("TZ=GMT date   : ");
	system("TZ=GMT date '+%%H:%%M:%%S'");
	printf("TIME_HHMMSS   : %s\n", TIME_HHMMSS);

	exit(0);
}
END
