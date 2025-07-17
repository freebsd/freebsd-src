#!/usr/local/bin/perl
#
# Remove almost all of the $ FreeBSD $ tags in the tree.
#
# Copyright (c) 2023, Warner Losh
# SPDX-License-Identifier: BSD-2-Clause
#
# Needs p5-File-Lib package
# Caveat Emptor
#
use strict;
use warnings;
use File::Find;
  
sub skip_list
{
    my $fn = $_[0];

    if ($fn =~ m=^./contrib/=) {
	return 1;
    }
    if ($fn =~ m=^./sys/contrib/=) {
	return 1;
    }
    if ($fn =~ m=^./cddl/contrib/=) {
	return 1;
    }
    if ($fn =~ m=^./crypto/=) {
	return 1;
    }
    if ($fn =~ m=^./.git/=) {
	return 1;
    }
    if ($fn =~ m=~$=) {
	return 1;
    }
    return 0;
}

my $pretty;
my $pattern;
my $repl;
my $count;
my $syshash;
my $hash;

sub do_one
{
    $pretty = $_[0];
    $pattern = $_[1];
    $repl = "";
    $repl = $_[2];
    $syshash = $_[3];
    $hash = $_[4];
    $count = 0;

    sub findfiles
    {
	return unless -f;
	my $fn="$File::Find::name";
	return if skip_list($fn);
	open my $fh, '<', $_ or die "Can't open $fn: $!\n";
	local $/;
	my $file = <$fh>;
	close $fh;
	my $len = length($file);

	$file =~ s=$pattern=$repl=gm;
	my $len2 = length($file);
	return if $len2 == $len;
	print "$pretty: $fn\n";
	open my $fhw, '>', $_ or die "Can't write $fn: $!\n";
	print $fhw $file;
	close $fhw;
	$count++;
    }

    $count = 0;
    find({ wanted => \&findfiles, }, './sys');
    if ($count > 0) {
	print "Changed $pretty\n";
	system("git commit -a -m'sys: Remove \$FreeBSD\$: $pretty\n\nRemove /$pattern/\n\nSimilar commit in current:\n(cherry picked from commit $syshash)'");
    }
    $count = 0;
    find({ wanted => \&findfiles, }, '.');
    if ($count > 0) {
	print "Changed $pretty\n";
	system("git commit -a -m'Remove \$FreeBSD\$: $pretty\n\nRemove /$pattern/\n\nSimilar commit in main:\n(cherry picked from commit $hash)'");
    }
}

# These are the commits to head
# 9524e274b548 Remove $FreeBSD$: one-line xdr pattern
# 26a58599a09a Remove $FreeBSD$: one-line forth tag
# 401ab69cff8f Remove $FreeBSD$: one-line ps tag
# 6ef644f5889a Remove $FreeBSD$: one-line lua tag
# 9636a14538f5 Remove $FreeBSD$: two-line lua tag
# 8c99d94c900f sys: Remove $FreeBSD$: two-line lua tag
# ae992a336e8d Remove $FreeBSD$: one-line catalog
# 2063df147163 sys: Remove $FreeBSD$: one-line catalog
# 05248206f720 Remove $FreeBSD$: one-line bare tag
# 78d146160dc5 sys: Remove $FreeBSD$: one-line bare tag
# b2c76c41be32 Remove $FreeBSD$: one-line nroff pattern
# fa9896e082a1 Remove $FreeBSD$: two-line nroff pattern
# 9e7892125655 sys: Remove $FreeBSD$: two-line nroff pattern
# d0b2dbfa0ecf Remove $FreeBSD$: one-line sh pattern
# 031beb4e239b sys: Remove $FreeBSD$: one-line sh pattern
# b1cfcffa89e6 Remove $FreeBSD$: one-line .S pattern
# d4bf8003ee42 sys: Remove $FreeBSD$: one-line .S pattern
# c8573564095b Remove $FreeBSD$: alt one-line .c pattern
# da5432eda807 Remove $FreeBSD$: alt two-line .c pattern
# 1d386b48a555 Remove $FreeBSD$: one-line .c pattern
# 685dc743dc3b sys: Remove $FreeBSD$: one-line .c pattern
# e5d258c9e599 Remove $FreeBSD$: two-line .c pattern
# dfc016587a1e sys: Remove $FreeBSD$: two-line .c pattern
# 2a63c3be1582 Remove $FreeBSD$: one-line .c comment pattern
# 71625ec9ad2a sys: Remove $FreeBSD$: one-line .c comment pattern
# 42b388439bd3 Remove $FreeBSD$: one-line .h pattern
# 2ff63af9b88c sys: Remove $FreeBSD$: one-line .h pattern
# b3e7694832e8 Remove $FreeBSD$: two-line .h pattern
# 95ee2897e98f sys: Remove $FreeBSD$: two-line .h pattern
# d54a7d337331 Remove $FreeBSD$: one-line m4 tag
# 82a265ad9bad sys: Remove $FreeBSD$: sound driver version

# Note: Do two line before one line
#	commit message			pattern					replacement or ''		sys hash	src hash
do_one("sound driver version",		'SND_DECLARE_FILE\("\$FreeBSD\$"\);',	'SND_DECLARE_FILE("");',	'',		'82a265ad9bad');
do_one("one-line m4 tag",		'^dnl\s*\$FreeBSD\$.*$\n',		'',				'',		'd54a7d337331');
do_one("two-line .h pattern",		'^\s*\*\n \*\s+\$FreeBSD\$$\n',		'',				'95ee2897e98f',	'b3e7694832e8');
do_one("one-line .h pattern",		'^\s*\*+\s*\$FreeBSD\$.*$\n',		'',				'2ff63af9b88c',	'42b388439bd3');
do_one("one-line .c comment pattern",	'^/[*/]\s*\$FreeBSD\$.*\n',		'',				'71625ec9ad2a',	'2a63c3be1582');
do_one("two-line .c pattern",		'^#include\s+<sys/cdefs.h>.*$\n\s+__FBSDID\("\$FreeBSD\$"\);\n','',	'dfc016587a1e',	'e5d258c9e599');
do_one("one-line .c pattern",		'^[\s*]*__FBSDID\("\$FreeBSD\$"\);?\s*\n', '',				'685dc743dc3b',	'1d386b48a555');
do_one("alt two-line .c pattern",	'^\s*__RCSID\("\$FreeBSD\$"\);\n\n',	'',				'',		'da5432eda807');
do_one("alt one-line .c pattern",	'^\s*__RCSID\("\$FreeBSD\$"\);\n',	'',				'',		'c8573564095b');
do_one("one-line .S pattern",		'^\s\.(asciz|ident)\s+\"\$FreeBSD\$\".*\n', '',				'd4bf8003ee42',	'b1cfcffa89e6');
do_one("one-line sh pattern",		'^\s*#[#!]?\s*\$FreeBSD\$.*$\n',	'',				'031beb4e239b',	'd0b2dbfa0ecf');
do_one("two-line nroff pattern",	'^\.\\\\"\n\.\\\\"\s*\$FreeBSD\$$\n',	'',				'9e7892125655',	'fa9896e082a1');
do_one("one-line nroff pattern",	'^\.\\\\"\s*\$FreeBSD\$$\n',		'',				'',		'b2c76c41be32');
do_one("one-line bare tag",		'^\s*\$FreeBSD\$$\n',			'',				'78d146160dc5',	'05248206f720');
do_one("one-line catalog",		'^\s*\$\s*\$FreeBSD\$$\n',		'',				'2063df147163',	'ae992a336e8d');
do_one("two-line lua tag",		'^--\n--\s*\$FreeBSD\$.*$\n',		'',				'8c99d94c900f',	'9636a14538f5');
do_one("one-line lua tag",		'^--\s*\$FreeBSD\$.*$\n',		'',				'',		'6ef644f5889a');
do_one("one-line ps tag",		'^%\s*RCSID:\s*\$FreeBSD\$.*$\n',	'',				'',		'401ab69cff8f');
do_one("one-line forth tag",		'^\\\\[\s*]*\$FreeBSD\$.*$\n',		'',				'',		'26a58599a09a');
do_one("one-line xdr pattern",		'^\s*%\s*__FBSDID\("\$FreeBSD\$"\);?\s*\n', '',				'',		'9524e274b548');
exit;
