# chat.pl: chat with a server
#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternative: Socket
#
# Based on: V2.01.alpha.7 91/06/16
# Randal L. Schwartz (was <merlyn@stonehenge.com>)
# multihome additions by A.Macpherson@bnr.co.uk
# allow for /dev/pts based systems by Joe Doupnik <JRD@CC.USU.EDU>

package chat;

require 'sys/socket.ph';

if( defined( &main'PF_INET ) ){
	$pf_inet = &main'PF_INET;
	$sock_stream = &main'SOCK_STREAM;
	local($name, $aliases, $proto) = getprotobyname( 'tcp' );
	$tcp_proto = $proto;
}
else {
	# XXX hardwired $PF_INET, $SOCK_STREAM, 'tcp'
	# but who the heck would change these anyway? (:-)
	$pf_inet = 2;
	$sock_stream = 1;
	$tcp_proto = 6;
}


$sockaddr = 'S n a4 x8';
chop($thishost = `hostname`);

# *S = symbol for current I/O, gets assigned *chatsymbol....
$next = "chatsymbol000000"; # next one
$nextpat = "^chatsymbol"; # patterns that match next++, ++, ++, ++


## $handle = &chat'open_port("server.address",$port_number);
## opens a named or numbered TCP server

sub open_port { ## public
	local($server, $port) = @_;

	local($serveraddr,$serverproc);

	# We may be multi-homed, start with 0, fixup once connexion is made
	$thisaddr = "\0\0\0\0" ;
	$thisproc = pack($sockaddr, 2, 0, $thisaddr);

	*S = ++$next;
	if ($server =~ /^(\d+)+\.(\d+)\.(\d+)\.(\d+)$/) {
		$serveraddr = pack('C4', $1, $2, $3, $4);
	} else {
		local(@x) = gethostbyname($server);
		return undef unless @x;
		$serveraddr = $x[4];
	}
	$serverproc = pack($sockaddr, 2, $port, $serveraddr);
	unless (socket(S, $pf_inet, $sock_stream, $tcp_proto)) {
		($!) = ($!, close(S)); # close S while saving $!
		return undef;
	}
	unless (bind(S, $thisproc)) {
		($!) = ($!, close(S)); # close S while saving $!
		return undef;
	}
	unless (connect(S, $serverproc)) {
		($!) = ($!, close(S)); # close S while saving $!
		return undef;
	}
# We opened with the local address set to ANY, at this stage we know
# which interface we are using.  This is critical if our machine is
# multi-homed, with IP forwarding off, so fix-up.
	local($fam,$lport);
	($fam,$lport,$thisaddr) = unpack($sockaddr, getsockname(S));
	$thisproc = pack($sockaddr, 2, 0, $thisaddr);
# end of post-connect fixup
	select((select(S), $| = 1)[0]);
	$next; # return symbol for switcharound
}

## ($host, $port, $handle) = &chat'open_listen([$port_number]);
## opens a TCP port on the current machine, ready to be listened to
## if $port_number is absent or zero, pick a default port number
## process must be uid 0 to listen to a low port number

sub open_listen { ## public

	*S = ++$next;
	local($thisport) = shift || 0;
	local($thisproc_local) = pack($sockaddr, 2, $thisport, $thisaddr);
	local(*NS) = "__" . time;
	unless (socket(NS, $pf_inet, $sock_stream, $tcp_proto)) {
		($!) = ($!, close(NS));
		return undef;
	}
	unless (bind(NS, $thisproc_local)) {
		($!) = ($!, close(NS));
		return undef;
	}
	unless (listen(NS, 1)) {
		($!) = ($!, close(NS));
		return undef;
	}
	select((select(NS), $| = 1)[0]);
	local($family, $port, @myaddr) =
		unpack("S n C C C C x8", getsockname(NS));
	$S{"needs_accept"} = *NS; # so expect will open it
	(@myaddr, $port, $next); # returning this
}

## $handle = &chat'open_proc("command","arg1","arg2",...);
## opens a /bin/sh on a pseudo-tty

sub open_proc { ## public
	local(@cmd) = @_;

	*S = ++$next;
	local(*TTY) = "__TTY" . time;
	local($pty,$tty) = &_getpty(S,TTY);
	die "Cannot find a new pty" unless defined $pty;
	$pid = fork;
	die "Cannot fork: $!" unless defined $pid;
	unless ($pid) {
		close STDIN; close STDOUT; close STDERR;
		setpgrp(0,$$);
		if (open(DEVTTY, "/dev/tty")) {
		    ioctl(DEVTTY,0x20007471,0);		# XXX s/b &TIOCNOTTY
		    close DEVTTY;
		}
		open(STDIN,"<&TTY");
		open(STDOUT,">&TTY");
		open(STDERR,">&STDOUT");
		die "Oops" unless fileno(STDERR) == 2;	# sanity
		close(S);
		exec @cmd;
		die "Cannot exec @cmd: $!";
	}
	close(TTY);
	$next; # return symbol for switcharound
}

# $S is the read-ahead buffer

## $return = &chat'expect([$handle,] $timeout_time,
## 	$pat1, $body1, $pat2, $body2, ... )
## $handle is from previous &chat'open_*().
## $timeout_time is the time (either relative to the current time, or
## absolute, ala time(2)) at which a timeout event occurs.
## $pat1, $pat2, and so on are regexs which are matched against the input
## stream.  If a match is found, the entire matched string is consumed,
## and the corresponding body eval string is evaled.
##
## Each pat is a regular-expression (probably enclosed in single-quotes
## in the invocation).  ^ and $ will work, respecting the current value of $*.
## If pat is 'TIMEOUT', the body is executed if the timeout is exceeded.
## If pat is 'EOF', the body is executed if the process exits before
## the other patterns are seen.
##
## Pats are scanned in the order given, so later pats can contain
## general defaults that won't be examined unless the earlier pats
## have failed.
##
## The result of eval'ing body is returned as the result of
## the invocation.  Recursive invocations are not thought
## through, and may work only accidentally. :-)
##
## undef is returned if either a timeout or an eof occurs and no
## corresponding body has been defined.
## I/O errors of any sort are treated as eof.

$nextsubname = "expectloop000000"; # used for subroutines

sub expect { ## public
	if ($_[0] =~ /$nextpat/) {
		*S = shift;
	}
	local($endtime) = shift;

	local($timeout,$eof) = (1,1);
	local($caller) = caller;
	local($rmask, $nfound, $timeleft, $thisbuf);
	local($cases, $pattern, $action, $subname);
	$endtime += time if $endtime < 600_000_000;

	if (defined $S{"needs_accept"}) { # is it a listen socket?
		local(*NS) = $S{"needs_accept"};
		delete $S{"needs_accept"};
		$S{"needs_close"} = *NS;
		unless(accept(S,NS)) {
			($!) = ($!, close(S), close(NS));
			return undef;
		}
		select((select(S), $| = 1)[0]);
	}

	# now see whether we need to create a new sub:

	unless ($subname = $expect_subname{$caller,@_}) {
		# nope.  make a new one:
		$expect_subname{$caller,@_} = $subname = $nextsubname++;

		$cases .= <<"EDQ"; # header is funny to make everything elsif's
sub $subname {
	LOOP: {
		if (0) { ; }
EDQ
		while (@_) {
			($pattern,$action) = splice(@_,0,2);
			if ($pattern =~ /^eof$/i) {
				$cases .= <<"EDQ";
		elsif (\$eof) {
	 		package $caller;
			$action;
		}
EDQ
				$eof = 0;
			} elsif ($pattern =~ /^timeout$/i) {
			$cases .= <<"EDQ";
		elsif (\$timeout) {
		 	package $caller;
			$action;
		}
EDQ
				$timeout = 0;
			} else {
				$pattern =~ s#/#\\/#g;
			$cases .= <<"EDQ";
		elsif (\$S =~ /$pattern/) {
			\$S = \$';
		 	package $caller;
			$action;
		}
EDQ
			}
		}
		$cases .= <<"EDQ" if $eof;
		elsif (\$eof) {
			undef;
		}
EDQ
		$cases .= <<"EDQ" if $timeout;
		elsif (\$timeout) {
			undef;
		}
EDQ
		$cases .= <<'ESQ';
		else {
			$rmask = "";
			vec($rmask,fileno(S),1) = 1;
			($nfound, $rmask) =
		 		select($rmask, undef, undef, $endtime - time);
			if ($nfound) {
				$nread = sysread(S, $thisbuf, 1024);
				if ($nread > 0) {
					$S .= $thisbuf;
				} else {
					$eof++, redo LOOP; # any error is also eof
				}
			} else {
				$timeout++, redo LOOP; # timeout
			}
			redo LOOP;
		}
	}
}
ESQ
		eval $cases; die "$cases:\n$@" if $@;
	}
	$eof = $timeout = 0;
	do $subname();
}

## &chat'print([$handle,] @data)
## $handle is from previous &chat'open().
## like print $handle @data

sub print { ## public
	if ($_[0] =~ /$nextpat/) {
		*S = shift;
	}

	local $out = join $, , @_;
	syswrite(S, $out, length $out);
	if( $chat'debug ){
		print STDERR "printed:";
		print STDERR @_;
	}
}

## &chat'close([$handle,])
## $handle is from previous &chat'open().
## like close $handle

sub close { ## public
	if ($_[0] =~ /$nextpat/) {
	 	*S = shift;
	}
	close(S);
	if (defined $S{"needs_close"}) { # is it a listen socket?
		local(*NS) = $S{"needs_close"};
		delete $S{"needs_close"};
		close(NS);
	}
}

## @ready_handles = &chat'select($timeout, @handles)
## select()'s the handles with a timeout value of $timeout seconds.
## Returns an array of handles that are ready for I/O.
## Both user handles and chat handles are supported (but beware of
## stdio's buffering for user handles).

sub select { ## public
	local($timeout) = shift;
	local(@handles) = @_;
	local(%handlename) = ();
	local(%ready) = ();
	local($caller) = caller;
	local($rmask) = "";
	for (@handles) {
		if (/$nextpat/o) { # one of ours... see if ready
			local(*SYM) = $_;
			if (length($SYM)) {
				$timeout = 0; # we have a winner
				$ready{$_}++;
			}
			$handlename{fileno($_)} = $_;
		} else {
			$handlename{fileno(/'/ ? $_ : "$caller\'$_")} = $_;
		}
	}
	for (sort keys %handlename) {
		vec($rmask, $_, 1) = 1;
	}
	select($rmask, undef, undef, $timeout);
	for (sort keys %handlename) {
		$ready{$handlename{$_}}++ if vec($rmask,$_,1);
	}
	sort keys %ready;
}

# ($pty,$tty) = $chat'_getpty(PTY,TTY):
# internal procedure to get the next available pty.
# opens pty on handle PTY, and matching tty on handle TTY.
# returns undef if can't find a pty.
# Modify "/dev/pty" to "/dev/pts" for Dell Unix v2.2 (aka SVR4.04). Joe Doupnik.

sub _getpty { ## private
	local($_PTY,$_TTY) = @_;
	$_PTY =~ s/^([^']+)$/(caller)[$[]."'".$1/e;
	$_TTY =~ s/^([^']+)$/(caller)[$[]."'".$1/e;
	local($pty, $tty, $kind);
	if( -e "/dev/pts000" ){		## mods by Joe Doupnik Dec 1992
		$kind = "pts";		## SVR4 Streams
	} else {
		$kind = "pty";		## BSD Clist stuff
	}
	for $bank (112..127) {
		next unless -e sprintf("/dev/$kind%c0", $bank);
		for $unit (48..57) {
			$pty = sprintf("/dev/$kind%c%c", $bank, $unit);
			open($_PTY,"+>$pty") || next;
			select((select($_PTY), $| = 1)[0]);
			($tty = $pty) =~ s/pty/tty/;
			open($_TTY,"+>$tty") || next;
			select((select($_TTY), $| = 1)[0]);
			system "stty nl>$tty";
			return ($pty,$tty);
		}
	}
	undef;
}

1;
