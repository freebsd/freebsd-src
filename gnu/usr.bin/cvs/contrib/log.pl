#! /local/bin/perl

# Modified by woods@web.apc.org to add support for mailing	3/29/93
#	use '-m user' for each user to receive cvs log reports
#	and use '-f logfile' for the logfile to append to
#
# Modified by berliner@Sun.COM to add support for CVS 1.3	2/27/92
#
# Date: Tue, 6 Aug 91 13:27 EDT
# From: samborn@sunrise.com (Kevin Samborn)
#
# I revised the perl script I sent you yesterday to use the info you
# send in on stdin.  (I am appending the newer script to the end)
#
# now the output looks like this:
#
#    **************************************
#    Date: Tuesday, August 6, 1991 @ 13:17
#    Author: samborn
#
#    Update of /elmer/cvs/CVSROOT.adm
#    In directory astro:/home/samborn/CVSROOT.adm
#    
#    Modified Files:
#    	test3 
#    Added Files:
#    	test6 
#    Removed Files:
#    	test4 
#    Log Message:
#    wow, what a test
#    
#    File: test.3	Status: Up-to-date 
#        Version:	1.4     Thu Apr 29 14:47:07 EDT 1993
#    File: test6	Status: Up-to-date
#        Version:	1.1     Thu Apr 29 14:47:33 EDT 1993
#    File: test4	Status: Up-to-date
#        Version:	1.1     Thu Apr 29 14:47:46 EDT 1993
#

$cvsroot = $ENV{'CVSROOT'};

# turn off setgid
#
$) = $(;

# parse command line arguments
#
while (@ARGV) {
        $arg = shift @ARGV;

	if ($arg eq '-m') {
                $users = "$users " . shift @ARGV;
	} elsif ($arg eq '-f') {
		($logfile) && die "Too many '-f' args";
		$logfile = shift @ARGV;
	} else {
		($donefiles) && die "Too many arguments!\n";
		$donefiles = 1;
		@files = split(/ /, $arg);
	}
}

$srepos = shift @files;
$mailcmd = "| Mail -s 'CVS update: $srepos'";

# Some date and time arrays
#
@mos = (January,February,March,April,May,June,July,August,September,
        October,November,December);
@days = (Sunday,Monday,Tuesday,Wednesday,Thursday,Friday,Saturday);

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime;

# get login name
#
$login = getlogin || (getpwuid($<))[0] || "nobody";

# open log file for appending
#
open(OUT, ">>" . $logfile) || die "Could not open(" . $logfile . "): $!\n";
if ($users) {
	$mailcmd = "$mailcmd $users";
	open(MAIL, $mailcmd) || die "Could not Exec($mailcmd): $!\n";
}

# print out the log Header
# 
print OUT "\n";
print OUT "**************************************\n";
print OUT "Date:\t$days[$wday] $mos[$mon] $mday, 19$year @ $hour:" . sprintf("%02d", $min) . "\n";
print OUT "Author:\t$login\n\n";

if (MAIL) {
	print MAIL "\n";
	print MAIL "Date:\t$days[$wday] $mos[$mon] $mday, 19$year @ $hour:" . sprintf("%02d", $min) . "\n";
	print MAIL "Author:\t$login\n\n";
}

# print the stuff from logmsg that comes in on stdin to the logfile
#
open(IN, "-");
while (<IN>) {
	print OUT $_;
	if (MAIL) {
		print MAIL $_;
	}
}
close(IN);

print OUT "\n";

# after log information, do an 'cvs -Qn status' on each file in the arguments.
#
while (@files) {
	$file = shift @files;
	if ($file eq "-") {
		print OUT "[input file was '-']\n";
		if (MAIL) {
			print MAIL "[input file was '-']\n";
		}
		last;
	}

	open(RCS, "-|") || exec 'cvs', '-Qn', 'status', $file;

	while (<RCS>) {
		if (/^[ \t]*Version/ || /^File:/) {
			print OUT;
			if (MAIL) {
				print MAIL;
			}
		}
	}
	close(RCS);
}

close(OUT);
die "Write to $logfile failed" if $?;

close(MAIL);
die "Pipe to $mailcmd failed" if $?;

exit 0;

### Local Variables:
### eval: (fundamental-mode)
### End:
