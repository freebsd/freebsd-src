#

# Copyright 2006 Massachusetts Institute of Technology.
# All Rights Reserved.
#
# Export of this software from the United States of America may
#   require a specific license from the United States Government.
#   It is the responsibility of any person or organization contemplating
#   export to obtain such a license before exporting.
#
# WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
# distribute this software and its documentation for any purpose and
# without fee is hereby granted, provided that the above copyright
# notice appear in all copies and that both that copyright notice and
# this permission notice appear in supporting documentation, and that
# the name of M.I.T. not be used in advertising or publicity pertaining
# to distribution of the software without specific, written prior
# permission.  Furthermore if you modify this software you must label
# your software as modified software and not distribute it in such a
# fashion that it might be confused with the original M.I.T. software.
# M.I.T. makes no representations about the suitability of
# this software for any purpose.  It is provided "as is" without express
# or implied warranty.
#

$0 =~ s/^.*?([\w.-]+)$/$1/;

# The real stuff.

# Args: exportlist libfoo.so

# This code assumes the GNU version of nm.
# For now, we'll only run it on GNU/Linux systems, so that's okay.

if ($#ARGV != 1) {
    die "usage: $0 exportfile libfoo.so\n";
}
my($exfile, $libfile) = @ARGV;

@missing = ();
open NM, "nm -Dg --defined-only $libfile |" || die "can't run nm on $libfile: $!";
open EXPORT, "< $exfile" || die "can't read $exfile: $!";

@export = <EXPORT>;
map chop, @export;
@export = sort @export;

@found = ();
while (<NM>) {
    chop;
    s/^[0-9a-fA-F]+ +//;
    s/@@.*$//;
    next if /^A /;
    if (!/^[TDRBGS] /) {
	unlink $libfile;
	die "not sure what to do with '$_'";
    }
    s/^[TDRBGS] +//;
    push @found, $_;
}
@found = sort @found;
while ($#export >= 0 && $#found >= 0) {
    if ($#export >= 1 && $export[0] eq $export[1]) {
	print STDERR "Duplicate symbol in export list: $export[0]\n";
	exit(1);
    }
    if ($export[0] eq $found[0]) {
#	print "ok $export[0]\n";
	shift @export;
	shift @found;
    } elsif ($export[0] lt $found[0]) {
	push @missing, shift @export;
    } else {
	# Ignore added symbols, for now.
	shift @found;
    }
}
if ($#export >= 0) { @missing = (@missing, @export); }
if ($#missing >= 0) {
    print STDERR "Missing symbols:\n\t", join("\n\t", @missing), "\n";
#    unlink $libfile;
    exit(1);
}
exit 0;
