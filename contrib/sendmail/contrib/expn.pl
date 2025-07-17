#!/usr/bin/perl
'di ';
'ds 00 \\"';
'ig 00 ';
#
#       THIS PROGRAM IS ITS OWN MANUAL PAGE.  INSTALL IN man & bin.
#

use 5.001;
use IO::Socket;
use Fcntl;

# system requirements:
# 	must have 'nslookup' and 'hostname' programs.

# $OrigHeader: /home/muir/bin/RCS/expn,v 3.11 1997/09/10 08:14:02 muir Exp muir $

# TODO:
#	less magic should apply to command-line addresses
#	less magic should apply to local addresses
#	add magic to deal with cross-domain cnames
#	disconnect & reconnect after 25 commands to the same sendmail 8.8.* host

# Checklist: (hard addresses)
#	250 Kimmo Suominen <"|/usr/local/mh/lib/slocal -user kim"@grendel.tac.nyc.ny.us>
#	harry@hofmann.cs.Berkeley.EDU -> harry@tenet (.berkeley.edu)  [dead]
#	bks@cs.berkeley.edu -> shiva.CS (.berkeley.edu)		      [dead]
#	dan@tc.cornell.edu -> brown@tiberius (.tc.cornell.edu)

#############################################################################
#
#  Copyright (c) 1993 David Muir Sharnoff
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. All advertising materials mentioning features or use of this software
#     must display the following acknowledgement:
#       This product includes software developed by the David Muir Sharnoff.
#  4. The name of David Sharnoff may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE DAVID MUIR SHARNOFF ``AS IS'' AND
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL DAVID MUIR SHARNOFF BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#  SUCH DAMAGE.
#
# This copyright notice derrived from material copyrighted by the Regents
# of the University of California.
#
# Contributions accepted.
#
#############################################################################

# overall structure:
#	in an effort to not trace each address individually, but rather
#	ask each server in turn a whole bunch of questions, addresses to
#	be expanded are queued up.
#
#	This means that all accounting w.r.t. an address must be stored in
#	various arrays.  Generally these arrays are indexed by the
#	string "$addr *** $server" where $addr is the address to be
#	expanded "foo" or maybe "foo@bar" and $server is the hostname
#	of the SMTP server to contact.
#

# important global variables:
#
# @hosts : list of servers still to be contacted
# $server : name of the current we are currently looking at
# @users = $users{@hosts[0]} : addresses to expand at this server
# $u = $users[0] : the current address being expanded
# $names{"$users[0] *** $server"} : the 'name' associated with the address
# $mxbacktrace{"$users[0] *** $server"} : record of mx expansion
# $mx_secondary{$server} : other mx relays at the same priority
# $domainify_fallback{"$users[0] *** $server"} : alternative names to try 
#	instead of $server if $server doesn't work
# $temporary_redirect{"$users[0] *** $server"} : when trying alternates,
#	temporarily channel all tries along current path
# $giveup{$server} : do not bother expanding addresses at $server
# $verbose : -v
# $watch : -w
# $vw : -v or -w
# $debug : -d
# $valid : -a
# $levels : -1
# $S : the socket connection to $server

$have_nslookup = 1;	# we have the nslookup program
$port = 'smtp';
$av0 = $0;
$ENV{'PATH'} .= ":/usr/etc" unless $ENV{'PATH'} =~ m,/usr/etc,;
$ENV{'PATH'} .= ":/usr/ucb" unless $ENV{'PATH'} =~ m,/usr/ucb,;
select(STDERR);

$0 = "$av0 - running hostname";
chop($name = `hostname || uname -n`);

$0 = "$av0 - lookup host FQDN and IP addr";
($hostname,$aliases,$type,$len,$thisaddr) = gethostbyname($name);

$0 = "$av0 - parsing args";
$usage = "Usage: $av0 [-1avwd] user[\@host] [user2[host2] ...]";
for $a (@ARGV) {
	die $usage if $a eq "-";
	while ($a =~ s/^(-.*)([1avwd])/$1/) {
		eval '$'."flag_$2 += 1";
	}
	next if $a eq "-";
	die $usage if $a =~ /^-/;
	&expn(&parse($a,$hostname,undef,1));
}
$verbose = $flag_v;
$watch = $flag_w;
$vw = $flag_v + $flag_w;
$debug = $flag_d;
$valid = $flag_a;
$levels = $flag_1;

die $usage unless @hosts;
if ($valid) {
	if ($valid == 1) {
		$validRequirement = 0.8;
	} elsif ($valid == 2) {
		$validRequirement = 1.0;
	} elsif ($valid == 3) {
		$validRequirement = 0.9;
	} else {
		$validRequirement = (1 - (1/($valid-3)));
		print "validRequirement = $validRequirement\n" if $debug;
	}
}

HOST:
while (@hosts) {
	$server = shift(@hosts);
	@users = split(' ',$users{$server});
	delete $users{$server};

	# is this server already known to be bad?
	$0 = "$av0 - looking up $server";
	if ($giveup{$server}) {
		&giveup('mx domainify',$giveup{$server});
		next;
	}

	# do we already have an mx record for this host?
	next HOST if &mxredirect($server,*users);

	# look it up, or try for an mx.
	$0 = "$av0 - gethostbyname($server)";

	($name,$aliases,$type,$len,$thataddr) = gethostbyname($server);
	# if we can't get an A record, try for an MX record.
	unless($thataddr) {
		&mxlookup(1,$server,"$server: could not resolve name",*users);
		next HOST;
	}
				
	# get a connection, or look for an mx
	$0 = "$av0 - socket to $server";

	$S = new IO::Socket::INET (
		'PeerAddr' => $server,
		'PeerPort' => $port,
		'Proto' => 'tcp');

	if (! $S || ($debug == 10 && $server =~ /relay\d.UU.NET$/i)) {
		$0 = "$av0 - $server: could not connect: $!\n";
		$emsg = $!;
		unless (&mxlookup(0,$server,"$server: could not connect: $!",*users)) {
			&giveup('mx',"$server: Could not connect: $emsg");
		}
		next HOST;
	}
	$S->autoflush(1);

	# read the greeting
	$0 = "$av0 - talking to $server";
	&alarm("greeting with $server",'');
	while(<$S>) {
		alarm(0);
		print if $watch;
		if (/^(\d+)([- ])/) {
			if ($1 != 220) {
				$0 = "$av0 - bad numeric response from $server";
				&alarm("giving up after bad response from $server",'');
				&read_response($2,$watch);
				alarm(0);
				print STDERR "$server: NOT 220 greeting: $_"
					if ($debug || $vw);
				if (&mxlookup(0,$server,"$server: did not respond with a 220 greeting",*users)) {
					close($S);
					next HOST;
				}
			}
			last if ($2 eq " ");
		} else {
			$0 = "$av0 - bad response from $server";
			print STDERR "$server: NOT 220 greeting: $_"
				if ($debug || $vw);
			unless (&mxlookup(0,$server,"$server: did not respond with SMTP codes",*users)) {
				&giveup('',"$server: did not talk SMTP");
			}
			close($S);
			next HOST;
		}
		&alarm("greeting with $server",'');
	}
	alarm(0);
	
	# if this causes problems, remove it
	$0 = "$av0 - sending helo to $server";
	&alarm("sending helo to $server","");
	&ps("helo $hostname");
	while(<$S>) {
		print if $watch;
		last if /^\d+ /;
	}
	alarm(0);

	# try the users, one by one
	USER:
	while(@users) {
		$u = shift(@users);
		$0 = "$av0 - expanding $u [\@$server]";

		# do we already have a name for this user?
		$oldname = $names{"$u *** $server"};

		print &compact($u,$server)." ->\n" if ($verbose && ! $valid);
		if ($valid) {
			#
			# when running with -a, we delay taking any action 
			# on the results of our query until we have looked
			# at the complete output.  @toFinal stores expansions
			# that will be final if we take them.  @toExpn stores
			# expnansions that are not final.  @isValid keeps
			# track of our ability to send mail to each of the
			# expansions.
			#
			@isValid = ();
			@toFinal = ();
			@toExpn = ();
		}

#		($ecode,@expansion) = &expn_vrfy($u,$server);
		(@foo) = &expn_vrfy($u,$server);
		($ecode,@expansion) = @foo;
		if ($ecode) {
			&giveup('',$ecode,$u);
			last USER;
		}

		for $s (@expansion) {
			$s =~ s/[\n\r]//g;
			$0 = "$av0 - parsing $server: $s";

			$skipwatch = $watch;

			if ($s =~ /^[25]51([- ]).*<(.+)>/) {
				print "$s" if $watch;
				print "(pretending 250$1<$2>)" if ($debug && $watch);
				print "\n" if $watch;
				$s = "250$1<$2>";
				$skipwatch = 0;
			}

			if ($s =~ /^250([- ])(.+)/) {
				print "$s\n" if $skipwatch;
				($done,$addr) = ($1,$2);
				($newhost, $newaddr, $newname) =  &parse($addr,$server,$oldname, $#expansion == 0);
				print "($newhost, $newaddr, $newname) = &parse($addr, $server, $oldname)\n" if $debug;
				if (! $newhost) {
					# no expansion is possible w/o a new server to call
					if ($valid) {
						push(@isValid, &validAddr($newaddr));
						push(@toFinal,$newaddr,$server,$newname);
					} else {
						&verbose(&final($newaddr,$server,$newname));
					}
				} else {
					$newmxhost = &mx($newhost,$newaddr);
					print "$newmxhost = &mx($newhost)\n" 
						if ($debug && $newhost ne $newmxhost);
					$0 = "$av0 - parsing $newaddr [@$newmxhost]";
					print "levels = $levels, level{$u *** $server} = ".$level{"$u *** $server"}."\n" if ($debug > 1);
					# If the new server is the current one, 
					# it would have expanded things for us
					# if it could have.  Mx records must be
					# followed to compare server names.
					# We are also done if the recursion
					# count has been exceeded.
					if (&trhost($newmxhost) eq &trhost($server) || ($levels && $level{"$u *** $server"} >= $levels)) {
						if ($valid) {
							push(@isValid, &validAddr($newaddr));
							push(@toFinal,$newaddr,$newmxhost,$newname);
						} else {
							&verbose(&final($newaddr,$newmxhost,$newname));
						}
					} else {
						# more work to do...
						if ($valid) {
							push(@isValid, &validAddr($newaddr));
							push(@toExpn,$newmxhost,$newaddr,$newname,$level{"$u *** $server"});
						} else {
							&verbose(&expn($newmxhost,$newaddr,$newname,$level{"$u *** $server"}));
						}
					}
				}
				last if ($done eq " ");
				next;
			}
			# 550 is a known code...  Should the be
			# included in -a output?  Might be a bug
			# here.  Does it matter?  Can assume that
			# there won't be UNKNOWN USER responses 
			# mixed with valid users?
			if ($s =~ /^(550)([- ])/) {
				if ($valid) {
					print STDERR "\@$server:$u ($oldname) USER UNKNOWN\n";
				} else {
					&verbose(&final($u,$server,$oldname,"USER UNKNOWN"));
				}
				last if ($2 eq " ");
				next;
			} 
			# 553 is a known code...  
			if ($s =~ /^(553)([- ])/) {
				if ($valid) {
					print STDERR "\@$server:$u ($oldname) USER AMBIGUOUS\n";
				} else {
					&verbose(&final($u,$server,$oldname,"USER AMBIGUOUS"));
				}
				last if ($2 eq " ");
				next;
			} 
			# 252 is a known code...  
			if ($s =~ /^(252)([- ])/) {
				if ($valid) {
					print STDERR "\@$server:$u ($oldname) REFUSED TO VRFY\n";
				} else {
					&verbose(&final($u,$server,$oldname,"REFUSED TO VRFY"));
				}
				last if ($2 eq " ");
				next;
			} 
			&giveup('',"$server: did not grok '$s'",$u);
			last USER;
		}

		if ($valid) {
			#
			# now we decide if we are going to take these
			# expansions or roll them back.
			#
			$avgValid = &average(@isValid);
			print "avgValid = $avgValid\n" if $debug;
			if ($avgValid >= $validRequirement) {
				print &compact($u,$server)." ->\n" if $verbose;
				while (@toExpn) {
					&verbose(&expn(splice(@toExpn,0,4)));
				}
				while (@toFinal) {
					&verbose(&final(splice(@toFinal,0,3)));
				}
			} else {
				print "Tossing some valid to avoid invalid ".&compact($u,$server)."\n" if ($avgValid > 0.0 && ($vw || $debug));
				print &compact($u,$server)." ->\n" if $verbose;
				&verbose(&final($u,$server,$newname));
			}
		}
	}

	&alarm("sending 'quit' to $server",'');
	$0 = "$av0 - sending 'quit' to $server";
	&ps("quit");
	while(<$S>) {
		print if $watch;
		last if /^\d+ /;
	}
	close($S);
	alarm(0);
}

$0 = "$av0 - printing final results";
print "----------\n" if $vw;
select(STDOUT);
for $f (sort @final) {
	print "$f\n";
}
unlink("/tmp/expn$$");
exit(0);


# abandon all attempts deliver to $server
# register the current addresses as the final ones
sub giveup
{
	local($redirect_okay,$reason,$user) = @_;
	local($us,@so,$nh,@remaining_users);
	local($pk,$file,$line);
	($pk, $file, $line) = caller;

	$0 = "$av0 - giving up on $server: $reason";
	#
	# add back a user if we gave up in the middle
	#
	push(@users,$user) if $user;
	#
	# don't bother with this system anymore
	#
	unless ($giveup{$server}) {
		$giveup{$server} = $reason;
		print STDERR "$reason\n";
	}
	print "Giveup at $file:$line!!! redirect okay = $redirect_okay; $reason\n" if $debug;
	#
	# Wait!
	# Before giving up, see if there is a chance that
	# there is another host to redirect to!
	# (Kids, don't do this at home!  Hacking is a dangerous
	# crime and you could end up behind bars.)
	#
	for $u (@users) {
		if ($redirect_okay =~ /\bmx\b/) {
			next if &try_fallback('mx',$u,*server,
				*mx_secondary,
				*already_mx_fellback);
		}
		if ($redirect_okay =~ /\bdomainify\b/) {
			next if &try_fallback('domainify',$u,*server,
				*domainify_fallback,
				*already_domainify_fellback);
		}
		push(@remaining_users,$u);
	}
	@users = @remaining_users;
	for $u (@users) {
		print &compact($u,$server)." ->\n" if ($verbose && $valid && $u);
		&verbose(&final($u,$server,$names{"$u *** $server"},$reason));
	}
}
#
# This routine is used only within &giveup.  It checks to
# see if we really have to giveup or if there is a second
# chance because we did something before that can be 
# backtracked.
#
# %fallback{"$user *** $host"} tracks what is able to fallback
# %fellback{"$user *** $host"} tracks what has fallen back
#
# If there is a valid backtrack, then queue up the new possibility
#
sub try_fallback
{
	local($method,$user,*host,*fall_table,*fellback) = @_;
	local($us,$fallhost,$oldhost,$ft,$i);

	if ($debug > 8) {
		print "Fallback table $method:\n";
		for $i (sort keys %fall_table) {
			print "\t'$i'\t\t'$fall_table{$i}'\n";
		}
		print "Fellback table $method:\n";
		for $i (sort keys %fellback) {
			print "\t'$i'\t\t'$fellback{$i}'\n";
		}
		print "U: $user H: $host\n";
	}
	
	$us = "$user *** $host";
	if (defined $fellback{$us}) {
		#
		# Undo a previous fallback so that we can try again
		# Nested fallbacks are avoided because they could
		# lead to infinite loops
		#
		$fallhost = $fellback{$us};
		print "Already $method fell back from $us -> \n" if $debug;
		$us = "$user *** $fallhost";
		$oldhost = $fallhost;
	} elsif (($method eq 'mx') && (defined $mxbacktrace{$us}) && (defined $mx_secondary{$mxbacktrace{$us}})) {
		print "Fallback an MX expansion $us -> \n" if $debug;
		$oldhost = $mxbacktrace{$us};
	} else {
		print "Oldhost($host, $us) = " if $debug;
		$oldhost = $host;
	}
	print "$oldhost\n" if $debug;
	if (((defined $fall_table{$us}) && ($ft = $us)) || ((defined $fall_table{$oldhost}) && ($ft = $oldhost))) {
		print "$method Fallback = ".$fall_table{$ft}."\n" if $debug;
		local(@so,$newhost);
		@so = split(' ',$fall_table{$ft});
		$newhost = shift(@so);
		print "Falling back ($method) $us -> $newhost (from $oldhost)\n" if $debug;
		if ($method eq 'mx') {
			if (! defined ($mxbacktrace{"$user *** $newhost"})) {
				if (defined $mxbacktrace{"$user *** $oldhost"}) {
					print "resetting oldhost $oldhost to the original: " if $debug;
					$oldhost = $mxbacktrace{"$user *** $oldhost"};
					print "$oldhost\n" if $debug;
				}
				$mxbacktrace{"$user *** $newhost"} = $oldhost;
				print "mxbacktrace $user *** $newhost -> $oldhost\n" if $debug;
			}
			$mx{&trhost($oldhost)} = $newhost;
		} else {
			$temporary_redirect{$us} = $newhost;
		}
		if (@so) {
			print "Can still $method  $us: @so\n" if $debug;
			$fall_table{$ft} = join(' ',@so);
		} else {
			print "No more fallbacks for $us\n" if $debug;
			delete $fall_table{$ft};
		}
		if (defined $create_host_backtrack{$us}) {
			$create_host_backtrack{"$user *** $newhost"} 
				= $create_host_backtrack{$us};
		}
		$fellback{"$user *** $newhost"} = $oldhost;
		&expn($newhost,$user,$names{$us},$level{$us});
		return 1;
	}
	delete $temporary_redirect{$us};
	$host = $oldhost;
	return 0;
}
# return 1 if you could send mail to the address as is.
sub validAddr
{
	local($addr) = @_;
	$res = &do_validAddr($addr);
	print "validAddr($addr) = $res\n" if $debug;
	$res;
}
sub do_validAddr
{
	local($addr) = @_;
	local($urx) = "[-A-Za-z_.0-9+]+";

	# \u
	return 0 if ($addr =~ /^\\/);
	# ?@h
	return 1 if ($addr =~ /.\@$urx$/);
	# @h:?
	return 1 if ($addr =~ /^\@$urx\:./);
	# h!u
	return 1 if ($addr =~ /^$urx!./);
	# u
	return 1 if ($addr =~ /^$urx$/);
	# ?
	print "validAddr($addr) = ???\n" if $debug;
	return 0;
}
# Some systems use expn and vrfy interchangeably.  Some only
# implement one or the other.  Some check expn against mailing
# lists and vrfy against users.  It doesn't appear to be
# consistent.
#
# So, what do we do?  We try everything!
#
#
# Ranking of result codes: good: 250, 251/551, 252, 550, anything else
#
# Ranking of inputs: best: user@host.domain, okay: user
#
# Return value: $error_string, @responses_from_server
sub expn_vrfy
{
	local($u,$server) = @_;
	local(@c) = ('expn', 'vrfy');
	local(@try_u) = $u;
	local(@ret,$code);

	if (($u =~ /(.+)@(.+)/) && (&trhost($2) eq &trhost($server))) {
		push(@try_u,$1);
	}

	TRY:
	for $c (@c) {
		for $try_u (@try_u) {
			&alarm("${c}'ing $try_u on $server",'',$u);
			&ps("$c $try_u");
			alarm(0);
			$s = <$S>;
			if ($s eq '') {
				return "$server: lost connection";
			}
			if ($s !~ /^(\d+)([- ])/) {
				return "$server: garbled reply to '$c $try_u'";
			}
			if ($1 == 250) {
				$code = 250;
				@ret = ("",$s);
				push(@ret,&read_response($2,$debug));
				return (@ret);
			} 
			if ($1 == 551 || $1 == 251) {
				$code = $1;
				@ret = ("",$s);
				push(@ret,&read_response($2,$debug));
				next;
			}
			if ($1 == 252 && ($code == 0 || $code == 550)) {
				$code = 252;
				@ret = ("",$s);
				push(@ret,&read_response($2,$watch));
				next;
			}
			if ($1 == 550 && $code == 0) {
				$code = 550;
				@ret = ("",$s);
				push(@ret,&read_response($2,$watch));
				next;
			}
			&read_response($2,$watch);
		}
	}
	return "$server: expn/vrfy not implemented" unless @ret;
	return @ret;
}
# sometimes the old parse routine (now parse2) didn't
# reject funky addresses. 
sub parse
{
	local($oldaddr,$server,$oldname,$one_to_one) = @_;
	local($newhost, $newaddr, $newname, $um) =  &parse2($oldaddr,$server,$oldname,$one_to_one);
	if ($newaddr =~ m,^["/],) {
		return (undef, $oldaddr, $newname) if $valid;
		return (undef, $um, $newname);
	}
	return ($newhost, $newaddr, $newname);
}

# returns ($new_smtp_server,$new_address,$new_name)
# given a response from a SMTP server ($newaddr), the 
# current host ($server), the old "name" and a flag that
# indicates if it is being called during the initial 
# command line parsing ($parsing_args)
sub parse2
{
	local($newaddr,$context_host,$old_name,$parsing_args) = @_;
	local(@names) = $old_name;
	local($urx) = "[-A-Za-z_.0-9+]+";
	local($unmangle);

	#
	# first, separate out the address part.
	#

	#
	# [NAME] <ADDR [(NAME)]>
	# [NAME] <[(NAME)] ADDR
	# ADDR [(NAME)]
	# (NAME) ADDR
	# [(NAME)] <ADDR>
	#
	if ($newaddr =~ /^\<(.*)\>$/) {
		print "<A:$1>\n" if $debug;
		($newaddr) = &trim($1);
		print "na = $newaddr\n" if $debug;
	}
	if ($newaddr =~ /^([^\<\>]*)\<([^\<\>]*)\>([^\<\>]*)$/) {
		# address has a < > pair in it.
		print "N:$1 <A:$2> N:$3\n" if $debug;
		($newaddr) = &trim($2);
		unshift(@names, &trim($3,$1));
		print "na = $newaddr\n" if $debug;
	}
	if ($newaddr =~ /^([^\(\)]*)\(([^\(\)]*)\)([^\(\)]*)$/) {
		# address has a ( ) pair in it.
		print "A:$1 (N:$2) A:$3\n" if $debug;
		unshift(@names,&trim($2));
		local($f,$l) = (&trim($1),&trim($3));
		if (($f && $l) || !($f || $l)) {
			# address looks like:
			# foo (bar) baz  or (bar)
			# not allowed!
			print STDERR "Could not parse $newaddr\n" if $vw;
			return(undef,$newaddr,&firstname(@names));
		}
		$newaddr = $f if $f;
		$newaddr = $l if $l;
		print "newaddr now = $newaddr\n" if $debug;
	}
	#
	# @foo:bar
	# j%k@l
	# a@b
	# b!a
	# a
	#
	$unmangle = $newaddr;
	if ($newaddr =~ /^\@($urx)\:(.+)$/) {
		print "(\@:)" if $debug;
		# this is a bit of a cheat, but it seems necessary
		return (&domainify($1,$context_host,$2),$2,&firstname(@names),$unmangle);
	}
	if ($newaddr =~ /^(.+)\@($urx)$/) {
		print "(\@)" if $debug;
		return (&domainify($2,$context_host,$newaddr),$newaddr,&firstname(@names),$unmangle);
	}
	if ($parsing_args) {
		if ($newaddr =~ /^($urx)\!(.+)$/) {
			return (&domainify($1,$context_host,$newaddr),$newaddr,&firstname(@names),$unmangle);
		}
		if ($newaddr =~ /^($urx)$/) {
			return ($context_host,$newaddr,&firstname(@names),$unmangle);
		}
		print STDERR "Could not parse $newaddr\n";
	}
	print "(?)" if $debug;
	return(undef,$newaddr,&firstname(@names),$unmangle);
}
# return $u (@$server) unless $u includes reference to $server
sub compact
{
	local($u, $server) = @_;
	local($se) = $server;
	local($sp);
	$se =~ s/(\W)/\\$1/g;
	$sp = " (\@$server)";
	if ($u !~ /$se/i) {
		return "$u$sp";
	}
	return $u;
}
# remove empty (spaces don't count) members from an array
sub trim
{
	local(@v) = @_;
	local($v,@r);
	for $v (@v) {
		$v =~ s/^\s+//;
		$v =~ s/\s+$//;
		push(@r,$v) if ($v =~ /\S/);
	}
	return(@r);
}
# using the host part of an address, and the server name, add the
# servers' domain to the address if it doesn't already have a 
# domain.  Since this sometimes fails, save a back reference so
# it can be unrolled.
sub domainify
{
	local($host,$domain_host,$u) = @_;
	local($domain,$newhost);

	# cut of trailing dots 
	$host =~ s/\.$//;
	$domain_host =~ s/\.$//;

	if ($domain_host !~ /\./) {
		#
		# domain host isn't, keep $host whatever it is
		#
		print "domainify($host,$domain_host) = $host\n" if $debug;
		return $host;
	}

	# 
	# There are several weird situtations that need to be 
	# accounted for.  They have to do with domain relay hosts.
	#
	# Examples: 
	#	host		server		"right answer"
	#	
	#	shiva.cs	cs.berkeley.edu	shiva.cs.berkeley.edu
	#	shiva		cs.berkeley.edu	shiva.cs.berekley.edu
	#	cumulus		reed.edu	@reed.edu:cumulus.uucp
	# 	tiberius	tc.cornell.edu	tiberius.tc.cornell.edu
	#
	# The first try must always be to cut the domain part out of 
	# the server and tack it onto the host.
	#
	# A reasonable second try is to tack the whole server part onto
	# the host and for each possible repeated element, eliminate 
	# just that part.
	#
	# These extra "guesses" get put into the %domainify_fallback
	# array.  They will be used to give addresses a second chance
	# in the &giveup routine
	#

	local(%fallback);

	local($long); 
	$long = "$host $domain_host";
	$long =~ tr/A-Z/a-z/;
	print "long = $long\n" if $debug;
	if ($long =~ s/^([^ ]+\.)([^ ]+) \2(\.[^ ]+\.[^ ]+)/$1$2$3/) {
		# matches shiva.cs cs.berkeley.edu and returns shiva.cs.berkeley.edu
		print "condensed fallback $host $domain_host -> $long\n" if $debug;
		$fallback{$long} = 9;
	}

	local($fh);
	$fh = $domain_host;
	while ($fh =~ /\./) {
		print "FALLBACK $host.$fh = 1\n" if $debug > 7;
		$fallback{"$host.$fh"} = 1;
		$fh =~ s/^[^\.]+\.//;
	}

	$fallback{"$host.$domain_host"} = 2;

	($domain = $domain_host) =~ s/^[^\.]+//;
	$fallback{"$host$domain"} = 6
		if ($domain =~ /\./);

	if ($host =~ /\./) {
		#
		# Host is already okay, but let's look for multiple
		# interpretations
		#
		print "domainify($host,$domain_host) = $host\n" if $debug;
		delete $fallback{$host};
		$domainify_fallback{"$u *** $host"} = join(' ',sort {$fallback{$b} <=> $fallback{$a};} keys %fallback) if %fallback;
		return $host;
	}

	$domain = ".$domain_host"
		if ($domain !~ /\..*\./);
	$newhost = "$host$domain";

	$create_host_backtrack{"$u *** $newhost"} = $domain_host;
	print "domainify($host,$domain_host) = $newhost\n" if $debug;
	delete $fallback{$newhost};
	$domainify_fallback{"$u *** $newhost"} = join(' ',sort {$fallback{$b} <=> $fallback{$a};} keys %fallback) if %fallback;
	if ($debug) {
		print "fallback = ";
		print $domainify_fallback{"$u *** $newhost"} 
			if defined($domainify_fallback{"$u *** $newhost"});
		print "\n";
	}
	return $newhost;
}
# return the first non-empty element of an array
sub firstname
{
	local(@names) = @_;
	local($n);
	while(@names) {
		$n = shift(@names);
		return $n if $n =~ /\S/;
	}
	return undef;
}
# queue up more addresses to expand
sub expn
{
	local($host,$addr,$name,$level) = @_;
	if ($host) {
		$host = &trhost($host);

		if (($debug > 3) || (defined $giveup{$host})) {
			unshift(@hosts,$host) unless $users{$host};
		} else {
			push(@hosts,$host) unless $users{$host};
		}
		$users{$host} .= " $addr";
		$names{"$addr *** $host"} = $name;
		$level{"$addr *** $host"} = $level + 1;
		print "expn($host,$addr,$name)\n" if $debug;
		return "\t$addr\n";
	} else {
		return &final($addr,'NONE',$name);
	}
}
# compute the numerical average value of an array
sub average
{
	local(@e) = @_;
	return 0 unless @e;
	local($e,$sum);
	for $e (@e) {
		$sum += $e;
	}
	$sum / @e;
}
# print to the server (also to stdout, if -w)
sub ps
{
	local($p) = @_;
	print ">>> $p\n" if $watch;
	print $S "$p\n";
}
# return case-adjusted name for a host (for comparison purposes)
sub trhost 
{
	# treat foo.bar as an alias for Foo.BAR
	local($host) = @_;
	local($trhost) = $host;
	$trhost =~ tr/A-Z/a-z/;
	if ($trhost{$trhost}) {
		$host = $trhost{$trhost};
	} else {
		$trhost{$trhost} = $host;
	}
	$trhost{$trhost};
}
# re-queue users if an mx record dictates a redirect
# don't allow a user to be redirected more than once
sub mxredirect
{
	local($server,*users) = @_;
	local($u,$nserver,@still_there);

	$nserver = &mx($server);

	if (&trhost($nserver) ne &trhost($server)) {
		$0 = "$av0 - mx redirect $server -> $nserver\n";
		for $u (@users) {
			if (defined $mxbacktrace{"$u *** $nserver"}) {
				push(@still_there,$u);
			} else {
				$mxbacktrace{"$u *** $nserver"} = $server;
				print "mxbacktrace{$u *** $nserver} = $server\n"
					if ($debug > 1);
				&expn($nserver,$u,$names{"$u *** $server"});
			}
		}
		@users = @still_there;
		if (! @users) {
			return $nserver;
		} else {
			return undef;
		}
	}
	return undef;
}
# follow mx records, return a hostname
# also follow temporary redirections coming from &domainify and
# &mxlookup
sub mx
{
	local($h,$u) = @_;

	for (;;) {
		if (defined $mx{&trhost($h)} && $h ne $mx{&trhost($h)}) {
			$0 = "$av0 - mx expand $h";
			$h = $mx{&trhost($h)};
			return $h;
		}
		if ($u) {
			if (defined $temporary_redirect{"$u *** $h"}) {
				$0 = "$av0 - internal redirect $h";
				print "Temporary redirect taken $u *** $h -> " if $debug;
				$h = $temporary_redirect{"$u *** $h"};
				print "$h\n" if $debug;
				next;
			}
			$htr = &trhost($h);
			if (defined $temporary_redirect{"$u *** $htr"}) {
				$0 = "$av0 - internal redirect $h";
				print "temporary redirect taken $u *** $h -> " if $debug;
				$h = $temporary_redirect{"$u *** $htr"};
				print "$h\n" if $debug;
				next;
			}
		}
		return $h;
	}
}
# look up mx records with the name server.
# re-queue expansion requests if possible
# optionally give up on this host.
sub mxlookup 
{
	local($lastchance,$server,$giveup,*users) = @_;
	local(*T);
	local(*NSLOOKUP);
	local($nh, $pref,$cpref);
	local($o0) = $0;
	local($nserver);
	local($name,$aliases,$type,$len,$thataddr);
	local(%fallback);

	return 1 if &mxredirect($server,*users);

	if ((defined $mx{$server}) || (! $have_nslookup)) {
		return 0 unless $lastchance;
		&giveup('mx domainify',$giveup);
		return 0;
	}

	$0 = "$av0 - nslookup of $server";
	sysopen(T,"/tmp/expn$$",O_RDWR|O_CREAT|O_EXCL,0600) || die "open > /tmp/expn$$: $!\n";
	print T "set querytype=MX\n";
	print T "$server\n";
	close(T);
	$cpref = 1.0E12;
	undef $nserver;
	open(NSLOOKUP,"nslookup < /tmp/expn$$ 2>&1 |") || die "open nslookup: $!";
	while(<NSLOOKUP>) {
		print if ($debug > 2);
		if (/mail exchanger = ([-A-Za-z_.0-9+]+)/) {
			$nh = $1;
			if (/preference = (\d+)/) {
				$pref = $1;
				if ($pref < $cpref) {
					$nserver = $nh;
					$cpref = $pref;
				} elsif ($pref) {
					$fallback{$pref} .= " $nh";
				}
			}
		}
		if (/Non-existent domain/) {
			#
			# These addresss are hosed.  Kaput!  Dead! 
			# However, if we created the address in the
			# first place then there is a chance of 
			# salvation.
			#
			1 while(<NSLOOKUP>);	
			close(NSLOOKUP);
			return 0 unless $lastchance;
			&giveup('domainify',"$server: Non-existent domain",undef,1);
			return 0;	
		}
				
	}
	close(NSLOOKUP);
	unlink("/tmp/expn$$");
	unless ($nserver) {
		$0 = "$o0 - finished mxlookup";
		return 0 unless $lastchance;
		&giveup('mx domainify',"$server: Could not resolve address");
		return 0;
	}

	# provide fallbacks in case $nserver doesn't work out
	if (defined $fallback{$cpref}) {
		$mx_secondary{$server} = $fallback{$cpref};
	}

	$0 = "$av0 - gethostbyname($nserver)";
	($name,$aliases,$type,$len,$thataddr) = gethostbyname($nserver);

	unless ($thataddr) {
		$0 = $o0;
		return 0 unless $lastchance;
		&giveup('mx domainify',"$nserver: could not resolve address");
		return 0;
	}
	print "MX($server) = $nserver\n" if $debug;
	print "$server -> $nserver\n" if $vw && !$debug;
	$mx{&trhost($server)} = $nserver;
	# redeploy the users
	unless (&mxredirect($server,*users)) {
		return 0 unless $lastchance;
		&giveup('mx domainify',"$nserver: only one level of mx redirect allowed");
		return 0;
	}
	$0 = "$o0 - finished mxlookup";
	return 1;
}
# if mx expansion did not help to resolve an address
# (ie: foo@bar became @baz:foo@bar, then undo the 
# expansion).
# this is only used by &final
sub mxunroll
{
	local(*host,*addr) = @_;
	local($r) = 0;
	print "looking for mxbacktrace{$addr *** $host}\n"
		if ($debug > 1);
	while (defined $mxbacktrace{"$addr *** $host"}) {
		print "Unrolling MX expnasion: \@$host:$addr -> " 
			if ($debug || $verbose);
		$host = $mxbacktrace{"$addr *** $host"};
		print "\@$host:$addr\n" 
			if ($debug || $verbose);
		$r = 1;
	}
	return 1 if $r;
	$addr = "\@$host:$addr"
		if ($host =~ /\./);
	return 0;
}
# register a completed expnasion.  Make the final address as 
# simple as possible.
sub final
{
	local($addr,$host,$name,$error) = @_;
	local($he);
	local($hb,$hr);
	local($au,$ah);

	if ($error =~ /Non-existent domain/) {
		# 
		# If we created the domain, then let's undo the
		# damage...
		#
		if (defined $create_host_backtrack{"$addr *** $host"}) {
			while (defined $create_host_backtrack{"$addr *** $host"}) {
				print "Un&domainifying($host) = " if $debug;
				$host = $create_host_backtrack{"$addr *** $host"};
				print "$host\n" if $debug;
			}
			$error = "$host: could not locate";
		} else {
			# 
			# If we only want valid addresses, toss out
			# bad host names.
			#
			if ($valid) {
				print STDERR "\@$host:$addr ($name) Non-existent domain\n";
				return "";
			}
		}
	}

	MXUNWIND: {
		$0 = "$av0 - final parsing of \@$host:$addr";
		($he = $host) =~ s/(\W)/\\$1/g;
		if ($addr !~ /@/) {
			# addr does not contain any host
			$addr = "$addr@$host";
		} elsif ($addr !~ /$he/i) {
			# if host part really something else, use the something
			# else.
			if ($addr =~ m/(.*)\@([^\@]+)$/) {
				($au,$ah) = ($1,$2);
				print "au = $au ah = $ah\n" if $debug;
				if (defined $temporary_redirect{"$addr *** $ah"}) {
					$addr = "$au\@".$temporary_redirect{"$addr *** $ah"};
					print "Rewrite! to $addr\n" if $debug;
					next MXUNWIND;
				}
			}
			# addr does not contain full host
			if ($valid) {
				if ($host =~ /^([^\.]+)(\..+)$/) {
					# host part has a . in it - foo.bar
					($hb, $hr) = ($1, $2);
					if ($addr =~ /\@([^\.\@]+)$/ && ($1 eq $hb)) {
						# addr part has not . 
						# and matches beginning of
						# host part -- tack on a 
						# domain name.
						$addr .= $hr;
					} else {
						&mxunroll(*host,*addr) 
							&& redo MXUNWIND;
					}
				} else {
					&mxunroll(*host,*addr) 
						&& redo MXUNWIND;
				}
			} else {
				$addr = "${addr}[\@$host]"
					if ($host =~ /\./);
			}
		}
	}
	$name = "$name " if $name;
	$error = " $error" if $error;
	if ($valid) {
		push(@final,"$name<$addr>");
	} else {
		push(@final,"$name<$addr>$error");
	}
	"\t$name<$addr>$error\n";
}

sub alarm
{
	local($alarm_action,$alarm_redirect,$alarm_user) = @_;
	alarm(3600);
	$SIG{ALRM} = 'handle_alarm';
}
# this involves one great big ugly hack.
# the "next HOST" unwinds the stack!
sub handle_alarm
{
	&giveup($alarm_redirect,"Timed out during $alarm_action",$alarm_user);
	next HOST;
}

# read the rest of the current smtp daemon's response (and toss it away)
sub read_response
{
	local($done,$watch) = @_;
	local(@resp);
	print $s if $watch;
	while(($done eq "-") && ($s = <$S>) && ($s =~ /^\d+([- ])/)) {
		print $s if $watch;
		$done = $1;
		push(@resp,$s);
	}
	return @resp;
}
# print args if verbose.  Return them in any case
sub verbose
{
	local(@tp) = @_;
	print "@tp" if $verbose;
}
# to pass perl -w:
@tp;
$flag_a;
$flag_d;
$flag_1;
%already_domainify_fellback;
%already_mx_fellback;
&handle_alarm;
################### BEGIN PERL/TROFF TRANSITION 
.00 ;	

'di
.nr nl 0-1
.nr % 0
.\\"'; __END__ 
.\" ############## END PERL/TROFF TRANSITION
.TH EXPN 1 "March 11, 1993"
.AT 3
.SH NAME
expn \- recursively expand mail aliases
.SH SYNOPSIS
.B expn
.RI [ -a ]
.RI [ -v ]
.RI [ -w ]
.RI [ -d ]
.RI [ -1 ]
.IR user [@ hostname ]
.RI [ user [@ hostname ]]...
.SH DESCRIPTION
.B expn
will use the SMTP
.B expn
and 
.B vrfy
commands to expand mail aliases.  
It will first look up the addresses you provide on the command line.
If those expand into addresses on other systems, it will 
connect to the other systems and expand again.  It will keep 
doing this until no further expansion is possible.
.SH OPTIONS
The default output of 
.B expn
can contain many lines which are not valid
email addresses.  With the 
.I -aa
flag, only expansions that result in legal addresses
are used.  Since many mailing lists have an illegal
address or two, the single
.IR -a ,
address, flag specifies that a few illegal addresses can
be mixed into the results.   More 
.I -a
flags vary the ratio.  Read the source to track down
the formula.  With the
.I -a
option, you should be able to construct a new mailing
list out of an existing one.
.LP
If you wish to limit the number of levels deep that 
.B expn
will recurse as it traces addresses, use the
.I -1
option.  For each 
.I -1
another level will be traversed.  So, 
.I -111
will traverse no more than three levels deep.
.LP
The normal mode of operation for
.B expn
is to do all of its work silently.
The following options make it more verbose.
It is not necessary to make it verbose to see what it is
doing because as it works, it changes its 
.BR argv [0]
variable to reflect its current activity.
To see how it is expanding things, the 
.IR -v ,
verbose, flag will cause 
.B expn 
to show each address before
and after translation as it works.
The 
.IR -w ,
watch, flag will cause
.B expn
to show you its conversations with the mail daemons.
Finally, the 
.IR -d ,
debug, flag will expose many of the inner workings so that
it is possible to eliminate bugs.
.SH ENVIRONMENT
No environment variables are used.
.SH FILES
.PD 0
.B /tmp/expn$$
.B temporary file used as input to 
.BR nslookup .
.SH SEE ALSO
.BR aliases (5), 
.BR sendmail (8),
.BR nslookup (8),
RFC 823, and RFC 1123.
.SH BUGS
Not all mail daemons will implement 
.B expn
or
.BR vrfy .
It is not possible to verify addresses that are served
by such daemons.
.LP
When attempting to connect to a system to verify an address,
.B expn
only tries one IP address.  Most mail daemons
will try harder.
.LP
It is assumed that you are running domain names and that 
the 
.BR nslookup (8) 
program is available.  If not, 
.B expn
will not be able to verify many addresses.  It will also pause
for a long time unless you change the code where it says
.I $have_nslookup = 1
to read
.I $have_nslookup = 
.IR 0 .
.LP
Lastly, 
.B expn
does not handle every valid address.  If you have an example,
please submit a bug report.
.SH CREDITS
In 1986 or so, Jon Broome wrote a program of the same name
that did about the same thing.  It has since suffered bit rot
and Jon Broome has dropped off the face of the earth!
(Jon, if you are out there, drop me a line)
.SH AVAILABILITY
The latest version of 
.B expn
is available through anonymous ftp at
.IR ftp://ftp.idiom.com/pub/muir-programs/expn .
.SH AUTHOR
.I David Muir Sharnoff\ \ \ \ <muir@idiom.com>
