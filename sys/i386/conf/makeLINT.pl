#! /usr/bin/perl
# $FreeBSD$

$lno=0;
while (<>) {
	chop;
	s/#.*//;
	s/\f//g;
	s/^[ \t]+//;
	$line = $_;
	$lno++;
	($key, @rest) = split;
	next if ($key eq "");
	next if ($key =~ /^hint\./);
	if ($key eq "machine" || $key eq "ident" || $key eq "device" ||
	    $key eq "makeoptions" || $key eq "options" ||
	    $key eq "profile" ||
	    $key eq "cpu" || $key eq "option" || $key eq "maxusers") {
		print "$line\n";
	} else {
		print STDERR "unrecognized line: line $lno: $line\n";
	}
}
