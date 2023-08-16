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

sub do_one
{
    $pretty = $_[0];
    $pattern = $_[1];
    $repl = "";
    $repl = $_[2] if defined($_[2]);
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
	system("git commit -a -m'sys: Remove \$FreeBSD\$: $pretty\n\nRemove /$pattern/'");
    }
    $count = 0;
    find({ wanted => \&findfiles, }, '.');
    if ($count > 0) {
	print "Changed $pretty\n";
	system("git commit -a -m'Remove \$FreeBSD\$: $pretty\n\nRemove /$pattern/'");
    }
}

# Note: Do two line before one line
do_one("sound driver version", 'SND_DECLARE_FILE\("\$FreeBSD\$"\);', 'SND_DECLARE_FILE("");');
do_one("one-line m4 tag", '^dnl\s*\$FreeBSD\$.*$\n');
do_one("two-line .h pattern", '^\s*\*\n \*\s+\$FreeBSD\$$\n');
do_one("one-line .h pattern", '^\s*\*+\s*\$FreeBSD\$.*$\n');
do_one("one-line .c comment pattern", '^/[*/]\s*\$FreeBSD\$.*\n');
do_one("two-line .c pattern", '^#include\s+<sys/cdefs.h>.*$\n\s+__FBSDID\("\$FreeBSD\$"\);\n');
do_one("one-line .c pattern", '^[\s*]*__FBSDID\("\$FreeBSD\$"\);?\s*\n');
do_one("alt two-line .c pattern", '^\s*__RCSID\("\$FreeBSD\$"\);\n\n');
do_one("alt one-line .c pattern", '^\s*__RCSID\("\$FreeBSD\$"\);\n');
do_one("one-line .S pattern", '^\s\.(asciz|ident)\s+\"\$FreeBSD\$\".*\n');
do_one("one-line sh pattern", '^\s*#[#!]?\s*\$FreeBSD\$.*$\n');
do_one("two-line nroff pattern", '^\.\\\\"\n\.\\\\"\s*\$FreeBSD\$$\n');
do_one("one-line nroff pattern", '^\.\\\\"\s*\$FreeBSD\$$\n');
do_one("one-line bare tag", '^\s*\$FreeBSD\$$\n');
do_one("one-line catalog", '^\s*\$\s*\$FreeBSD\$$\n');
do_one("two-line lua tag", '^--\n--\s*\$FreeBSD\$.*$\n');
do_one("one-line lua tag", '^--\s*\$FreeBSD\$.*$\n');
do_one("one-line ps tag", '^%\s*RCSID:\s*\$FreeBSD\$.*$\n');
do_one("one-line forth tag", '^\\\\[\s*]*\$FreeBSD\$.*$\n');
do_one("one-line xdr pattern", '^\s*%\s*__FBSDID\("\$FreeBSD\$"\);?\s*\n');
exit;
