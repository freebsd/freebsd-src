package File::Spec;

require Exporter;

@ISA = qw(Exporter);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	
);
@EXPORT_OK = qw($Verbose);

use strict;
use vars qw(@ISA $VERSION $Verbose);

$VERSION = '0.6';

$Verbose = 0;

require File::Spec::Unix;


sub load {
	my($class,$OS) = @_;
	if ($OS eq 'VMS') {
		require File::Spec::VMS;
		require VMS::Filespec;
		'File::Spec::VMS'
	} elsif ($OS eq 'os2') {
		require File::Spec::OS2;
		'File::Spec::OS2'
	} elsif ($OS eq 'MacOS') {
		require File::Spec::Mac;
		'File::Spec::Mac'
	} elsif ($OS eq 'MSWin32') {
		require File::Spec::Win32;
		'File::Spec::Win32'
	} else {
		'File::Spec::Unix'
	}
}

@ISA = load('File::Spec', $^O);

1;
__END__

=head1 NAME

File::Spec - portably perform operations on file names

=head1 SYNOPSIS

C<use File::Spec;>

C<$x=File::Spec-E<gt>catfile('a','b','c');>

which returns 'a/b/c' under Unix.

=head1 DESCRIPTION

This module is designed to support operations commonly performed on file
specifications (usually called "file names", but not to be confused with the
contents of a file, or Perl's file handles), such as concatenating several
directory and file names into a single path, or determining whether a path
is rooted. It is based on code directly taken from MakeMaker 5.17, code
written by Andreas KE<ouml>nig, Andy Dougherty, Charles Bailey, Ilya
Zakharevich, Paul Schinder, and others.

Since these functions are different for most operating systems, each set of
OS specific routines is available in a separate module, including:

	File::Spec::Unix
	File::Spec::Mac
	File::Spec::OS2
	File::Spec::Win32
	File::Spec::VMS

The module appropriate for the current OS is automatically loaded by
File::Spec. Since some modules (like VMS) make use of OS specific
facilities, it may not be possible to load all modules under all operating
systems.

Since File::Spec is object oriented, subroutines should not called directly,
as in:

	File::Spec::catfile('a','b');
	
but rather as class methods:

	File::Spec->catfile('a','b');

For a reference of available functions, please consult L<File::Spec::Unix>,
which contains the entire set, and inherited by the modules for other
platforms. For further information, please see L<File::Spec::Mac>,
L<File::Spec::OS2>, L<File::Spec::Win32>, or L<File::Spec::VMS>.

=head1 SEE ALSO

File::Spec::Unix, File::Spec::Mac, File::Spec::OS2, File::Spec::Win32,
File::Spec::VMS, ExtUtils::MakeMaker

=head1 AUTHORS

Kenneth Albanowski <F<kjahds@kjahds.com>>, Andy Dougherty
<F<doughera@lafcol.lafayette.edu>>, Andreas KE<ouml>nig
<F<A.Koenig@franz.ww.TU-Berlin.DE>>, Tim Bunce <F<Tim.Bunce@ig.co.uk>>. VMS
support by Charles Bailey <F<bailey@newman.upenn.edu>>.  OS/2 support by
Ilya Zakharevich <F<ilya@math.ohio-state.edu>>. Mac support by Paul Schinder
<F<schinder@pobox.com>>.

=cut


1;
