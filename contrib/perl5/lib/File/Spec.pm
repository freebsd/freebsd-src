package File::Spec;

use strict;
use vars qw(@ISA $VERSION);

$VERSION = 0.82 ;

my %module = (MacOS   => 'Mac',
	      MSWin32 => 'Win32',
	      os2     => 'OS2',
	      VMS     => 'VMS',
	      epoc    => 'Epoc');

my $module = $module{$^O} || 'Unix';
require "File/Spec/$module.pm";
@ISA = ("File::Spec::$module");

1;
__END__

=head1 NAME

File::Spec - portably perform operations on file names

=head1 SYNOPSIS

	use File::Spec;

	$x=File::Spec->catfile('a', 'b', 'c');

which returns 'a/b/c' under Unix. Or:

	use File::Spec::Functions;

	$x = catfile('a', 'b', 'c');

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
File::Spec. Since some modules (like VMS) make use of facilities available
only under that OS, it may not be possible to load all modules under all
operating systems.

Since File::Spec is object oriented, subroutines should not called directly,
as in:

	File::Spec::catfile('a','b');

but rather as class methods:

	File::Spec->catfile('a','b');

For simple uses, L<File::Spec::Functions> provides convenient functional
forms of these methods.

For a list of available methods, please consult L<File::Spec::Unix>,
which contains the entire set, and which is inherited by the modules for
other platforms. For further information, please see L<File::Spec::Mac>,
L<File::Spec::OS2>, L<File::Spec::Win32>, or L<File::Spec::VMS>.

=head1 SEE ALSO

File::Spec::Unix, File::Spec::Mac, File::Spec::OS2, File::Spec::Win32,
File::Spec::VMS, File::Spec::Functions, ExtUtils::MakeMaker

=head1 AUTHORS

Kenneth Albanowski <F<kjahds@kjahds.com>>, Andy Dougherty
<F<doughera@lafcol.lafayette.edu>>, Andreas KE<ouml>nig
<F<A.Koenig@franz.ww.TU-Berlin.DE>>, Tim Bunce <F<Tim.Bunce@ig.co.uk>>. VMS
support by Charles Bailey <F<bailey@newman.upenn.edu>>.  OS/2 support by
Ilya Zakharevich <F<ilya@math.ohio-state.edu>>. Mac support by Paul Schinder
<F<schinder@pobox.com>>.  abs2rel() and rel2abs() written by
Shigio Yamaguchi <F<shigio@tamacom.com>>, modified by Barrie Slaymaker
<F<barries@slaysys.com>>.  splitpath(), splitdir(), catpath() and catdir()
by Barrie Slaymaker.
