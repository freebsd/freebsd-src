#!/usr/bin/perl
# 
# Copyright (c) 2000  "HOSOKAWA, Tatsumi" <hosokawa@FreeBSD.org>
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
# $FreeBSD$
# 

if ($#ARGV != 1) {
    print STDERR "Usage: driver-remove.pl config_file BOOTMFS\n";
    exit 1;
}

$config = $ARGV[0];
$bootmfs = $ARGV[1];

open CONFIG, "< $config" or die "Cannot open $config.\n";
while (<CONFIG>) {
    s/#.*$//;
    if (/^(\w+)\s+(\w+)\s+(\d+)\s+(\w+)\s+\"(.*)\"\s*$/) {
	$drivers{$1} = 1;
    }
}
close CONFIG;

open BOOTMFS, "< $bootmfs" or die "Cannot open $bootmfs.\n";
while (<BOOTMFS>) {
    next if (/^device\s+(\w+)/ && $drivers{$1});
    push @bootmfs, $_;
}
close BOOTMFS;

open BOOTMFS, "> $bootmfs" or die "Cannot open $bootmfs.\n";
foreach (@bootmfs) {
    print BOOTMFS;
}
close BOOTMFS;
