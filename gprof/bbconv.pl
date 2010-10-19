#! /usr/bin/perl

# This script converts a "bb.out" file into a format
# suitable for processing by gprof
#
# Copyright 2001 Free Software Foundation, Inc.
#
#   This file is part of GNU Binutils.
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
#   02110-1301, USA.

# Write a new-style gmon header

print pack("A4Ix12", "gmon", 1);


# The input file format contains header lines and data lines.
# Header lines contain a count of how many data lines follow before
# the next header line.  $blockcount is set to the count that
# appears in each header line, then decremented at each data line.
# $blockcount should always be zero at the start of a header line,
# and should never be zero at the start of a data line.

$blockcount=0;

while (<>) {
    if (/^File .*, ([0-9]+) basic blocks/) {
	print STDERR "Miscount: line $.\n" if ($blockcount != 0);
	$blockcount = $1;

	print pack("cI", 2, $blockcount);
    }
    if (/Block.*executed([ 0-9]+) time.* address= 0x([0-9a-fA-F]*)/) {
	print STDERR "Miscount: line $.\n" if ($blockcount == 0);
	$blockcount-- if ($blockcount > 0);

	$count = $1;
	$addr = hex $2;

	print pack("II",$addr,$count);
    }
}
