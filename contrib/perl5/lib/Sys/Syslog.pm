package Sys::Syslog;
require 5.000;
require Exporter;
use Carp;

@ISA = qw(Exporter);
@EXPORT = qw(openlog closelog setlogmask syslog);
@EXPORT_OK = qw(setlogsock);

use Socket;
use Sys::Hostname;

# adapted from syslog.pl
#
# Tom Christiansen <tchrist@convex.com>
# modified to use sockets by Larry Wall <lwall@jpl-devvax.jpl.nasa.gov>
# NOTE: openlog now takes three arguments, just like openlog(3)
# Modified to add UNIX domain sockets by Sean Robinson <robinson_s@sc.maricopa.edu>
#  with support from Tim Bunce <Tim.Bunce@ig.co.uk> and the perl5-porters mailing list

# Todo: enable connect to try all three types before failing (auto setlogsock)?

=head1 NAME

Sys::Syslog, openlog, closelog, setlogmask, syslog - Perl interface to the UNIX syslog(3) calls

=head1 SYNOPSIS

    use Sys::Syslog;                          # all except setlogsock, or:
    use Sys::Syslog qw(:DEFAULT setlogsock);  # default set, plus setlogsock

    setlogsock $sock_type;
    openlog $ident, $logopt, $facility;
    syslog $priority, $format, @args;
    $oldmask = setlogmask $mask_priority;
    closelog;

=head1 DESCRIPTION

Sys::Syslog is an interface to the UNIX C<syslog(3)> program.
Call C<syslog()> with a string priority and a list of C<printf()> args
just like C<syslog(3)>.

Syslog provides the functions:

=over

=item openlog $ident, $logopt, $facility

I<$ident> is prepended to every message.
I<$logopt> contains zero or more of the words I<pid>, I<ndelay>, I<cons>, I<nowait>.
I<$facility> specifies the part of the system

=item syslog $priority, $format, @args

If I<$priority> permits, logs I<($format, @args)>
printed as by C<printf(3V)>, with the addition that I<%m>
is replaced with C<"$!"> (the latest error message).

=item setlogmask $mask_priority

Sets log mask I<$mask_priority> and returns the old mask.

=item setlogsock $sock_type (added in 5.004_02)

Sets the socket type to be used for the next call to
C<openlog()> or C<syslog()> and returns TRUE on success,
undef on failure.

A value of 'unix' will connect to the UNIX domain socket returned by
C<_PATH_LOG> in F<syslog.ph>.  A value of 'inet' will connect to an
INET socket returned by getservbyname().  Any other value croaks.

The default is for the INET socket to be used.

=item closelog

Closes the log file.

=back

Note that C<openlog> now takes three arguments, just like C<openlog(3)>.

=head1 EXAMPLES

    openlog($program, 'cons,pid', 'user');
    syslog('info', 'this is another test');
    syslog('mail|warning', 'this is a better test: %d', time);
    closelog();

    syslog('debug', 'this is the last test');

    setlogsock('unix');
    openlog("$program $$", 'ndelay', 'user');
    syslog('notice', 'fooprogram: this is really done');

    setlogsock('inet');
    $! = 55;
    syslog('info', 'problem was %m'); # %m == $! in syslog(3)

=head1 DEPENDENCIES

B<Sys::Syslog> needs F<syslog.ph>, which can be created with C<h2ph>.

=head1 SEE ALSO

L<syslog(3)>

=head1 AUTHOR

Tom Christiansen E<lt>F<tchrist@perl.com>E<gt> and Larry Wall E<lt>F<larry@wall.org>E<gt>.
UNIX domain sockets added by Sean Robinson E<lt>F<robinson_s@sc.maricopa.edu>E<gt>
with support from Tim Bunce <Tim.Bunce@ig.co.uk> and the perl5-porters mailing list.

=cut

require 'syslog.ph';

$maskpri = &LOG_UPTO(&LOG_DEBUG);

sub openlog {
    ($ident, $logopt, $facility) = @_;  # package vars
    $lo_pid = $logopt =~ /\bpid\b/;
    $lo_ndelay = $logopt =~ /\bndelay\b/;
    $lo_cons = $logopt =~ /\bcons\b/;
    $lo_nowait = $logopt =~ /\bnowait\b/;
    &connect if $lo_ndelay;
} 

sub closelog {
    $facility = $ident = '';
    &disconnect;
} 

sub setlogmask {
    local($oldmask) = $maskpri;
    $maskpri = shift;
    $oldmask;
}
 
sub setlogsock {
    local($setsock) = shift;
    &disconnect if $connected;
    if (lc($setsock) eq 'unix') {
        if (defined &_PATH_LOG) {
            $sock_type = 1;
        } else {
            return undef;
        }
    } elsif (lc($setsock) eq 'inet') {
        if (getservbyname('syslog','udp')) {
            undef($sock_type);
        } else {
            return undef;
        }
    } else {
        croak "Invalid argument passed to setlogsock; must be 'unix' or 'inet'";
    }
    return 1;
}

sub syslog {
    local($priority) = shift;
    local($mask) = shift;
    local($message, $whoami);
    local(@words, $num, $numpri, $numfac, $sum);
    local($facility) = $facility;	# may need to change temporarily.

    croak "syslog: expected both priority and mask" unless $mask && $priority;

    @words = split(/\W+/, $priority, 2);# Allow "level" or "level|facility".
    undef $numpri;
    undef $numfac;
    foreach (@words) {
	$num = &xlate($_);		# Translate word to number.
	if (/^kern$/ || $num < 0) {
	    croak "syslog: invalid level/facility: $_";
	}
	elsif ($num <= &LOG_PRIMASK) {
	    croak "syslog: too many levels given: $_" if defined($numpri);
	    $numpri = $num;
	    return 0 unless &LOG_MASK($numpri) & $maskpri;
	}
	else {
	    croak "syslog: too many facilities given: $_" if defined($numfac);
	    $facility = $_;
	    $numfac = $num;
	}
    }

    croak "syslog: level must be given" unless defined($numpri);

    if (!defined($numfac)) {	# Facility not specified in this call.
	$facility = 'user' unless $facility;
	$numfac = &xlate($facility);
    }

    &connect unless $connected;

    $whoami = $ident;

    if (!$whoami && $mask =~ /^(\S.*?):\s?(.*)/) {
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
    unless (send(SYSLOG,"<$sum>$whoami: $message\0",0)) {
	if ($lo_cons) {
	    if ($pid = fork) {
		unless ($lo_nowait) {
		    $died = waitpid($pid, 0);
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
    $name = uc $name;
    $name = "LOG_$name" unless $name =~ /^LOG_/;
    $name = "Sys::Syslog::$name";
    defined &$name ? &$name : -1;
}

sub connect {
    unless ($host) {
	require Sys::Hostname;
	my($host_uniq) = Sys::Hostname::hostname();
	($host) = $host_uniq =~ /([A-Za-z0-9_.-]+)/; # allow FQDN (inc _)
    }
    unless ( $sock_type ) {
        my $udp = getprotobyname('udp');
        my $syslog = getservbyname('syslog','udp');
        my $this = sockaddr_in($syslog, INADDR_ANY);
        my $that = sockaddr_in($syslog, inet_aton($host) || croak "Can't lookup $host");
        socket(SYSLOG,AF_INET,SOCK_DGRAM,$udp)           || croak "socket: $!";
        connect(SYSLOG,$that)                            || croak "connect: $!";
    } else {
        my $syslog = &_PATH_LOG                          || croak "_PATH_LOG not found in syslog.ph";
        my $that = sockaddr_un($syslog)                  || croak "Can't locate $syslog";
        socket(SYSLOG,AF_UNIX,SOCK_STREAM,0)             || croak "socket: $!";
        if (!connect(SYSLOG,$that)) {
            socket(SYSLOG,AF_UNIX,SOCK_DGRAM,0)          || croak "socket: $!";
            connect(SYSLOG,$that) || croak "connect: $! (SOCK_DGRAM after trying SOCK_STREAM)";
        }
    }
    local($old) = select(SYSLOG); $| = 1; select($old);
    $connected = 1;
}

sub disconnect {
    close SYSLOG;
    $connected = 0;
}

1;
