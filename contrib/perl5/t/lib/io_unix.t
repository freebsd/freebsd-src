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
	elsif ($^O eq 'os2') {
	    require IO::Socket;

	    eval {IO::Socket::pack_sockaddr_un('/tmp/foo') || 1}
	      or $@ !~ /not implemented/ or
		$reason = 'compiled without TCP/IP stack v4';
	} elsif ($^O eq 'qnx') {
	    $reason = 'Not implemented';
	}
	undef $reason if $^O eq 'VMS' and $Config{d_socket};
	if ($reason) {
	    print "1..0 # Skip: $reason\n";
	    exit 0;
        }
    }
}

$PATH = "/tmp/sock-$$";

# Test if we can create the file within the tmp directory
if (-e $PATH or not open(TEST, ">$PATH") and $^O ne 'os2') {
    print "1..0 # Skip: cannot open '$PATH' for write\n";
    exit 0;
}
close(TEST);
unlink($PATH) or $^O eq 'os2' or die "Can't unlink $PATH: $!";

# Start testing
$| = 1;
print "1..5\n";

use IO::Socket;

$listen = IO::Socket::UNIX->new(Local=>$PATH, Listen=>0) || die "$!";
print "ok 1\n";

if($pid = fork()) {

    $sock = $listen->accept();
    print "ok 2\n";

    print $sock->getline();

    print $sock "ok 4\n";

    $sock->close;

    waitpid($pid,0);
    unlink($PATH) || $^O eq 'os2' || warn "Can't unlink $PATH: $!";

    print "ok 5\n";

} elsif(defined $pid) {

    $sock = IO::Socket::UNIX->new(Peer => $PATH) or die "$!";

    print $sock "ok 3\n";

    print $sock->getline();

    $sock->close;

    exit;
} else {
 die;
}
