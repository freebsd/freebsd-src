#

package IO;

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

For more information on any of these modules, please see its respective
documentation.

=cut

use IO::Handle;
use IO::Seekable;
use IO::File;
use IO::Pipe;
use IO::Socket;

1;

