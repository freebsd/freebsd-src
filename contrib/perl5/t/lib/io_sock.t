#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	@INC = '../lib' if -d '../lib';
    }
}

use Config;

BEGIN {
    if (-d "lib" && -f "TEST") {
        if (!$Config{'d_fork'} ||
	    (($Config{'extensions'} !~ /\bSocket\b/ ||
	      $Config{'extensions'} !~ /\bIO\b/) &&
	     !(($^O eq 'VMS') && $Config{d_socket}))) {
	    print "1..0\n";
	    exit 0;
        }
    }
}

$| = 1;
print "1..5\n";

use IO::Socket;

$listen = IO::Socket::INET->new(Listen => 2,
				Proto => 'tcp',
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

    $sock = $listen->accept();
    print "ok 2\n";

    $sock->autoflush(1);
    print $sock->getline();

    print $sock "ok 4\n";

    $sock->close;

    waitpid($pid,0);

    print "ok 5\n";

} elsif(defined $pid) {

    # This can fail if localhost is undefined or the
    # special 'loopback' address 127.0.0.1 is not configured
    # on your system. (/etc/rc.config.d/netconfig on HP-UX.)
    # As a shortcut (not recommended) you could change 'localhost'
    # here to be the name of this machine eg 'myhost.mycompany.com'.

    $sock = IO::Socket::INET->new(PeerPort => $port,
				  Proto => 'tcp',
				  PeerAddr => 'localhost'
				 )
	    or die "$! (maybe your system does not have the 'localhost' address defined)";

    $sock->autoflush(1);

    print $sock "ok 3\n";

    print $sock->getline();

    $sock->close;

    exit;
} else {
 die;
}






