#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
}

use Config;

BEGIN {
    if (-d "lib" && -f "TEST") {
	my $reason;
	if (! $Config{'d_fork'}) {
	    $reason = 'no fork';
	}
	elsif ($Config{'extensions'} !~ /\bSocket\b/) {
	    $reason = 'Socket extension unavailable';
	}
	elsif ($Config{'extensions'} !~ /\bIO\b/) {
	    $reason = 'IO extension unavailable';
	}
	undef $reason if $^O eq 'VMS' and $Config{d_socket};
	if ($reason) {
	    print "1..0 # Skip: $reason\n";
	    exit 0;
        }
    }
}

$| = 1;
print "1..14\n";

use IO::Socket;

$listen = IO::Socket::INET->new(Listen => 2,
				Proto => 'tcp',
				# some systems seem to need as much as 10,
				# so be generous with the timeout
				Timeout => 15,
			       ) or die "$!";

print "ok 1\n";

# Check if can fork with dynamic extensions (bug in CRT):
if ($^O eq 'os2' and
    system "$^X -I../lib -MOpcode -e 'defined fork or die'  > /dev/null 2>&1") {
    print "ok $_ # skipped: broken fork\n" for 2..5;
    exit 0;
}

$port = $listen->sockport;

if($pid = fork()) {

    $sock = $listen->accept() or die "accept failed: $!";
    print "ok 2\n";

    $sock->autoflush(1);
    print $sock->getline();

    print $sock "ok 4\n";

    $sock->close;

    waitpid($pid,0);

    print "ok 5\n";

} elsif(defined $pid) {

    $sock = IO::Socket::INET->new(PeerPort => $port,
				  Proto => 'tcp',
				  PeerAddr => 'localhost'
				 )
         || IO::Socket::INET->new(PeerPort => $port,
				  Proto => 'tcp',
				  PeerAddr => '127.0.0.1'
				 )
	or die "$! (maybe your system does not have a localhost at all, 'localhost' or 127.0.0.1)";

    $sock->autoflush(1);

    print $sock "ok 3\n";

    print $sock->getline();

    $sock->close;

    exit;
} else {
 die;
}

# Test various other ways to create INET sockets that should
# also work.
$listen = IO::Socket::INET->new(Listen => '', Timeout => 15) or die "$!";
$port = $listen->sockport;

if($pid = fork()) {
  SERVER_LOOP:
    while (1) {
       last SERVER_LOOP unless $sock = $listen->accept;
       while (<$sock>) {
           last SERVER_LOOP if /^quit/;
           last if /^done/;
           print;
       }
       $sock = undef;
    }
    $listen->close;
} elsif (defined $pid) {
    # child, try various ways to connect
    $sock = IO::Socket::INET->new("localhost:$port")
         || IO::Socket::INET->new("127.0.0.1:$port");
    if ($sock) {
	print "not " unless $sock->connected;
	print "ok 6\n";
       $sock->print("ok 7\n");
       sleep(1);
       print "ok 8\n";
       $sock->print("ok 9\n");
       $sock->print("done\n");
       $sock->close;
    }
    else {
	print "# $@\n";
	print "not ok 6\n";
	print "not ok 7\n";
	print "not ok 8\n";
	print "not ok 9\n";
    }

    # some machines seem to suffer from a race condition here
    sleep(2);

    $sock = IO::Socket::INET->new("127.0.0.1:$port");
    if ($sock) {
       $sock->print("ok 10\n");
       $sock->print("done\n");
       $sock->close;
    }
    else {
	print "# $@\n";
	print "not ok 10\n";
    }

    # some machines seem to suffer from a race condition here
    sleep(1);

    $sock = IO::Socket->new(Domain => AF_INET,
                            PeerAddr => "localhost:$port")
         || IO::Socket->new(Domain => AF_INET,
                            PeerAddr => "127.0.0.1:$port");
    if ($sock) {
       $sock->print("ok 11\n");
       $sock->print("quit\n");
    }
    $sock = undef;
    sleep(1);
    exit;
} else {
    die;
}

# Then test UDP sockets
$server = IO::Socket->new(Domain => AF_INET,
                          Proto  => 'udp',
                          LocalAddr => 'localhost')
       || IO::Socket->new(Domain => AF_INET,
                          Proto  => 'udp',
                          LocalAddr => '127.0.0.1');
$port = $server->sockport;

if ($^O eq 'mpeix') {
    print("ok 12 # skipped\n")
} else {
    if ($pid = fork()) {
        my $buf;
        $server->recv($buf, 100);
        print $buf;
    } elsif (defined($pid)) {
        #child
        $sock = IO::Socket::INET->new(Proto => 'udp',
                                      PeerAddr => "localhost:$port")
             || IO::Socket::INET->new(Proto => 'udp',
                                      PeerAddr => "127.0.0.1:$port");
        $sock->send("ok 12\n");
        sleep(1);
        $sock->send("ok 12\n");  # send another one to be sure
        exit;
    } else {
        die;
    }
}

print "not " unless $server->blocking;
print "ok 13\n";

$server->blocking(0);
print "not " if $server->blocking;
print "ok 14\n";
