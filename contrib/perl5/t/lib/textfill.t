#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

@tests = (split(/\nEND\n/s, <<DONE));
TEST1
Cyberdog Information

Cyberdog & Netscape in the news
Important Press Release regarding Cyberdog and Netscape. Check it out! 

Cyberdog Plug-in Support!
Cyberdog support for Netscape Plug-ins is now available to download! Go
to the Cyberdog Beta Download page and download it now! 

Cyberdog Book
Check out Jesse Feiler's way-cool book about Cyberdog. You can find
details out about the book as well as ordering information at Philmont
Software Mill site. 

Java!
Looking to view Java applets in Cyberdog 1.1 Beta 3? Download and install
the Mac OS Runtime for Java and try it out! 

Cyberdog 1.1 Beta 3
We hope that Cyberdog and OpenDoc 1.1 will be available within the next
two weeks. In the meantime, we have released another version of
Cyberdog, Cyberdog 1.1 Beta 3. This version fixes several bugs that were
reported to us during out public beta period. You can check out our release
notes to see what we fixed! 
END
    Cyberdog Information
    Cyberdog & Netscape in the news Important Press Release regarding
 Cyberdog and Netscape. Check it out! 
    Cyberdog Plug-in Support! Cyberdog support for Netscape Plug-ins is now
 available to download! Go to the Cyberdog Beta Download page and download
 it now! 
    Cyberdog Book Check out Jesse Feiler's way-cool book about Cyberdog.
 You can find details out about the book as well as ordering information at
 Philmont Software Mill site. 
    Java! Looking to view Java applets in Cyberdog 1.1 Beta 3? Download and
 install the Mac OS Runtime for Java and try it out! 
    Cyberdog 1.1 Beta 3 We hope that Cyberdog and OpenDoc 1.1 will be
 available within the next two weeks. In the meantime, we have released
 another version of Cyberdog, Cyberdog 1.1 Beta 3. This version fixes
 several bugs that were reported to us during out public beta period. You
 can check out our release notes to see what we fixed! 
END
DONE


$| = 1;

print "1..", @tests/2, "\n";

use Text::Wrap;

$rerun = $ENV{'PERL_DL_NONLAZY'} ? 0 : 1;

$tn = 1;
while (@tests) {
	my $in = shift(@tests);
	my $out = shift(@tests);

	$in =~ s/^TEST(\d+)?\n//;

	my $back = fill('    ', ' ', $in);

	if ($back eq $out) {
		print "ok $tn\n";
	} elsif ($rerun) {
		my $oi = $in;
		open(F,">#o") and do { print F $back; close(F) };
		open(F,">#e") and do { print F $out;  close(F) };
		foreach ($in, $back, $out) {
			s/\t/^I\t/gs;
			s/\n/\$\n/gs;
		}
		print "------------ input ------------\n";
		print $in;
		print "\n------------ output -----------\n";
		print $back;
		print "\n------------ expected ---------\n";
		print $out;
		print "\n-------------------------------\n";
		$Text::Wrap::debug = 1;
		fill('    ', ' ', $oi);
		exit(1);
	} else {
		print "not ok $tn\n";
	}
	$tn++;
}
