package Thread::Signal;
use Thread qw(async);

=head1 NAME

Thread::Signal - Start a thread which runs signal handlers reliably

=head1 SYNOPSIS

    use Thread::Signal;

    $SIG{HUP} = \&some_handler;

=head1 DESCRIPTION

The C<Thread::Signal> module starts up a special signal handler thread.
All signals to the process are delivered to it and it runs the
associated C<$SIG{FOO}> handlers for them. Without this module,
signals arriving at inopportune moments (such as when perl's internals
are in the middle of updating critical structures) cause the perl
code of the handler to be run unsafely which can cause memory corruption
or worse.

=head1 BUGS

This module changes the semantics of signal handling slightly in that
the signal handler is run separately from the main thread (and in
parallel with it). This means that tricks such as calling C<die> from
a signal handler behave differently (and, in particular, can't be
used to exit directly from a system call).

=cut

if (!init_thread_signals()) {
    require Carp;
    Carp::croak("init_thread_signals failed: $!");
}

async {
    my $sig;
    while ($sig = await_signal()) {
	&$sig();
    }
};

END {
    kill_sighandler_thread();
}

1;
