#
# syslog.pl
#
# $Log: syslog.pl,v $
# Revision 1.1.1.1  1993/08/23  21:29:51  nate
# PERL!
#
# Revision 4.0.1.1  92/06/08  13:48:05  lwall
# patch20: new warning for ambiguous use of unary operators
# 
# Revision 4.0  91/03/20  01:26:24  lwall
# 4.0 baseline.
# 
# Revision 3.0.1.4  90/11/10  01:41:11  lwall
# patch38: syslog.pl was referencing an absolute path
# 
# Revision 3.0.1.3  90/10/15  17:42:18  lwall
# patch29: various portability fixes
# 
# Revision 3.0.1.1  90/08/09  03:57:17  lwall
# patch19: Initial revision
# 
# Revision 1.2  90/06/11  18:45:30  18:45:30  root ()
# - Changed 'warn' to 'mail|warning' in test call (to give example of
#   facility specification, and because 'warn' didn't work on HP-UX).
# - Fixed typo in &openlog ("ncons" should be "cons").
# - Added (package-global) $maskpri, and &setlogmask.
# - In &syslog:
#   - put argument test ahead of &connect (why waste cycles?),
#   - allowed facility to be specified in &syslog's first arg (temporarily
#     overrides any $facility set in &openlog), just as in syslog(3C),
#   - do a return 0 when bit for $numpri not set in log mask (see syslog(3C)),
#   - changed $whoami code to use getlogin, getpwuid($<) and 'syslog'
#     (in that order) when $ident is null,
#   - made PID logging consistent with syslog(3C) and subject to $lo_pid only,
#   - fixed typo in "print CONS" statement ($<facility should be <$facility).
#   - changed \n to \r in print CONS (\r is useful, $message already has a \n).
# - Changed &xlate to return -1 for an unknown name, instead of croaking.
# 
#
# tom christiansen <tchrist@convex.com>
# modified to use sockets by Larry Wall <lwall@jpl-devvax.jpl.nasa.gov>
# NOTE: openlog now takes three arguments, just like openlog(3)
#
# call syslog() with a string priority and a list of printf() args
# like syslog(3)
#
#  usage: require 'syslog.pl';
#
#  then (put these all in a script to test function)
#		
#
#	do openlog($program,'cons,pid','user');
#	do syslog('info','this is another test');
#	do syslog('mail|warning','this is a better test: %d', time);
#	do closelog();
#	
#	do syslog('debug','this is the last test');
#	do openlog("$program $$",'ndelay','user');
#	do syslog('notice','fooprogram: this is really done');
#
#	$! = 55;
#	do syslog('info','problem was %m'); # %m == $! in syslog(3)

package syslog;

$host = 'localhost' unless $host;	# set $syslog'host to change

require 'syslog.ph';

$maskpri = &LOG_UPTO(&LOG_DEBUG);

sub main'openlog {
    ($ident, $logopt, $facility) = @_;  # package vars
    $lo_pid = $logopt =~ /\bpid\b/;
    $lo_ndelay = $logopt =~ /\bndelay\b/;
    $lo_cons = $logopt =~ /\bcons\b/;
    $lo_nowait = $logopt =~ /\bnowait\b/;
    &connect if $lo_ndelay;
} 

sub main'closelog {
    $facility = $ident = '';
    &disconnect;
} 

sub main'setlogmask {
    local($oldmask) = $maskpri;
    $maskpri = shift;
    $oldmask;
}
 
sub main'syslog {
    local($priority) = shift;
    local($mask) = shift;
    local($message, $whoami);
    local(@words, $num, $numpri, $numfac, $sum);
    local($facility) = $facility;	# may need to change temporarily.

    die "syslog: expected both priority and mask" unless $mask && $priority;

    @words = split(/\W+/, $priority, 2);# Allow "level" or "level|facility".
    undef $numpri;
    undef $numfac;
    foreach (@words) {
	$num = &xlate($_);		# Translate word to number.
	if (/^kern$/ || $num < 0) {
	    die "syslog: invalid level/facility: $_\n";
	}
	elsif ($num <= &LOG_PRIMASK) {
	    die "syslog: too many levels given: $_\n" if defined($numpri);
	    $numpri = $num;
	    return 0 unless &LOG_MASK($numpri) & $maskpri;
	}
	else {
	    die "syslog: too many facilities given: $_\n" if defined($numfac);
	    $facility = $_;
	    $numfac = $num;
	}
    }

    die "syslog: level must be given\n" unless defined($numpri);

    if (!defined($numfac)) {	# Facility not specified in this call.
	$facility = 'user' unless $facility;
	$numfac = &xlate($facility);
    }

    &connect unless $connected;

    $whoami = $ident;

    if (!$ident && $mask =~ /^(\S.*):\s?(.*)/) {
	$whoami = $1;
	$mask = $2;
    } 

    unless ($whoami) {
	($whoami = getlogin) ||
	    ($whoami = getpwuid($<)) ||
		($whoami = 'syslog');
    }

    $whoami .= "[$$]" if $lo_pid;

    $mask =~ s/%m/$!/g;
    $mask .= "\n" unless $mask =~ /\n$/;
    $message = sprintf ($mask, @_);

    $sum = $numpri + $numfac;
    unless (send(SYSLOG,"<$sum>$whoami: $message",0)) {
	if ($lo_cons) {
	    if ($pid = fork) {
		unless ($lo_nowait) {
		    do {$died = wait;} until $died == $pid || $died < 0;
		}
	    }
	    else {
		open(CONS,">/dev/console");
		print CONS "<$facility.$priority>$whoami: $message\r";
		exit if defined $pid;		# if fork failed, we're parent
		close CONS;
	    }
	}
    }
}

sub xlate {
    local($name) = @_;
    $name =~ y/a-z/A-Z/;
    $name = "LOG_$name" unless $name =~ /^LOG_/;
    $name = "syslog'$name";
    eval(&$name) || -1;
}

sub connect {
    $pat = 'S n C4 x8';

    $af_unix = 1;
    $af_inet = 2;

    $stream = 1;
    $datagram = 2;

    ($name,$aliases,$proto) = getprotobyname('udp');
    $udp = $proto;

    ($name,$aliase,$port,$proto) = getservbyname('syslog','udp');
    $syslog = $port;

    if (chop($myname = `hostname`)) {
	($name,$aliases,$addrtype,$length,@addrs) = gethostbyname($myname);
	die "Can't lookup $myname\n" unless $name;
	@bytes = unpack("C4",$addrs[0]);
    }
    else {
	@bytes = (0,0,0,0);
    }
    $this = pack($pat, $af_inet, 0, @bytes);

    if ($host =~ /^\d+\./) {
	@bytes = split(/\./,$host);
    }
    else {
	($name,$aliases,$addrtype,$length,@addrs) = gethostbyname($host);
	die "Can't lookup $host\n" unless $name;
	@bytes = unpack("C4",$addrs[0]);
    }
    $that = pack($pat,$af_inet,$syslog,@bytes);

    socket(SYSLOG,$af_inet,$datagram,$udp) || die "socket: $!\n";
    bind(SYSLOG,$this) || die "bind: $!\n";
    connect(SYSLOG,$that) || die "connect: $!\n";

    local($old) = select(SYSLOG); $| = 1; select($old);
    $connected = 1;
}

sub disconnect {
    close SYSLOG;
    $connected = 0;
}

1;
