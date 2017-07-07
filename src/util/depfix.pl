#!env perl -w
#
# Copyright 1995,2001,2002,2003,2004,2005,2009 by the Massachusetts Institute of Technology.
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

eval 'exec perl -S $0 ${1+"$@"}'
  if 0;
$0 =~ s/^.*?(\w+)[\.\w+]*$/$1/;

# Input: srctop thisdir srcdir buildtop stlibobjs

# Notes: myrelativedir is something like "lib/krb5/asn.1" or ".".
# stlibobjs will usually be empty, or include spaces.

# A typical set of inputs, produced with srcdir=.. at top level:
#
# top_srcdir = ../../../util/et/../..
# thisdir = util/et
# srcdir = ../../../util/et
# BUILDTOP = ../..
# STLIBOBJS = error_message.o et_name.o com_err.o

my($top_srcdir,$thisdir,$srcdir,$BUILDTOP,$STLIBOBJS) = @ARGV;

if (0) {
    print STDERR "top_srcdir = $top_srcdir\n";
    print STDERR "BUILDTOP = $BUILDTOP\n";
    print STDERR "STLIBOBJS = $STLIBOBJS\n";
}

#$srcdirpat = quotemeta($srcdir);

my($extrasuffixes) = ($STLIBOBJS ne "");

sub my_qm {
    my($x) = @_;
    $x = quotemeta($x);
    $x =~ s,\\/,/,g;
    return $x;
}

sub strrep {
    my($old,$new,$s) = @_;
    my($l) = "strrep('$old','$new','$s')";
    my($out) = "";
    while ($s ne "") {
	my($i) = index($s, $old);
	if ($i == -1) {
	    $out .= $s;
	    $s = "";
	} else {
	    $out .= substr($s, 0, $i) . $new;
	    if (length($s) > $i + length($old)) {
		$s = substr($s, $i + length($old));
	    } else {
		$s = "";
	    }
	}
    }
#    print STDERR "$l = '$out'\n";
    return $out;
}

sub do_subs {
    local($_) = @_;
    s,\\$, \\,g; s, + \\$, \\,g;
    s,//+,/,g; s, \./, ,g;
    if ($extrasuffixes) {
	# Only care about the additional prefixes if we're building
	# shared libraries.
	s,^([a-zA-Z0-9_\-]*)\.o:,$1.so $1.po \$(OUTPRE)$1.\$(OBJEXT):,;
    } else {
	s,^([a-zA-Z0-9_\-]*)\.o:,\$(OUTPRE)$1.\$(OBJEXT):,;
    }
    # Recognize $(top_srcdir) and variants.
    my($srct) = $top_srcdir . "/";
    $_ = strrep(" $srct", " \$(top_srcdir)/", $_);
#    s, $pat, \$(top_srcdir)/,go;
    while ($srct =~ m,/[a-z][a-zA-Z0-9_.\-]*/\.\./,) {
	$srct =~ s,/[a-z][a-zA-Z0-9_.\-]*/\.\./,/,;
	$_ = strrep(" $srct", " \$(top_srcdir)/", $_);
    }
    # Now try to produce pathnames relative to $(srcdir).
    if ($thisdir eq ".") {
	# blah
    } else {
	my($pat) = " \$(top_srcdir)/$thisdir/";
	my($out) = " \$(srcdir)/";
	$_ = strrep($pat, $out, $_);
	while ($pat =~ m,/[a-z][a-zA-Z0-9_.\-]*/$,) {
	    $pat =~ s,/[a-z][a-zA-Z0-9_.\-]*/$,/,;
	    $out .= "../";
	    if ($pat ne " \$(top_srcdir)/") {
		$_ = strrep($pat, $out, $_);
	    }
	}
    }
    # Now substitute for BUILDTOP:
    $_ = strrep(" $BUILDTOP/", " \$(BUILDTOP)/", $_);
    return $_;
}

sub do_subs_2 {
    local($_) = @_;
    # Add a trailing space.
    s/$/ /;
    # Remove excess spaces.
    s/  */ /g;
    # Delete headers external to the source and build tree.
    s; /[^ ]*;;g;
    # Remove foo/../ sequences.
    while (m/\/[a-z][a-z0-9_.\-]*\/\.\.\//) {
	s//\//g;
    }
    # Use VPATH.
    s;\$\(srcdir\)/([^ /]* );$1;g;

    $_ = &uniquify($_);

    # Allow override of some util dependencies in case local tools are used.
    s;\$\(BUILDTOP\)/include/com_err.h ;\$(COM_ERR_DEPS) ;g;
    s;\$\(BUILDTOP\)/include/ss/ss.h \$\(BUILDTOP\)/include/ss/ss_err.h ;\$(SS_DEPS) ;g;
    s;\$\(BUILDTOP\)/include/db-config.h \$\(BUILDTOP\)/include/db.h ;\$(DB_DEPS) ;g;
    s;\$\(BUILDTOP\)/include/verto.h ;\$(VERTO_DEPS) ;g;
    if ($thisdir eq "util/gss-kernel-lib") {
	# Here com_err.h is used from the current directory.
	s;com_err.h ;\$(COM_ERR_DEPS) ;g;
    }
    if ($thisdir eq "lib/krb5/ccache") {
	# These files are only used (and kcmrpc.h only generated) on OS X.
	# There are conditional dependencies in Makefile.in.
	s;kcmrpc.h ;;g;
	s;kcmrpc_types.h ;;g;
    }

    $_ = &uniquify($_);

    # Delete trailing whitespace.
    s; *$;;g;

    return $_;
}

sub uniquify {
    # Apparently some versions of gcc -- like
    # "gcc version 3.4.4 20050721 (Red Hat 3.4.4-2)"
    # -- will sometimes emit duplicate header file names.
    local($_) = @_;
    my(@sides) = split ": ", $_;
    my($lhs) = "";
    if ($#sides == 1) {
	$lhs = $sides[0] . ": ";
	$_ = $sides[1];
    }
    my(@words) = split " ", $_;
    my($w);
    my($result) = "";
    my(%seen);
    undef %seen;
    foreach $w (sort { $a cmp $b; } @words) {
	next if defined($seen{$w});
	$seen{$w} = 1;
	if ($result ne "") { $result .= " "; }
	$result .= $w;
    }
    return $lhs . $result . " ";
}

sub split_lines {
    local($_) = @_;
    s/(.{50}[^ ]*) /$1 \\\n  /g;
    return $_ . "\n";
}

print <<EOH ;
#
# Generated makefile dependencies follow.
#
EOH
my $buf = '';
while (<STDIN>) {
    # Strip newline.
    chop;
    next if /^\s*#/;
    # Do directory-specific path substitutions on each filename read.
    $_ = &do_subs($_);
    if (m/\\$/) {
	chop;
	$buf .= $_;
    } else {
	$buf = &do_subs_2($buf . $_);
	print &split_lines($buf);
	$buf = '';
    }
}
exit 0;
