#!/usr/bin/perl
#
# Copyright (c) 1995 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $Id: which.pl,v 1.12 1998/01/02 13:46:25 helbig Exp $

$all = $silent = $found = 0;
@path = split(/:/, $ENV{'PATH'});
if ($ENV{'PATH'} =~ /:$/) {
	$#path = $#path + 1;
	$path[$#path] = "";
}

if ($ARGV[0] eq "-a") {
    $all = 1; shift @ARGV;
} elsif ($ARGV[0] eq "-s") {
    $silent = 1; shift @ARGV;
} elsif ($ARGV[0] =~ /^-(h|help|\?)$/) {
    die "usage: which [-a] [-s] program ...\n";
}

foreach $prog (@ARGV) {
    if ("$prog" =~ '/' && -x "$prog" && -f "$prog") {
	print "$prog\n" unless $silent;
	$found = 1;
    } else {
	foreach $e (@path) {
	    $e = "." if !$e;
	    if (-x "$e/$prog" && -f "$e/$prog") {
		print "$e/$prog\n" unless $silent;
		$found = 1;
		last unless $all;
	    }
	}
    }
}

exit (!$found);
