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

if ($#ARGV != 2) {
    print STDERR "Usage: driver-copy.pl config_file src_ko_dir dst_ko_dir\n";
    exit 1;
}

$config = $ARGV[0];
$srcdir = $ARGV[1];
$dstdir = $ARGV[2];

open CONFIG, "< $config" or die "Cannot open $config.\n";
while (<CONFIG>) {
    s/#.*$//;
    if (/^(\w+)\s+(\w+)\s+(\d+)\s+(\w+)\s+\"(.*)\"\s*$/) {
	$flp{$2} = $3;
	$dsc{$2} = $5;
    }
}
close CONFIG;

-d $srcdir or die "Cannot find $srcdir directory.\n";
-d $dstdir or die "Cannot find $dstdir directory.\n";

undef $/;

foreach $f (sort keys %flp) {
    if ($flp{$f} == 1) {
	print STDERR "$f: There's nothing to do with driver on first floppy.\n";
    }
    elsif ($flp{$f} == 2) {
	$srcfile = $srcdir . '/' . $f . '.ko';
	$dstfile = $dstdir . '/' . $f . '.ko';
	$dscfile = $dstdir . '/' . $f . '.dsc';
	print STDERR "Copying $f.ko to $dstdir\n";
	open SRC, "< $srcfile" or die "Cannot open $srcfile\n";
	$file = <SRC>;
	close SRC;
	open DST, "> $dstfile" or die "Cannot open $dstfile\n";
	print DST $file;
	close DST;
	open DSC, "> $dscfile" or die "Cannot open $dscfile\n";
	print DSC $dsc{$f};
	close DSC;
    }
    elsif ($flp{$f} == 3) {
	# third driver floppy (currently not implemnted yet...)
	print STDERR "3rd driver floppy support has not implemented yet\n";
	exit 1;
    }
}
