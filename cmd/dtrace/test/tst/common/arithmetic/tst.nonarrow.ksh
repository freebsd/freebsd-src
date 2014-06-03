#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright (c) 2012, Joyent, Inc. All rights reserved.
#

#
# Somewhat surprisingly, DTrace very much relies on the fact that when 32-bit
# is forced, pointers are not, in fact, narrowed to 32-bits.  In particular, if
# this is not so (that is, if pointers are narrowed to their seemingly correct
# 32-bit width), helpers attached to 32-bit programs will fail to operate:
# they will erroneously zero the high 32-bits of the return values of 64-bit
# kernel pointers as returned by copyin(), alloca(), etc.  This test asserts
# this implicit behavior -- and this comment regrettably serves as this
# behavior's only documentation.
#
doit()
{
	/usr/sbin/dtrace $1 -n BEGIN'{trace(sizeof (long))}' \
	    -n 'BEGIN{*(int *)alloca(4) = 21506; exit(0)}' \
	    -n 'ERROR{exit(1)}'

	if [ "$?" -ne 0 ]; then
		exit $?
	fi
}

doit
doit -32
