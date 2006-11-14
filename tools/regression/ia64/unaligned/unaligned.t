#!/usr/bin/env perl -w
#
# Copyright (c) 2005 Marcel Moolenaar
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$

my $srcdir = `dirname $0`;
chomp $srcdir;

my @accesses = ("Load", "Store");
my @types = ("Integer", "FloatingPoint");
my @sizes = ("Small", "Medium", "Large");
my @postincs = ("NoPostInc", "MinConstPostInc", "PlusConstPostInc",
		"ScratchRegPostInc", "PreservedRegPostInc");

sub run ($$$$$) {
    local ($nr, $access, $type, $size, $postinc) = @_;
    local $test = "${access}_${type}_${size}_${postinc}";
    local $tmpfile = "/tmp/" . $$ . "_$test";
    local $st;

    $st = system("cc -o $tmpfile -DACCESS=$access -DTYPE=$type -DSIZE=$size -DPOSTINC=$postinc -Wall -O -g $srcdir/test.c");
    if ($st != 0) {
	print "not ok $nr $test # compiling $test\n";
    }
    else {
	$st = system($tmpfile);
	if ($st == 0) {
	    print "ok $nr $test\n";
	}
	elsif ($st == 256) {
	    print "not ok $nr $test # invalid combination\n";
	}
	elsif ($st == 512) {
	    print "not ok $nr $test # value mismatch\n";
	}
	elsif ($st == 1024) {
	    print "not ok $nr $test # post increment mismatch\n";
	}
	else {
	    print "not ok $nr $test # signalled (exit status $st)\n";
	    return; # Preserve the executable
	}
    }
    unlink $tmpfile;
}

system("sysctl debug.unaligned_test=1");
if (`sysctl -n debug.unaligned_test` != "1") {
    print "1..0 # SKIP The debug.unaligned_test sysctl could not be set\n";
    exit 0;
}

my $count = @accesses * @types * @sizes * @postincs;

# There's no register based post inc. for stores.
$count -= 12;

print "1..$count\n";

my $nr=0;
foreach $access (@accesses) {
    foreach $postinc (@postincs) {
	$_ = "$access $postinc";
	if (! /Store.+RegPostInc/) {
	    foreach $type (@types) {
		foreach $size (@sizes) {
		    run ++$nr, $access, $type, $size, $postinc;
		}
	    }
	}
    }
}

system("sysctl debug.unaligned_test=0");

exit 0;
