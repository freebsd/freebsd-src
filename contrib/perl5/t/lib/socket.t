#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bSocket\b/ && 
        !(($^O eq 'VMS') && $Config{d_socket})) {
	print "1..0\n";
	exit 0;
    }
}
	
use Socket;

print "1..8\n";

if (socket(T,PF_INET,SOCK_STREAM,6)) {
  print "ok 1\n";

  if (connect(T,pack_sockaddr_in(7,inet_aton("localhost")))){
	print "ok 2\n";

	print "# Connected to " .
		inet_ntoa((unpack_sockaddr_in(getpeername(T)))[1])."\n";

	syswrite(T,"hello",5);
	$read = sysread(T,$buff,10);	# Connection may be granted, then closed!
	while ($read > 0 && length($buff) < 5) {
	    # adjust for fact that TCP doesn't guarantee size of reads/writes
	    $read = sysread(T,$buff,10,length($buff));
	}
	print(($read == 0 || $buff eq "hello") ? "ok 3\n" : "not ok 3\n");
  }
  else {
	print "# You're allowed to fail tests 2 and 3 if.\n";
	print "# The echo service has been disabled.\n";
	print "# $!\n";
	print "ok 2\n";
	print "ok 3\n";
  }
}
else {
	print "# $!\n";
	print "not ok 1\n";
}

if( socket(S,PF_INET,SOCK_STREAM,6) ){
  print "ok 4\n";

  if (connect(S,pack_sockaddr_in(7,INADDR_LOOPBACK))){
	print "ok 5\n";

	print "# Connected to " .
		inet_ntoa((unpack_sockaddr_in(getpeername(S)))[1])."\n";

	syswrite(S,"olleh",5);
	$read = sysread(S,$buff,10);	# Connection may be granted, then closed!
	while ($read > 0 && length($buff) < 5) {
	    # adjust for fact that TCP doesn't guarantee size of reads/writes
	    $read = sysread(S,$buff,10,length($buff));
	}
	print(($read == 0 || $buff eq "olleh") ? "ok 6\n" : "not ok 6\n");
  }
  else {
	print "# You're allowed to fail tests 5 and 6 if.\n";
	print "# The echo service has been disabled.\n";
	print "# $!\n";
	print "ok 5\n";
	print "ok 6\n";
  }
}
else {
	print "# $!\n";
	print "not ok 4\n";
}

# warnings
$SIG{__WARN__} = sub {
    ++ $w if $_[0] =~ /^6-ARG sockaddr_in call is deprecated/ ;
} ;
$w = 0 ;
sockaddr_in(1,2,3,4,5,6) ;
print ($w == 1 ? "not ok 7\n" : "ok 7\n") ;
use warnings 'Socket' ;
sockaddr_in(1,2,3,4,5,6) ;
print ($w == 1 ? "ok 8\n" : "not ok 8\n") ;
