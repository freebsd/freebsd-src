#!/usr/bin/env perl
#
use strict;
use warnings;
use diagnostics;

my $help_txt = "help.txt";
my $help_c   = "help.c";
my $top = "
/*
 * This was generated from $help_txt.  Do not modify directly.
 *
 * Copyright (c) 2002-2009 Johann George.  All rights reserved.
 * Copyright (c) 2006-2009 QLogic Corporation.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
char *Usage[] ={
";
my $end = "
    0,
};
";

sub panic {
    print STDERR @_, "\n";
    exit 1;
}

sub main() {
    my %defs;
        $defs{$_} = 1 for (@ARGV);
    my $iFile;
    open($iFile, "<", $help_txt) or
        panic("cannot find $help_txt");
    my $str = "";
    my $keep = 1;
    while (<$iFile>) {
        chomp;
        s/\s+$//;
        if (/^    / or /^$/) {
            if ($keep) {
                s///;
                s/(["\\])/\\$1/g;
                s/$/\\n/;
                if (/^(.{68}(?>[^\\]?))(..*)/) {
                    $str .= " "x8  . "\"$1\"\n";
                    $str .= " "x12 . "\"$2\"\n";
                } else {
                    $str .= " "x8 . "\"$_\"\n";
                }
            }
        } else {
            my @args = split;
            my $arg0 = lc(shift @args);
            $keep = 1;
            for (@args) {
                if (/^\+(.*)/) {
                    $keep = 0 unless ($defs{$1});
                } elsif (/^-(.*)/) {
                    $keep = 0 if ($defs{$1});
                }
            }
            if ($keep) {
                if ($str) {
                    chop $str;
                    $str .= ",\n";
                }
                $str .= " "x4 . "\"$arg0\",\n";
            }
        }
    }
    close $iFile;
    if ($str) {
        chop $str;
        $str .= ",\n";
    }
    $top =~ s/^\n//;
    $end =~ s/^\n//;
    my $oFile;
    open($oFile, ">", $help_c) or
        panic("cannot create $help_c");
    print $oFile $top, $str, $end;
    close $oFile;
}

main();
