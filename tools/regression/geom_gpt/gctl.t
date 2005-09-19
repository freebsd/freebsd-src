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

my $cmd = "/tmp/gctl-$$";
my $out = "$cmd.out";
my $disk = "/tmp/disk-$$";
my $unit = "";

my %steps = (
    "1" => "gctl",
    "2" => "gctl",
    "3" => "gctl",
    "4" => "gctl",
    "5" => "mdcfg",
    "6" => "gctl",
    "7" => "gctl",
    "8" => "gctl",
    "9" => "mdcfg",
);

my %args = (
    "1" => "",
    "2" => "verb=invalid",
    "3" => "verb=create",
    "4" => "verb=create provider=invalid",
    "5" => "create",
    "6" => "verb=create provider=md%unit% entries=-1",
    "7" => "verb=create provider=md%unit%",
    "8" => "verb=create provider=md%unit%",
    "9" => "destroy",
);

my %result = (
    "1" => "FAIL Verb missing",
    "2" => "FAIL 22 verb 'invalid'",
    "3" => "FAIL 87 provider",
    "4" => "FAIL 22 provider 'invalid'",
	#
    "6" => "FAIL 22 entries -1",
    "7" => "PASS",
    "8" => "FAIL 17 geom 'md0'",
	#
);

my $verbose = "";
if (exists $ENV{'TEST_VERBOSE'}) {
    $verbose = "-v";
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

$count = keys (%steps);
print "1..$count\n";

foreach my $nr (sort keys %steps) {
    my $action = $steps{$nr};
    my $arg = $args{$nr};

    if ($action =~ "gctl") {
	$arg =~ s/%unit%/$unit/g;
	system("$cmd $verbose $arg | tee $out 2>&1");
	$st = `tail -1 $out`;
	if ($st =~ "^$result{$nr}") {
	    print "ok $nr\n";
	} else {
	    print "not ok $nr \# $st\n";
	}
	unlink $out;
    } elsif ($action =~ "mdcfg") {
	if ($arg =~ "create") {
	    system("dd if=/dev/zero of=$disk count=1024 2>&1");
	    $unit = `mdconfig -a -t vnode -f $disk`;
	    chomp $unit;
	    $unit =~ s/md//g;
	} elsif ($arg =~ "destroy") {
	    system("mdconfig -d -u $unit");
	    unlink $disk;
	    $unit = "";
	}
	print "ok $nr\n";
    }
}

unlink $cmd;
exit 0;
