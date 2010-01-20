#!/usr/bin/perl -w
#
# Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice(s), this list of conditions and the following disclaimer as
#    the first lines of this file unmodified other than the possible
#    addition of one or more copyright notices.
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
###########################################################################
#
# Verify that no cancellation points are propagated inside of libc_r.
#
# $FreeBSD$
#

@CPOINTS = ("aio_suspend", "close", "creat", "fcntl", "fsync", "mq_receive",
	    "mq_send", "msync", "nanosleep", "open", "pause",
	    "pthread_cond_timedwait", "pthread_cond_wait", "pthread_join",
	    "pthread_testcancel", "read", "sem_wait", "sigsuspend",
	    "sigtimedwait", "sigwait", "sigwaitinfo", "sleep", "system",
	    "tcdrain", "wait", "waitpid", "write");

print "1..1\n";

$cpoints = join '\|', @CPOINTS;
$regexp = "\" U \\(" . $cpoints . "\\\)\$\"";

`nm -a /usr/lib/libc.a |grep $regexp >propagate_s.out`;
if (!open (NMOUT, "<./propagate_s.out"))
{
    print "not ok 1\n";
}
else
{
    $propagations = 0; 

    while (<NMOUT>)
    {
	$propagations++;
	print "$_\n";
    }
    if ($propagations != 0)
    {
	print "$propagations propagation(s)\n";
	print "not ok 1\n";
    }
    else
    {
	print "ok 1\n";
    }
    close NMOUT;
    unlink "propagate_s.out";
}
