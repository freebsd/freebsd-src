# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42)
# <tobez@tobez.org> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Anton Berezin
# ----------------------------------------------------------------------------
#
# $FreeBSD$
#
package BSDPAN::ExtUtils::MM_Unix;
#
# The pod documentation for this module is at the end of this file.
#
use strict;
use Carp;
use BSDPAN;
use BSDPAN::Override;

sub init_main {
	my $orig = shift;
	my $him = $_[0];

	my @r = &$orig;

	# MakeMaker insist to use perl system path first;
	# free it of this misconception, since we know better.
	$him->{PERL_LIB_REAL} = $him->{PERL_LIB};
	$him->{PERL_LIB_BSDPAN} = BSDPAN->path;
	$him->{PERL_LIB} = $him->{PERL_LIB_BSDPAN};

	# MakeMaker is pretty lame when the user specifies PREFIX.
	# It has too fine granularity regarding the various places
	# it installs things in.  So in order to make a port PREFIX-
	# clean we modify some parameters it does not usually touch.
	#
	# XXX MakeMaker does some `clever' tricks depending whether
	# PREFIX contains the `perl' substring or not.  This severely
	# confuses port's PLIST, so we avoid such things here.
	#
	# This code should arguably do what it does even in the
	# case we are not installing a port, but let's be conservative
	# here and not violate Perl's own POLA.
	if ($him->{PREFIX} ne '/usr/local' && BSDPAN->builds_port) {
		my $site_perl = "lib/perl5/site_perl";
		my $perl_ver = BSDPAN->perl_ver;
		my $perl_version = BSDPAN->perl_version;
		my $perl_arch = BSDPAN->perl_arch;
		my $perl_man = "lib/perl5/$perl_version/man";
		$him->{INSTALLSITELIB}  = "\$(PREFIX)/$site_perl/$perl_ver";
		$him->{INSTALLSITEARCH} =
		    "\$(PREFIX)/$site_perl/$perl_ver/$perl_arch";
		$him->{INSTALLBIN}      = "\$(PREFIX)/bin";
		$him->{INSTALLSCRIPT}   = "\$(PREFIX)/bin";
		# these strange values seem to be default
		$him->{INSTALLMAN1DIR}  = "\$(PREFIX)/man/man1";
		$him->{INSTALLMAN3DIR}  = "\$(PREFIX)/$perl_man/man3";
	}

	return @r;
}

sub tool_xsubpp {
	my $orig = shift;
	my $him = $_[0];

	$him->{PERL_LIB} = $him->{PERL_LIB_REAL};
	my @r = &$orig;
	$him->{PERL_LIB} = $him->{PERL_LIB_BSDPAN};

	return @r;
}

BEGIN {
	override 'init_main', \&init_main;
	override 'tool_xsubpp', \&tool_xsubpp;
}

1;
=head1 NAME

BSDPAN::ExtUtils::MM_Unix - Override ExtUtils::MM_Unix functionality

=head1 SYNOPSIS

   None

=head1 DESCRIPTION

BSDPAN::ExtUtils::MM_Unix overrides two subs from the standard perl
module ExtUtils::MM_Unix.

The overridden init_main() first calls the original init_main().  Then,
if the Perl port build is detected, and the PREFIX stored in the
ExtUtils::MM_Unix object is not F</usr/local/>, it proceeds to change
various installation paths ExtUtils::MM_Unix maintains, to match PREFIX.

The overridden tool_xsubpp() sub temporarily undoes the change in the
environment done by the overridden init_main(), so that xsubpp(1) and
other XS tools will be searched in the right place.

Thus, BSDPAN::ExtUtils::MM_Unix is responsible for making p5- ports
PREFIX-clean.

=head1 AUTHOR

Anton Berezin, tobez@tobez.org

=head1 SEE ALSO

perl(1), L<BSDPAN(3)>, L<BSDPAN::Override(3)>, ports(7).

=cut
