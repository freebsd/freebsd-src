#! /bin/sh

# Test that a getdate executable meets its specification.
#
# Copyright (C) 2004 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

# Why are these dates tested?
#
# February 29, 2003
#   Is not a leap year - should be invalid.
#
# 2004-12-40
#   Make sure get_date does not "roll" date forward to January 9th.  Some
#   versions have been known to do this.
#
# Dec-5-1972
#   This is my birthday.  :)
#
# 3/29/1974
# 1996/05/12 13:57:45
#   Because.
#
# 12-05-12
#   This will be my 40th birthday.  Ouch.  :)
#
# 05/12/96
#   Because.
#
# third tuesday in March, 2078
#   Wanted this to work.
#
# 1969-12-32 2:00:00 UTC
# 1970-01-01 2:00:00 UTC
# 1969-12-32 2:00:00 +0400
# 1970-01-01 2:00:00 +0400
# 1969-12-32 2:00:00 -0400
# 1970-01-01 2:00:00 -0400
#   Playing near the UNIX Epoch boundry condition to make sure date rolling
#   is also disabled there.
#
# 1996-12-12 1 month
#   Test a relative date.
#
# Tue Jan 19 03:14:07 2038 +0000
#   For machines with 31-bit time_t, any date past this date will be an
#   invalid date. So, any test date with a value greater than this
#   time is not portable.
#
# Feb. 29, 2096 4 years
#   4 years from this date is _not_ a leap year, so Feb. 29th does not exist.
#
# Feb. 29, 2096 8 years
#   8 years from this date is a leap year, so Feb. 29th does exist,
#   but on many hosts with 32-bit time_t types time, this test will
#   fail. So, this is not a portable test.
#

TZ=UTC0; export TZ

cat >getdate-expected <<EOF
Enter date, or blank line to exit.
	> Bad format - couldn't convert.
	> Bad format - couldn't convert.
	> Bad format - couldn't convert.
	> Fri Mar 29 00:00:00 1974
	> Sun May 12 13:57:45 1996
	> Sat May 12 00:00:00 2012
	> Sun May 12 00:00:00 1996
	> Bad format - couldn't convert.
	> Bad format - couldn't convert.
	> Thu Jan  1 02:00:00 1970
	> Bad format - couldn't convert.
	> Bad format - couldn't convert.
	> Bad format - couldn't convert.
	> Thu Jan  1 06:00:00 1970
	> Sun Jan 12 00:00:00 1997
	> 
EOF

./getdate >getdate-got <<EOF
February 29, 2003
2004-12-40
Dec-5-1972
3/29/1974
1996/05/12 13:57:45
12-05-12
05/12/96
third tuesday in March, 2078
1969-12-32 2:00:00 UTC
1970-01-01 2:00:00 UTC
1969-12-32 2:00:00 +0400
1970-01-01 2:00:00 +0400
1969-12-32 2:00:00 -0400
1970-01-01 2:00:00 -0400
1996-12-12 1 month
EOF

echo >>getdate-got

if cmp getdate-expected getdate-got >getdate.cmp; then :; else
	LOGFILE=`pwd`/getdate.log
	cat getdate.cmp >${LOGFILE}
	echo "** expected: " >>${LOGFILE}
	cat getdate-expected >>${LOGFILE}
	echo "** got: " >>${LOGFILE}
	cat getdate-got >>${LOGFILE}
	echo "FAIL: getdate" | tee -a ${LOGFILE}
	echo "Failed!  See ${LOGFILE} for more!" >&2
	exit 1
fi

rm getdate-expected getdate-got getdate.cmp
exit 0
