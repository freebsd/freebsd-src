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

my $tmpfile = "/tmp/unaligned." . $$;

my @types = ("short", "int", "long", "float", "double", "long double");
my %values = (	"short" => "0x1234",
		"int" => "0x12345678",
		"long" => "0x123456789abcdef0",
		"float" => "1.04716",
		"double" => "3.1415",
		"long double" => "0.33312112048384"
	     );
my @tests = ("TEST_LOAD", "TEST_STORE");

sub run ($$$) {
	local ($nr, $type, $test) = @_;
	local $value = $values{$type};
	local $st;
	$st = system("cc -o $tmpfile -DDATA_TYPE='$type' -DDATA_VALUE=$value -D$test -Wall $srcdir/test.c"); 
	if ($st != 0) {
		print "not ok $nr ($type,$test) # compiling $tmpfile\n";
		return;
	}
	$st = system($tmpfile);
	if ($st == 0) {
		print "ok $nr ($type,$test)\n";
	}
	elsif ($st == 1) {
		print "not ok $nr ($type,$test) # value mismatch\n";
	}
	else {
		print "not ok $nr ($type,$test) # signalled\n";
	}
	unlink $tmpfile;
}

system("sysctl debug.unaligned_test=1");
if (`sysctl -n debug.unaligned_test` != "1") {
    print "1..0 # SKIP The debug.unaligned_test sysctl could not be set\n";
    exit 0;
}

my $count = @types * @tests;
print "1..$count\n";

my $nr=0;
foreach $type (@types) {
	foreach $test (@tests) {
		run ++$nr, $type, $test;
	}
}

system("sysctl debug.unaligned_test=0");

exit 0;
