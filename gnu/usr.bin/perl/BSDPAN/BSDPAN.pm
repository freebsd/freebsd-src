# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42)
# <tobez@tobez.org> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Anton Berezin
# ----------------------------------------------------------------------------
#
# $FreeBSD$
#
package BSDPAN;
#
# The pod documentation for this module is at the end of this file.
#

my $bsdpan_path;	# Directory pathname of BSDPAN itself

BEGIN {
	# Find this module's own path.  Do this before anything else
	# happens, even before the rest of the module compiles.  Do this
	# by looking into undocumented entries in the main (::) symbol
	# table.
	for (keys %::) {
		$bsdpan_path = $1 if /^_<(.*\/|)BSDPAN.pm$/;
	}
	# deduce the BSDPAN's directory pathname
	$bsdpan_path = '.' if $bsdpan_path eq '';
	$bsdpan_path =~ tr|/|/|s;
	$bsdpan_path =~ s|/$||;
}

sub path {
	return $bsdpan_path;
}

sub perl_version {
	require Config;
	return $Config::Config{version};
}

sub perl_ver {
	require Config;
	# pre-5.6.0 perls
	return $Config::Config{apiversion} if exists $Config::Config{apiversion};
	# post-5.6.0 perls
	return $Config::Config{version};
}

sub perl_arch {
	require Config;
	# pre-5.6.0 perls
	return $Config::Config{archname} if exists $Config::Config{apiversion};
	# post-5.6.0 perls
	return 'mach';
}

sub builds_port {
	# Are we building a p5 port at the moment?
	# XXX There must be a more reliable way to check this.
	if (defined $ENV{ARCH}		||
	    defined $ENV{OPSYS}		||
	    defined $ENV{OSREL}		||
	    defined $ENV{OSVERSION}	||
	    defined $ENV{PORTOBJFORMAT}	||
	    defined $ENV{SYSTEMVERSION}) {
		return 1;
	} else {
		return 0;
	}
}

sub builds_standalone {
	return !BSDPAN->builds_port;
}

1;
__END__
=head1 NAME

BSDPAN - Symbiogenetic tool for Perl & BSD

=head1 SYNOPSIS

  use BSDPAN;
  $path = BSDPAN->path;
  $ver = BSDPAN->perl_version;
  $ver = BSDPAN->perl_ver;
  $arch = BSDPAN->perl_arch;
  $port = BSDPAN->builds_port;
  $noport = BSDPAN->builds_standalone;

=head1 DESCRIPTION

BSDPAN is the collection of modules that provides tighter than ever
integration of Perl into BSD Unix.

Currently, BSDPAN does the following:

=over 4

=item o makes p5- FreeBSD ports PREFIX-clean;

=item o registers Perl modules with FreeBSD package database.

=back

BSDPAN achieves this by overriding certain functionality of the core
Perl modules, ExtUtils::MM_Unix, and ExtUtils::Packlist.

BSDPAN B<module> itself just provides useful helper functions for the
rest of the modules in BSDPAN collection.

=head1 AUTHOR

Anton Berezin, tobez@tobez.org

=head1 SEE ALSO

perl(1), ExtUtils::MakeMaker(3), L<BSDPAN::Override(3)>,
L<BSDPAN::ExtUtils::MM_Unix(3)>, L<BSDPAN::ExtUtils::Packlist(3)>.

=cut
