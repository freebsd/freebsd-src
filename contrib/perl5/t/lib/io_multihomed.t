#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
}

use Config;

BEGIN {
    if(-d "lib" && -f "TEST") {
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
	if ($reason) {
	    print "1..0 # Skip: $reason\n";
	    exit 0;
        }
    }
}

$| = 1;

print "1..8\n";


package Multi;
require IO::Socket::INET;
@ISA=qw(IO::Socket::INET);

use Socket qw(inet_aton inet_ntoa unpack_sockaddr_in);

sub _get_addr
{
    my($sock,$addr_str, $multi) = @_;
    #print "_get_addr($sock, $addr_str, $multi)\n";

    print "not " unless $multi;
    print "ok 2\n";

    (
     # private IP-addresses which I hope does not work anywhere :-)
     inet_aton("10.250.230.10"),
     inet_aton("10.250.230.12"),
     inet_aton("127.0.0.1")        # loopback
    )
}

sub connect
{
    my $self = shift;
    if (@_ == 1) {
	my($port, $addr) = unpack_sockaddr_in($_[0]);
	$addr = inet_ntoa($addr);
	#print "connect($self, $port, $addr)\n";
	if($addr eq "10.250.230.10") {
	    print "ok 3\n";
	    return 0;
	}
	if($addr eq "10.250.230.12") {
	    print "ok 4\n";
	    return 0;
	}
    }
    $self->SUPER::connect(@_);
}



package main;

use IO::Socket;

$listen = IO::Socket::INET->new(Listen => 2,
				Proto => 'tcp',
				Timeout => 5,
			       ) or die "$!";

print "ok 1\n";

$port = $listen->sockport;

if($pid = fork()) {

    $sock = $listen->accept() or die "$!";
    print "ok 5\n";

    print $sock->getline();
    print $sock "ok 7\n";

    waitpid($pid,0);

    $sock->close;

    print "ok 8\n";

} elsif(defined $pid) {

    $sock = Multi->new(PeerPort => $port,
		       Proto => 'tcp',
		       PeerAddr => 'localhost',
		       MultiHomed => 1,
		       Timeout => 1,
		      ) or die "$!";

    print $sock "ok 6\n";
    sleep(1); # race condition
    print $sock->getline();

    $sock->close;

    exit;
} else {
    die;
}
