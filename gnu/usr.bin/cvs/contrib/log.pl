#!/usr/bin/perl

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
#    date: Tuesday, August 6, 1991 @ 13:17
#    author: samborn
#    Update of /elmer/cvs/CVSROOT.adm
#    In directory astro:/home/samborn/CVSROOT.adm
#    
#    Modified Files:
#    	test3 
#    
#    Added Files:
#    	test6 
#    
#    Removed Files:
#    	test4 
#    
#    Log Message:
#    wow, what a test
#    
#    RCS:    1.4     /elmer/cvs/CVSROOT.adm/test3,v
#    RCS:    1.1     /elmer/cvs/CVSROOT.adm/test6,v
#    RCS:    1.1     /elmer/cvs/CVSROOT.adm/Attic/test4,v
#

#
# turn off setgid
#
$) = $(;

#
# parse command line arguments
#
@files = split(/ /,$ARGV[0]);
$logfile = $ARGV[1];
$cvsroot = $ENV{'CVSROOT'};

#
# Some date and time arrays
#
@mos = (January,February,March,April,May,June,July,August,September,
        October,November,December);
@days = (Sunday,Monday,Tuesday,Wednesday,Thursday,Friday,Saturday);
($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime;

#
# get login name
#
$login = getlogin || (getpwuid($<))[0] || "nobody";

#
# open log file for appending
#
if ((open(OUT, ">>" . $logfile)) != 1) {
    die "Could not open logfile " . $logfile . "\n";
}

# 
# Header
# 
print OUT "\n";
print OUT "**************************************\n";
print OUT "date: " . $days[$wday] . ", " . $mos[$mon] . " " . $mday . ", 19" . $year .
    " @ " . $hour . ":" . sprintf("%02d", $min) . "\n";
print OUT "author: " . $login . "\n";

#
#print the stuff on stdin to the logfile
#
open(IN, "-");
while(<IN>) {
   print OUT $_;
}
close(IN);

print OUT "\n";

#
# after log information, do an 'cvs -Qn status' on each file in the arguments.
#
for $file (@files[1..$#files]) {
    if ($file eq "-") {
	last;
    }
    open(RCS,"-|") || exec 'cvs', '-Qn', 'status', $file;
    while (<RCS>) {
        if (substr($_, 0, 7) eq "    RCS") {
            print OUT;
        }
    }
    close (RCS);
}

close (OUT);
