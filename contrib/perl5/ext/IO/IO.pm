#

package IO;

use XSLoader ();
use Carp;

$VERSION = "1.20";
XSLoader::load 'IO', $VERSION;

sub import {
    shift;
    my @l = @_ ? @_ : qw(Handle Seekable File Pipe Socket Dir);

    eval join("", map { "require IO::" . (/(\w+)/)[0] . ";\n" } @l)
	or croak $@;
}

1;

__END__

=head1 NAME

IO - load various IO modules

=head1 SYNOPSIS

    use IO;

=head1 DESCRIPTION

C<IO> provides a simple mechanism to load some of the IO modules at one go.
Currently this includes:

      IO::Handle
      IO::Seekable
      IO::File
      IO::Pipe
      IO::Socket
      IO::Dir

For more information on any of these modules, please see its respective
documentation.

=cut

