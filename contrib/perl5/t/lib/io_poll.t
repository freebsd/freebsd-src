#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib';
    }
}

if ($^O eq 'mpeix') {
    print "1..0 # Skip: broken on MPE/iX\n";
    exit 0;
}

select(STDERR); $| = 1;
select(STDOUT); $| = 1;

print "1..9\n";

use IO::Handle;
use IO::Poll qw(/POLL/);

my $poll = new IO::Poll;

my $stdout = \*STDOUT;
my $dupout = IO::Handle->new_from_fd(fileno($stdout),"w");

$poll->mask($stdout => POLLOUT);

print "not "
	unless $poll->mask($stdout) == POLLOUT;
print "ok 1\n";

$poll->mask($dupout => POLLPRI);

print "not "
	unless $poll->mask($dupout) == POLLPRI;
print "ok 2\n";

$poll->poll(0.1);

if ($^O eq 'MSWin32') {
print "ok 3 # skipped, doesn't work on non-socket fds\n";
print "ok 4 # skipped, doesn't work on non-socket fds\n";
}
else {
print "not "
	unless $poll->events($stdout) == POLLOUT;
print "ok 3\n";

print "not "
	if $poll->events($dupout);
print "ok 4\n";
}

my @h = $poll->handles;
print "not "
	unless @h == 2;
print "ok 5\n";

$poll->remove($stdout);

@h = $poll->handles;

print "not "
	unless @h == 1;
print "ok 6\n";

print "not "
	if $poll->mask($stdout);
print "ok 7\n";

$poll->poll(0.1);

print "not "
	if $poll->events($stdout);
print "ok 8\n";

$poll->remove($dupout);
print "not "
    if $poll->handles;
print "ok 9\n";
