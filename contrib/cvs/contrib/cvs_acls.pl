#! xPERL_PATHx
# -*-Perl-*-
#
# $Id: cvs_acls.pl,v 1.2 1995/07/10 02:01:33 kfogel Exp $
#
# Access control lists for CVS.  dgg@ksr.com (David G. Grubbs)
#
# CVS "commitinfo" for matching repository names, running the program it finds
# on the same line.  More information is available in the CVS man pages.
#
# ==== INSTALLATION:
#
# To use this program as I intended, do the following four things:
#
# 0. Install PERL.  :-)
#
# 1. Put one line, as the *only* non-comment line, in your commitinfo file:
#
#	DEFAULT		/usr/local/bin/cvs_acls
#
# 2. Install this file as /usr/local/bin/cvs_acls and make it executable.
#
# 3. Create a file named $CVSROOT/CVSROOT/avail.
#
# ==== FORMAT OF THE avail FILE:
#
# The avail file determines whether you may commit files.  It contains lines
# read from top to bottom, keeping track of a single "bit".  The "bit"
# defaults to "on".  It can be turned "off" by "unavail" lines and "on" by
# "avail" lines.  ==> Last one counts.
#
# Any line not beginning with "avail" or "unavail" is ignored.
#
# Lines beginning with "avail" or "unavail" are assumed to be '|'-separated
# triples: (All spaces and tabs are ignored in a line.)
#
#   {avail.*,unavail.*} [| user,user,... [| repos,repos,...]]
#
#    1. String starting with "avail" or "unavail".
#    2. Optional, comma-separated list of usernames.
#    3. Optional, comma-separated list of repository pathnames.
#	These are pathnames relative to $CVSROOT.  They can be directories or
#	filenames.  A directory name allows access to all files and
#	directories below it.
#
# Example:  (Text from the ';;' rightward may not appear in the file.)
#
#	unavail			;; Make whole repository unavailable.
#	avail|dgg		;; Except for user "dgg".
#	avail|fred, john|bin/ls	;; Except when "fred" or "john" commit to
#				;; the module whose repository is "bin/ls"
#
# PROGRAM LOGIC:
#
#	CVS passes to @ARGV an absolute directory pathname (the repository
#	appended to your $CVSROOT variable), followed by a list of filenames
#	within that directory.
#
#	We walk through the avail file looking for a line that matches both
#	the username and repository.
#
#	A username match is simply the user's name appearing in the second
#	column of the avail line in a space-or-comma separate list.
#
#	A repository match is either:
#		- One element of the third column matches $ARGV[0], or some
#		  parent directory of $ARGV[0].
#		- Otherwise *all* file arguments ($ARGV[1..$#ARGV]) must be
#		  in the file list in one avail line.
#	    - In other words, using directory names in the third column of
#	      the avail file allows committing of any file (or group of
#	      files in a single commit) in the tree below that directory.
#	    - If individual file names are used in the third column of
#	      the avail file, then files must be committed individually or
#	      all files specified in a single commit must all appear in
#	      third column of a single avail line.
#

$debug = 0;
$cvsroot = $ENV{'CVSROOT'};
$availfile = $cvsroot . "/CVSROOT/avail";
$myname = $ENV{"USER"} if !($myname = $ENV{"LOGNAME"});

eval "print STDERR \$die='Unknown parameter $1\n' if !defined \$$1; \$$1=\$';"
    while ($ARGV[0] =~ /^(\w+)=/ && shift(@ARGV));
exit 255 if $die;		# process any variable=value switches

die "Must set CVSROOT\n" if !$cvsroot;
($repos = shift) =~ s:^$cvsroot/::;
grep($_ = $repos . '/' . $_, @ARGV);

print "$$ Repos: $repos\n","$$ ==== ",join("\n$$ ==== ",@ARGV),"\n" if $debug;

$exit_val = 0;				# Good Exit value

$universal_off = 0;
open (AVAIL, $availfile) || exit(0);	# It is ok for avail file not to exist
while (<AVAIL>) {
    chop;
    next if /^\s*\#/;
    next if /^\s*$/;
    ($flagstr, $u, $m) = split(/[\s,]*\|[\s,]*/, $_);

    # Skip anything not starting with "avail" or "unavail" and complain.
    (print "Bad avail line: $_\n"), next
	if ($flagstr !~ /^avail/ && $flagstr !~ /^unavail/);

    # Set which bit we are playing with. ('0' is OK == Available).
    $flag = (($& eq "avail") ? 0 : 1);

    # If we find a "universal off" flag (i.e. a simple "unavail") remember it
    $universal_off = 1 if ($flag && !$u && !$m);

    # $myname considered "in user list" if actually in list or is NULL
    $in_user = (!$u || grep ($_ eq $myname, split(/[\s,]+/,$u)));
    print "$$ \$myname($myname) in user list: $_\n" if $debug && $in_user;

    # Module matches if it is a NULL module list in the avail line.  If module
    # list is not null, we check every argument combination.
    if (!($in_repo = !$m)) {
	@tmp = split(/[\s,]+/,$m);
	for $j (@tmp) {
	    # If the repos from avail is a parent(or equal) dir of $repos, OK
	    $in_repo = 1, last if ($repos eq $j || $repos =~ /^$j\//);
	}
	if (!$in_repo) {
	    $in_repo = 1;
	    for $j (@ARGV) {
		last if !($in_repo = grep ($_ eq $j, @tmp));
	    }
	}
    }
    print "$$ \$repos($repos) in repository list: $_\n" if $debug && $in_repo;

    $exit_val = $flag if ($in_user && $in_repo);
    print "$$ ==== \$exit_val = $exit_val\n$$ ==== \$flag = $flag\n" if $debug;
}
close(AVAIL);
print "$$ ==== \$exit_val = $exit_val\n" if $debug;
print "**** Access denied: Insufficient Karma ($myname|$repos)\n" if $exit_val;
print "**** Access allowed: Personal Karma exceeds Environmental Karma.\n"
	if $universal_off && !$exit_val;
exit($exit_val);
