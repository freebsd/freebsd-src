#

package IO::Seekable;

=head1 NAME

IO::Seekable - supply seek based methods for I/O objects

=head1 SYNOPSIS

    use IO::Seekable;
    package IO::Something;
    @ISA = qw(IO::Seekable);

=head1 DESCRIPTION

C<IO::Seekable> does not have a constructor of its own as it is intended to
be inherited by other C<IO::Handle> based objects. It provides methods
which allow seeking of the file descriptors.

If the C functions fgetpos() and fsetpos() are available, then
C<IO::File::getpos> returns an opaque value that represents the
current position of the IO::File, and C<IO::File::setpos> uses
that value to return to a previously visited position.

See L<perlfunc> for complete descriptions of each of the following
supported C<IO::Seekable> methods, which are just front ends for the
corresponding built-in functions:

    seek
    tell

=head1 SEE ALSO

L<perlfunc>, 
L<perlop/"I/O Operators">,
L<IO::Handle>
L<IO::File>

=head1 HISTORY

Derived from FileHandle.pm by Graham Barr E<lt>bodg@tiuk.ti.comE<gt>

=cut

require 5.000;
use Carp;
use strict;
use vars qw($VERSION @EXPORT @ISA);
use IO::Handle qw(SEEK_SET SEEK_CUR SEEK_END);
require Exporter;

@EXPORT = qw(SEEK_SET SEEK_CUR SEEK_END);
@ISA = qw(Exporter);

$VERSION = "1.06";

sub seek {
    @_ == 3 or croak 'usage: $fh->seek(POS, WHENCE)';
    seek($_[0], $_[1], $_[2]);
}

sub tell {
    @_ == 1 or croak 'usage: $fh->tell()';
    tell($_[0]);
}

1;
