package IPC::Open2;

use strict;
use vars qw($VERSION @ISA @EXPORT);

require 5.000;
require Exporter;

$VERSION	= 1.01;
@ISA		= qw(Exporter);
@EXPORT		= qw(open2);

=head1 NAME

IPC::Open2, open2 - open a process for both reading and writing

=head1 SYNOPSIS

    use IPC::Open2;
    $pid = open2(\*RDR, \*WTR, 'some cmd and args');
      # or
    $pid = open2(\*RDR, \*WTR, 'some', 'cmd', 'and', 'args');

=head1 DESCRIPTION

The open2() function spawns the given $cmd and connects $rdr for
reading and $wtr for writing.  It's what you think should work 
when you try

    open(HANDLE, "|cmd args|");

The write filehandle will have autoflush turned on.

If $rdr is a string (that is, a bareword filehandle rather than a glob
or a reference) and it begins with ">&", then the child will send output
directly to that file handle.  If $wtr is a string that begins with
"<&", then WTR will be closed in the parent, and the child will read
from it directly.  In both cases, there will be a dup(2) instead of a
pipe(2) made.

open2() returns the process ID of the child process.  It doesn't return on
failure: it just raises an exception matching C</^open2:/>.

=head1 WARNING 

It will not create these file handles for you.  You have to do this yourself.
So don't pass it empty variables expecting them to get filled in for you.

Additionally, this is very dangerous as you may block forever.
It assumes it's going to talk to something like B<bc>, both writing to
it and reading from it.  This is presumably safe because you "know"
that commands like B<bc> will read a line at a time and output a line at
a time.  Programs like B<sort> that read their entire input stream first,
however, are quite apt to cause deadlock.  

The big problem with this approach is that if you don't have control 
over source code being run in the child process, you can't control
what it does with pipe buffering.  Thus you can't just open a pipe to
C<cat -v> and continually read and write a line from it.

=head1 SEE ALSO

See L<IPC::Open3> for an alternative that handles STDERR as well.  This
function is really just a wrapper around open3().

=cut

# &open2: tom christiansen, <tchrist@convex.com>
#
# usage: $pid = open2('rdr', 'wtr', 'some cmd and args');
#    or  $pid = open2('rdr', 'wtr', 'some', 'cmd', 'and', 'args');
#
# spawn the given $cmd and connect $rdr for
# reading and $wtr for writing.  return pid
# of child, or 0 on failure.  
# 
# WARNING: this is dangerous, as you may block forever
# unless you are very careful.  
# 
# $wtr is left unbuffered.
# 
# abort program if
#	rdr or wtr are null
# 	a system call fails

require IPC::Open3;

sub open2 {
    my ($read, $write, @cmd) = @_;
    local $Carp::CarpLevel = $Carp::CarpLevel + 1;
    return IPC::Open3::_open3('open2', scalar caller,
				$write, $read, '>&STDERR', @cmd);
}

1
