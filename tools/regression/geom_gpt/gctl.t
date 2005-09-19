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

my $cmd = "/tmp/$$";
my $out = "$cmd.out";

my %tests = (
    "" => "FAIL Verb missing",
    "-s verb=invalid" => "FAIL 22 verb 'invalid'",
    # create
    "-s verb=create" => "FAIL 87 provider",
    "-s verb=create -s provider=invalid" => "FAIL 22 provider 'invalid'",
);

sub run ($$) {
    local ($nr, $test) = @_;
    local $st;

    if (exists $ENV{'TEST_VERBOSE'}) {
	system("$cmd -v $test > $out 2>&1");
	system("cat $out");
    }
    else {
	system("$cmd $test > $out 2>&1");
    }
    $st = `tail -1 $out`;
    if ($st =~ "^$tests{$test}") {
	print "ok $nr\n";
    } else {
	print "not ok $nr # $st\n";
    }
    unlink $out;
}

# Compile the driver...
my $st = system("cc -o $cmd -g $srcdir/test.c -lgeom");
if ($st != 0) {
    print "1..0 # SKIP error compiling test.c\n";
    exit 0;
}

# Make sure we have permission to use gctl...
if (`$cmd` =~ "^FAIL Permission denied") {
    print "1..0 # SKIP not enough permission\n";
    unlink $cmd;
    exit 0;
}

$count = keys (%tests);
print "1..$count\n";

my $nr=0;
foreach $test (keys %tests) {
    run ++$nr, $test;
}

unlink $cmd;
exit 0;
