#!/usr/bin/perl
#
# Copyright (C) 1995, Wolfram Schneider <wosch@cs.tu-berlin.de>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#      This product includes software developed by the University of
#      California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE CONTRIBUTOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# [whew!]
#
# $Id: which.pl,v 1.1 1995/01/26 21:49:54 jkh Exp $

$all = 0;
$silent = 0;
$found = 0;
@path = split(/:/, $ENV{'PATH'});

if ($ARGV[0] eq "-a") {
    $all = 1; shift @ARGV;
} elsif ($ARGV[0] eq "-s") {
    $silent = 1; shift @ARGV;
} elsif ($ARGV[0] =~ /^-(h|help|\?)$/) {
    die "usage:\n\twhich [-a] [-s] program ...\n";
}

foreach $prog (@ARGV) {
    foreach $e (@path) {
        if (-x "$e/$prog") {
	    if (! $silent) {
		print "$e/$prog\n";
	    }
	    $found = 1;
	    last unless $all;
        }
    }
}

if ($found) {
    exit 0;
} else {
    exit 1;
}
