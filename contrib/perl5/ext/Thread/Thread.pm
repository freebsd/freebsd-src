package Thread;
require Exporter;
use XSLoader ();
our($VERSION, @ISA, @EXPORT);

$VERSION = "1.0";

@ISA = qw(Exporter);
@EXPORT_OK = qw(yield cond_signal cond_broadcast cond_wait async);

=head1 NAME

Thread - manipulate threads in Perl (EXPERIMENTAL, subject to change)

=head1 CAVEAT

The Thread extension requires Perl to be built in a particular way to
enable the older 5.005 threading model.  Just to confuse matters, there
is an alternate threading model known as "ithreads" that does NOT
support this extension.  If you are using a binary distribution such
as ActivePerl that is built with ithreads support, this extension CANNOT
be used.

=head1 SYNOPSIS

    use Thread;

    my $t = new Thread \&start_sub, @start_args;

    $result = $t->join;
    $result = $t->eval;
    $t->detach;

    if($t->equal($another_thread)) {
    	# ...
    }

    my $tid = Thread->self->tid; 
    my $tlist = Thread->list;

    lock($scalar);
    yield();

    use Thread 'async';

=head1 DESCRIPTION

    WARNING: Threading is an experimental feature.  Both the interface
    and implementation are subject to change drastically.  In fact, this
    documentation describes the flavor of threads that was in version
    5.005.  Perl 5.6.0 and later have the beginnings of support for
    interpreter threads, which (when finished) is expected to be
    significantly different from what is described here.  The information
    contained here may therefore soon be obsolete.  Use at your own risk!

The C<Thread> module provides multithreading support for perl.

=head1 FUNCTIONS

=over 8

=item new \&start_sub

=item new \&start_sub, LIST

C<new> starts a new thread of execution in the referenced subroutine. The
optional list is passed as parameters to the subroutine. Execution
continues in both the subroutine and the code after the C<new> call.

C<new Thread> returns a thread object representing the newly created
thread.

=item lock VARIABLE

C<lock> places a lock on a variable until the lock goes out of scope.  If
the variable is locked by another thread, the C<lock> call will block until
it's available. C<lock> is recursive, so multiple calls to C<lock> are
safe--the variable will remain locked until the outermost lock on the
variable goes out of scope.

Locks on variables only affect C<lock> calls--they do I<not> affect normal
access to a variable. (Locks on subs are different, and covered in a bit)
If you really, I<really> want locks to block access, then go ahead and tie
them to something and manage this yourself. This is done on purpose. While
managing access to variables is a good thing, perl doesn't force you out of
its living room...

If a container object, such as a hash or array, is locked, all the elements
of that container are not locked. For example, if a thread does a C<lock
@a>, any other thread doing a C<lock($a[12])> won't block.

You may also C<lock> a sub, using C<lock &sub>. Any calls to that sub from
another thread will block until the lock is released. This behaviour is not
equivalent to declaring the sub with the C<locked> attribute.  The C<locked>
attribute serializes access to a subroutine, but allows different threads
non-simultaneous access. C<lock &sub>, on the other hand, will not allow
I<any> other thread access for the duration of the lock.

Finally, C<lock> will traverse up references exactly I<one> level.
C<lock(\$a)> is equivalent to C<lock($a)>, while C<lock(\\$a)> is not.

=item async BLOCK;

C<async> creates a thread to execute the block immediately following
it. This block is treated as an anonymous sub, and so must have a
semi-colon after the closing brace. Like C<new Thread>, C<async> returns a
thread object.

=item Thread->self

The C<Thread-E<gt>self> function returns a thread object that represents
the thread making the C<Thread-E<gt>self> call.

=item Thread->list

C<Thread-E<gt>list> returns a list of thread objects for all running and
finished but un-C<join>ed threads.

=item cond_wait VARIABLE

The C<cond_wait> function takes a B<locked> variable as a parameter,
unlocks the variable, and blocks until another thread does a C<cond_signal>
or C<cond_broadcast> for that same locked variable. The variable that
C<cond_wait> blocked on is relocked after the C<cond_wait> is satisfied.
If there are multiple threads C<cond_wait>ing on the same variable, all but
one will reblock waiting to reaquire the lock on the variable. (So if
you're only using C<cond_wait> for synchronization, give up the lock as
soon as possible)

=item cond_signal VARIABLE

The C<cond_signal> function takes a locked variable as a parameter and
unblocks one thread that's C<cond_wait>ing on that variable. If more than
one thread is blocked in a C<cond_wait> on that variable, only one (and
which one is indeterminate) will be unblocked.

If there are no threads blocked in a C<cond_wait> on the variable, the
signal is discarded.

=item cond_broadcast VARIABLE

The C<cond_broadcast> function works similarly to C<cond_signal>.
C<cond_broadcast>, though, will unblock B<all> the threads that are blocked
in a C<cond_wait> on the locked variable, rather than only one.

=item yield

The C<yield> function allows another thread to take control of the
CPU. The exact results are implementation-dependent.

=back

=head1 METHODS

=over 8

=item join

C<join> waits for a thread to end and returns any values the thread exited
with. C<join> will block until the thread has ended, though it won't block
if the thread has already terminated.

If the thread being C<join>ed C<die>d, the error it died with will be
returned at this time. If you don't want the thread performing the C<join>
to die as well, you should either wrap the C<join> in an C<eval> or use the
C<eval> thread method instead of C<join>.

=item eval

The C<eval> method wraps an C<eval> around a C<join>, and so waits for a
thread to exit, passing along any values the thread might have returned.
Errors, of course, get placed into C<$@>.

=item detach

C<detach> tells a thread that it is never going to be joined i.e.
that all traces of its existence can be removed once it stops running.
Errors in detached threads will not be visible anywhere - if you want
to catch them, you should use $SIG{__DIE__} or something like that.

=item equal 

C<equal> tests whether two thread objects represent the same thread and
returns true if they do.

=item tid

The C<tid> method returns the tid of a thread. The tid is a monotonically
increasing integer assigned when a thread is created. The main thread of a
program will have a tid of zero, while subsequent threads will have tids
assigned starting with one.

=back

=head1 LIMITATIONS

The sequence number used to assign tids is a simple integer, and no
checking is done to make sure the tid isn't currently in use. If a program
creates more than 2^32 - 1 threads in a single run, threads may be assigned
duplicate tids. This limitation may be lifted in a future version of Perl.

=head1 SEE ALSO

L<attributes>, L<Thread::Queue>, L<Thread::Semaphore>, L<Thread::Specific>.

=cut

#
# Methods
#

#
# Exported functions
#
sub async (&) {
    return new Thread $_[0];
}

sub eval {
    return eval { shift->join; };
}

XSLoader::load 'Thread';

1;
