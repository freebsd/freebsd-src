#! xPERL_PATHx
# -*-Perl-*-
#
# From: clyne@niwot.scd.ucar.EDU (John Clyne)
# Date: Fri, 28 Feb 92 09:54:21 MST
# 
# BTW, i wrote a perl script that is similar to 'nfpipe' except that in
# addition to logging to a file it provides a command line option for mailing
# change notices to a group of users. Obviously you probably wouldn't want
# to mail every change. But there may be certain directories that are commonly
# accessed by a group of users who would benefit from an email notice. 
# Especially if they regularly beat on the same directory. Anyway if you 
# think anyone would be interested here it is. 
#
#      $Id: mfpipe.pl,v 1.2 1995/07/10 02:01:57 kfogel Exp $
#
#
#	File:		mfpipe
#
#	Author:		John Clyne
#			National Center for Atmospheric Research
#			PO 3000, Boulder, Colorado
#
#	Date:		Wed Feb 26 18:34:53 MST 1992
#
#	Description:	Tee standard input to mail a list of users and to
#			a file. Used by CVS logging.
#
#	Usage:		mfpipe [-f file] [user@host...]
#
#	Environment:	CVSROOT	
#				Path to CVS root.
#
#	Files:
#
#
#	Options:	-f file	
#				Capture output to 'file'
#			

$header = "Log Message:\n";

$mailcmd = "| mail -s  'CVS update notice'";
$whoami = `whoami`;
chop $whoami;
$date = `date`;
chop $date;

$cvsroot = $ENV{'CVSROOT'};

while (@ARGV) {
        $arg = shift @ARGV;

	if ($arg eq '-f') {
                $file = shift @ARGV;
	}
	else {
		$users = "$users $arg";
	}
}

if ($users) {
	$mailcmd = "$mailcmd $users";
	open(MAIL, $mailcmd) || die "Execing $mail: $!\n";
}
 
if ($file) {
	$logfile = "$cvsroot/LOG/$file";
	open(FILE, ">> $logfile") || die "Opening $logfile: $!\n";
}

print FILE "$whoami $date--------BEGIN LOG ENTRY-------------\n" if ($logfile);

while (<>) {
	print FILE $log if ($log && $logfile);

	print FILE $_ if ($logfile);
	print MAIL $_ if ($users);

	$log = "log: " if ($_ eq $header);
}

close FILE;
die "Write failed" if $?;
close MAIL;
die "Mail failed" if $?;

exit 0;
