#! xPERL_PATHx
# -*-Perl-*-
#
# XXX: FIXME: handle multiple '-f logfile' arguments
#
# XXX -- I HATE Perl!  This *will* be re-written in shell/awk/sed soon!
#

# Usage:  log.pl [[-m user] ...] [-s] -f logfile 'dirname file ...'
#
#	-m user		- for each user to receive cvs log reports
#			(multiple -m's permitted)
#	-s		- to prevent "cvs status -v" messages
#	-f logfile	- for the logfile to append to (mandatory,
#			but only one logfile can be specified).

# here is what the output looks like:
#
#    From: woods@kuma.domain.top
#    Subject: CVS update: testmodule
#
#    Date: Wednesday November 23, 1994 @ 14:15
#    Author: woods
#
#    Update of /local/src-CVS/testmodule
#    In directory kuma:/home/kuma/woods/work.d/testmodule
#    
#    Modified Files:
#    	test3 
#    Added Files:
#    	test6 
#    Removed Files:
#    	test4 
#    Log Message:
#    - wow, what a test
#
# (and for each file the "cvs status -v" output is appended unless -s is used)
#
#    ==================================================================
#    File: test3           	Status: Up-to-date
#    
#       Working revision:	1.41	Wed Nov 23 14:15:59 1994
#       Repository revision:	1.41	/local/src-CVS/cvs/testmodule/test3,v
#       Sticky Options:	-ko
#    
#       Existing Tags:
#    	local-v2                 	(revision: 1.7)
#    	local-v1                 	(revision: 1.1.1.2)
#    	CVS-1_4A2                	(revision: 1.1.1.2)
#    	local-v0                 	(revision: 1.2)
#    	CVS-1_4A1                	(revision: 1.1.1.1)
#    	CVS                      	(branch: 1.1.1)

$cvsroot = $ENV{'CVSROOT'};

# turn off setgid
#
$) = $(;

$dostatus = 1;

# parse command line arguments
#
while (@ARGV) {
        $arg = shift @ARGV;

	if ($arg eq '-m') {
                $users = "$users " . shift @ARGV;
	} elsif ($arg eq '-f') {
		($logfile) && die "Too many '-f' args";
		$logfile = shift @ARGV;
	} elsif ($arg eq '-s') {
		$dostatus = 0;
	} else {
		($donefiles) && die "Too many arguments!\n";
		$donefiles = 1;
		@files = split(/ /, $arg);
	}
}

# the first argument is the module location relative to $CVSROOT
#
$modulepath = shift @files;

$mailcmd = "| Mail -s 'CVS update: $modulepath'";

# Initialise some date and time arrays
#
@mos = (January,February,March,April,May,June,July,August,September,
        October,November,December);
@days = (Sunday,Monday,Tuesday,Wednesday,Thursday,Friday,Saturday);

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime;

# get a login name for the guy doing the commit....
#
$login = getlogin || (getpwuid($<))[0] || "nobody";

# open log file for appending
#
open(OUT, ">>" . $logfile) || die "Could not open(" . $logfile . "): $!\n";

# send mail, if there's anyone to send to!
#
if ($users) {
	$mailcmd = "$mailcmd $users";
	open(MAIL, $mailcmd) || die "Could not Exec($mailcmd): $!\n";
}

# print out the log Header
# 
print OUT "\n";
print OUT "****************************************\n";
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

# after log information, do an 'cvs -Qq status -v' on each file in the arguments.
#
if ($dostatus != 0) {
	while (@files) {
		$file = shift @files;
		if ($file eq "-") {
			print OUT "[input file was '-']\n";
			if (MAIL) {
				print MAIL "[input file was '-']\n";
			}
			last;
		}
		open(RCS, "-|") || exec 'cvs', '-nQq', 'status', '-v', $file;
		while (<RCS>) {
			print OUT;
			if (MAIL) {
				print MAIL;
			}
		}
		close(RCS);
	}
}

close(OUT);
die "Write to $logfile failed" if $?;

close(MAIL);
die "Pipe to $mailcmd failed" if $?;

## must exit cleanly
##
exit 0;
