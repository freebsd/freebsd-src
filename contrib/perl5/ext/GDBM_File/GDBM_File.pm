# GDBM_File.pm -- Perl 5 interface to GNU gdbm library.

=head1 NAME

GDBM_File - Perl5 access to the gdbm library.

=head1 SYNOPSIS

    use GDBM_File ;
    tie %hash, 'GDBM_File', $filename, &GDBM_WRCREAT, 0640;
    # Use the %hash array.
    untie %hash ;

=head1 DESCRIPTION

B<GDBM_File> is a module which allows Perl programs to make use of the
facilities provided by the GNU gdbm library.  If you intend to use this
module you should really have a copy of the gdbm manualpage at hand.

Most of the libgdbm.a functions are available through the GDBM_File
interface.

=head1 AVAILABILITY

Gdbm is available from any GNU archive.  The master site is
C<prep.ai.mit.edu>, but your are strongly urged to use one of the many
mirrors.   You can obtain a list of mirror sites by issuing the
command	C<finger fsf@prep.ai.mit.edu>.

=head1 BUGS

The available functions and the gdbm/perl interface need to be documented.

=head1 SEE ALSO

L<perl(1)>, L<DB_File(3)>. 

=cut

package GDBM_File;

use strict;
use vars qw($VERSION @ISA @EXPORT $AUTOLOAD);

require Carp;
require Tie::Hash;
require Exporter;
use AutoLoader;
require DynaLoader;
@ISA = qw(Tie::Hash Exporter DynaLoader);
@EXPORT = qw(
	GDBM_CACHESIZE
	GDBM_FAST
	GDBM_INSERT
	GDBM_NEWDB
	GDBM_READER
	GDBM_REPLACE
	GDBM_WRCREAT
	GDBM_WRITER
);

$VERSION = "1.00";

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    Carp::croak("Your vendor has not defined GDBM_File macro $constname, used");
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap GDBM_File $VERSION;

# Preloaded methods go here.  Autoload methods go after __END__, and are
# processed by the autosplit program.

1;
__END__
