#!/usr/bin/perl
# doublebounce.pl
#	attempt to return a doubly-bounced email to a postmaster
#	jr@terra.net, 12/4/97
#
#	invoke by creating an mail alias such as:
#		doublebounce:	"|/usr/local/sbin/doublebounce"
#	then adding this line to your sendmail.cf:
#		O DoubleBounceAddress=doublebounce
#
#	optionally, add a "-d" flag in the aliases file, to send a
#	debug trace to your own postmaster showing what is going on
#
#	this allows the "postmaster" address to still go to a human being,
#	while bounce messages can go to this script, which will bounce them
#	back to the postmaster at the sending site.
#
#	the algorithm is to scan the double-bounce error report generated
#	by sendmail on stdin, for the original message (it starts after the
#	second "Orignal message follows" marker), look for From, Sender, and
#	Received headers from the point closest to the sender back to the point
#	closest to us, and try to deliver a double-bounce report back to a
#	postmaster at one of these sites in the hope that they can
#	return the message to the original sender, or do something about
#	the fact that that sender's return address is not valid.


use Socket;

# look for debug flag
#
$dflag = 0;
$dflag = 1 if ($ARGV[0] eq "-d");

# get local host name
#	you may need to edit these two lines for however your system does this
#
$host = `hostname`; chop($host);
$domain = `dnsdomainname`; chop($domain);

# get temp file name
$tmp = "/tmp/doubb$$";

# save message from STDIN to a file
#	I thought about reading it into a buffer here, but some messages
#	are 10+Mb so a buffer may not be a good idea
#
if (! open(MSG, "+> $tmp")) {
	# can't open temp file -- send message to local postmaster
	# open(MAIL, "| /usr/sbin/sendmail -oeq postmaster");
	print MAIL <STDIN>;
	close(MAIL);
	exit(1);
}
print MSG <STDIN>;

# scan message for list of possible sender sites
#	note that original message appears after the second
#	"Original message follows" marker
#	look for From, Sender, and Reply-To and try them, too
#
$inhdr = 0;
$hdrs = 0;
$skip = 0;
seek(MSG, 0, 0);
while (<MSG>) {
	chop;
	if (/^   ----- Original message follows -----$/
	     || /^   ----Unsent message follows----$/) {
		$i = 0;
		$inhdr = 1;
		$hdrs++;
		$skip = 1;
		next;
	}
	if ($skip) {
		$skip--;
		next;
	}
	if (/^$/) {
		last if ($hdrs >= 2);
		$inhdr = 0;
		next;
	}
	if (! $inhdr) {
		next;
	}
	if (! /^[ \t]/) { $hdr[$i++] = $_ }
	else {
		$i--;
		$hdr[$i++] .= $_;
	}
}
$rcvd = 0;
for ($j = 0; $j < $i; $j++) {
	print STDERR "DEBUG hdr[$j] = $hdr[$j]\n";
	if ($hdr[$j] =~ /^received:/i) {
		($addr[$rcvd++]) = $hdr[$j] =~ m/.*\sby\s([^\s]+)\s.*/;
	}
	if ($hdr[$j] =~ /^reply-to:/i) {
		($addr1{"reply-to"} = $hdr[$j]) =~ s/^reply-to: *//i;
	}
	if ($hdr[$j] =~ /^sender:/i) {
		($addr1{"sender"} = $hdr[$j]) =~ s/^sender: *//i;
		}
	if ($hdr[$j] =~ /^from:/i) {
		($addr1{"from"} = $hdr[$j]) =~ s/^from: *//i;
	}
}

# %addr and %addr1 arrays now contain lists of possible sites (or From headers).
# Go through them parsing for the site name, and attempting to send
# to the named person or postmaster@ each site in turn until successful
#
if ($dflag) {
	open(DEBUG, "|/usr/sbin/sendmail postmaster");
	print DEBUG "Subject: double bounce dialog\n";
}
$sent = 0;
# foreach $x ("from", "sender", "reply-to") {
foreach $x ("from", "sender") {
	$y = &parseaddr($addr1{$x});
	if ($y) {
		print DEBUG "Trying $y\n" if ($dflag);
		if (&sendbounce("$y")) {
			$sent++;
			last;
		}
		$y =~ s/.*@//;
		print DEBUG "Trying postmaster\@$y\n" if ($dflag);
		if (&sendbounce("postmaster\@$y")) {
			$sent++;
			last;
		}
	}
}
if (! $sent) {
	$rcvd--;
	for ($i = $rcvd; $i >= 0; $i--) {
		$y = &parseaddr($addr[$i]);
		$y =~ s/.*@//;
		if ($y) {
			print DEBUG "Trying postmaster\@$y\n" if ($dflag);
			if (&sendbounce("postmaster\@$y")) {
				$sent++;
				last;
			}
		}
	}
}
if (! $sent) {
	# queer things are happening to me
	# $addr[0] should be own domain, so we should have just
	# tried postmaster@our.domain.  theoretically, we should
	# not get here...
	if ($dflag) {
		print DEBUG "queer things are happening to me\n";
		print DEBUG "Trying postmaster\n";
	}
	&sendbounce("postmaster");
}

# clean up and get out
#
if ($dflag) {
	seek(MSG, 0, 0);
	print DEBUG "\n---\n"; print DEBUG <MSG>;
	close(DEBUG);
}
close(MSG);
unlink("$tmp");
exit(0);





# parseaddr()
#	parse hostname from From: header
#
sub parseaddr {
	local($hdr) = @_;
	local($addr);

	if ($hdr =~ /<.*>/) {
		($addr) = $hdr =~ m/<(.*)>/;
		return $addr;
	}
	if ($addr =~ /\s*\(/) {
		($addr) = $hdr =~ m/\s*(.*)\s*\(/;
		return $addr;
	}
	($addr) = $hdr =~ m/\s*(.*)\s*/;
	return $addr;
}


# sendbounce()
#	send bounce to postmaster
#
#	this re-invokes sendmail in immediate and quiet mode to try
#	to deliver to a postmaster.  sendmail's exit status tells us
#	wether the delivery attempt really was successful.
#
sub sendbounce {
	local($dest) = @_;
	local($st);

	open(MAIL, "| /usr/sbin/sendmail -ocn -odi -oeq $dest");
	print MAIL <<EOT;
From: Mail Delivery Subsystem <mail-router\@$domain>
Subject: Postmaster notify: double bounce
Reply-To: nobody\@$domain
Errors-To: nobody\@$domain
Precedence: junk
Auto-Submitted: auto-generated (postmaster notification)

The following message was received at $host.$domain for an invalid
recipient.  The sender's address was also invalid.  Since the message
originated at or transited through your mailer, this notification is being
sent to you in the hope that you will determine the real originator and
have them correct their From or Sender address.

The invalid sender address was: $addr1{"from"}.

   ----- The following is a double bounce at $host.$domain -----

EOT
	seek(MSG, 0, 0);
	print MAIL <MSG>;
	return close(MAIL);
}
