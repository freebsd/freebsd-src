package filetest;

=head1 NAME

filetest - Perl pragma to control the filetest permission operators

=head1 SYNOPSIS

    $can_perhaps_read = -r "file";	# use the mode bits
    {
        use filetest 'access';		# intuit harder
        $can_really_read = -r "file";
    }
    $can_perhaps_read = -r "file";	# use the mode bits again

=head1 DESCRIPTION

This pragma tells the compiler to change the behaviour of the filetest
permissions operators, the C<-r> C<-w> C<-x> C<-R> C<-W> C<-X>
(see L<perlfunc>).

The default behaviour to use the mode bits as returned by the stat()
family of calls.  This, however, may not be the right thing to do if
for example various ACL (access control lists) schemes are in use.
For such environments, C<use filetest> may help the permission
operators to return results more consistent with other tools.

Each "use filetest" or "no filetest" affects statements to the end of
the enclosing block.

There may be a slight performance decrease in the filetests
when C<use filetest> is in effect, because in some systems
the extended functionality needs to be emulated.

B<NOTE>: using the file tests for security purposes is a lost cause
from the start: there is a window open for race conditions (who is to
say that the permissions will not change between the test and the real
operation?).  Therefore if you are serious about security, just try
the real operation and test for its success.  Think atomicity.

=head2 subpragma access

Currently only one subpragma, C<access> is implemented.  It enables
(or disables) the use of access() or similar system calls.  This
extended filetest functionality is used only when the argument of the
operators is a filename, not when it is a filehandle.

=cut

$filetest::hint_bits = 0x00400000;

sub import {
    if ( $_[1] eq 'access' ) {
	$^H |= $filetest::hint_bits;
    } else {
	die "filetest: the only implemented subpragma is 'access'.\n";
    }
}

sub unimport {
    if ( $_[1] eq 'access' ) {
	$^H &= ~$filetest::hint_bits;
    } else {
	die "filetest: the only implemented subpragma is 'access'.\n";
    }
}

1;
