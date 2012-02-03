#!/usr/bin/perl -w
#
# Copyright (C) 2001 Jason Evans <jasone@freebsd.org>.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice(s), this list of conditions and the following disclaimer
#    unmodified other than the allowable addition of one or more
#    copyright notices.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice(s), this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#
# Test thread stack guard functionality.  The C test program needs to be driven
# by this script because it segfaults when the stack guard is hit.
#

print "1..30\n";

$i = 0;
# Iterates 10 times.
for ($stacksize = 65536; $stacksize < 131072; $stacksize += 7168)
{
    # Iterates 3 times (1024, 4096, 7168).
    for ($guardsize = 1024; $guardsize < 8192; $guardsize += 3072)
    {
	$i++;

	print "stacksize: $stacksize, guardsize: $guardsize\n";

	`./guard_b $stacksize $guardsize >guard_b.out 2>&1`;

	if (! -f "./guard_b.out")
	{
	    print "not ok $i\n";
	}
	else
	{
	    `diff guard_b.exp guard_b.out >guard_b.diff 2>&1`;
	    if ($?)
	    {
		# diff returns non-zero if there is a difference.
		print "not ok $i\n";
	    }
	    else
	    {
		print "ok $i\n";
	    }
	}
    }
}
