#!/usr/local/bin/perl
eval ' exec /usr/local/bin/perl $0 "$@" '
	if $running_under_some_shell;

# From a script by <Bill.Campbell@celestial.com>
# Newsgroups: comp.sources.misc
# Subject: v28i073:  uureroute - Reroute HDB queued mail, Part01/01
# Date: 26 Feb 92 02:28:37 GMT
#
# This is a Honey DanBer specific routine written in perl to reroute all
# mail queued up for a specific host.  It needs to be run as "root" since
# uucp will not allow itself to remove others requests.
#
# Revision ***  92/21/09:  Francois Pinard <pinard@iro.umontreal.ca>
#	1.	adapted for Taylor UUCP
#
# Revision 1.3  91/10/08  09:01:21  src
# 	1.	Rewritten in perl
#   	2.	Add -v option for debugging.
#
# Revision 1.2  91/10/07  23:57:42  root
#	1.	Fix mail program path.
#	2.	Truncate directory name to 7 characters

($progname = $0) =~ s!.*/!!;	# save this very early

$USAGE = "
#   Reroute uucp mail
#
#   Usage: $progname [-v] host [host...]
#
# Options   Argument    Description
#   -v                  Verbose (doesn't execute /bin/sh)
#
";

$UUSTAT = "/usr/local/bin/uustat";
$SHELL = "/bin/sh";
$SMAIL = "/bin/smail";

sub usage
{
    die join ("\n", @_) . "\n$USAGE\n";
}

do "getopts.pl";

&usage ("Invalid Option") unless do Getopts ("vV");

$verbose = ($opt_v ? '-v' : ());
$suffix = ($verbose ? '' : $$);

&usage ("No system specified") if $#ARGV < 0;

if (!$verbose)
{
    open (SHELL, "| $SHELL");
    select SHELL;
}

while ($system = shift)
{
    $sysprefix = substr ($system, 0, 7);
    $directory = "/usr/spool/uucp/$sysprefix";
    open (UUSTAT, "$UUSTAT -s $system -c rmail |");
    print "set -ex\n";
    while (<UUSTAT>)
    {
	($jobid, ) = split;
	($cfile) = substr ($jobid, length ($jobid) - 5);
	$cfilename = "$directory/C./C.$cfile";
	open (CFILE, $cfilename) || die "Cannot open $cfilename\n";
	$_ = <CFILE>;
	close CFILE;
	if (/^E D\.(....) [^ ]+ [^ ]+ -CR D\.\1 0666 [^ ]+ 0 rmail (.*)/)
	{
	    $datafile = "$directory/D./D.$1";
	    $address = $2;
	}
	else
	{
	    print STDERR;
	    die "Cannot parse previous line from $cfilename\n";
	}
	print "$SMAIL -R $system!$address < $datafile && $UUSTAT -k $jobid\n";
    }
    close UUSTAT;
}
close SHELL unless $verbose;

exit 0;
