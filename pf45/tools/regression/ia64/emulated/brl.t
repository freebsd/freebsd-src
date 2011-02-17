#!/usr/bin/env perl -w
#
# Copyright (c) 2006 Marcel Moolenaar
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

my @types = ("Call", "Cond");
my @preds = ("False", "True");
my %variant_mapping = (
	"Call" => "",
	"Cond" => "Backward Forward"
);

sub run ($$$$) {
    local ($nr, $type, $pred, $var) = @_;
    local $test = "${type}_${pred}_${var}";
    local $tmpfile = "/tmp/" . $$ . "_$test";
    local $st;

    $st = system("cc -o $tmpfile -DTYPE=$type -DPRED=$pred -DVAR=$var -Wall -O1 -g $srcdir/test.c");
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
            print "not ok $nr $test # long branch failure\n";
        }
        else {
            print "not ok $nr $test # signalled (exit status $st)\n";
            return; # Preserve the executable
        }
    }
    unlink $tmpfile;
}

#
# We can only test the long brach emulation on the Merced processor.
# Check for that and skip these tests if it's not...
#
$_ = `sysctl -n hw.model`;
if (! /^Merced$/) {
    print "1..0 # SKIP This test can only be run on the Merced\n";
    exit 0;
}

#
# Get the total number of tests we're going to perform.
#
my $count = 0;
foreach $type (@types) {
    my @variants = split(/ /, $variant_mapping{$type});
    $count += @preds * @variants;
}

print "1..$count\n";

my $nr=0;
foreach $type (@types) {
    my @variants = split(/ /, $variant_mapping{$type});
    foreach $pred (@preds) {
	foreach $var (@variants) {
	    run ++$nr, $type, $pred, $var;
        }
    }
}

exit 0;
