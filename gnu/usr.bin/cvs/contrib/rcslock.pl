#!/usr/bin/perl

# Author: John Rouillard (rouilj@cs.umb.edu)
# Supported: Yeah right. (Well what do you expect for 2 hours work?)
# Blame-to: rouilj@cs.umb.edu
# Complaints to: Anybody except Brian Berliner, he's blameless for
#		 this script.
# Acknowlegements: The base code for this script has been acquired
# 		   from the log.pl script.

# rcslock.pl - A program to prevent commits when a file to be ckecked
# 	       in is locked in the repository.

# There are times when you need exclusive access to a file.  This
# often occurs when binaries are checked into the repository, since
# cvs's (actually rcs's) text based merging mechanism won't work. This
# script allows you to use the rcs lock mechanism (rcs -l) to make
# sure that no changes to a repository are able to be committed if
# those changes would result in a locked file being changed.

# WARNING:
# This script will work only if locking is set to strict.
#

# Setup:
# Add the following line to the commitinfo file:

#         ALL /local/location/for/script/lockcheck [options]

# Where ALL is replaced by any suitable regular expression.
# Options are -v for verbose info, or -d for debugging info.
# The %s will provide the repository directory name and the names of
# all changed files.  

# Use:
# When a developer needs exclusive access to a version of a file, s/he
# should use "rcs -l" in the repository tree to lock the version they
# are working on.  CVS will automagically release the lock when the
# commit is performed.

# Method:
# An "rlog -h" is exec'ed to give info on all about to be
# committed files.  This (header) information is parsed to determine
# if any locks are outstanding and what versions of the file are
# locked.  This filename, version number info is used to index an
# associative array.  All of the files to be committed are checked to
# see if any locks are outstanding.  If locks are outstanding, the
# version number of the current file (taken from the CVS/Entries
# subdirectory) is used in the key to determine if that version is
# locked. If the file being checked in is locked by the person doing
# the checkin, the commit is allowed, but if the lock is held on that
# version of a file by another person, the commit is not allowed.

$ext = ",v";  # The extension on your rcs files.

$\="\n";  # I hate having to put \n's at the end of my print statements
$,=' ';   # Spaces should occur between arguments to print when printed

# turn off setgid
#
$) = $(;

#
# parse command line arguments
#
require 'getopts.pl';

&Getopts("vd"); # verbose or debugging

# Verbose is useful when debugging
$opt_v = $opt_d if defined $opt_d;

# $files[0] is really the name of the subdirectory.
# @files = split(/ /,$ARGV[0]);
@files = @ARGV[0..$#ARGV];
$cvsroot = $ENV{'CVSROOT'};

#
# get login name
#
$login = getlogin || (getpwuid($<))[0] || "nobody";

#
# save the current directory since we have to return here to parse the
# CVS/Entries file if a lock is found.
#
$pwd = `/bin/pwd`;
chop $pwd;

print "Starting directory is $pwd" if defined $opt_d ;

#
# cd to the repository directory and check on the files.
#
print "Checking directory ", $files[0] if defined $opt_v ;

if ( $files[0] =~ /^\// )
{
   print "Directory path is $files[0]" if defined $opt_d ;
   chdir $files[0] || die "Can't change to repository directory $files[0]" ;
}
else
{
   print "Directory path is $cvsroot/$files[0]" if defined $opt_d ;
   chdir ($cvsroot . "/" . $files[0]) || 
         die "Can't change to repository directory $files[0] in $cvsroot" ;
}


# Open the rlog process and apss all of the file names to that one
# process to cut down on exec overhead.  This may backfire if there
# are too many files for the system buffer to handle, but if there are
# that many files, chances are that the cvs repository is not set up
# cleanly.

print "opening rlog -h @files[1..$#files] |" if defined $opt_d;

open( RLOG, "rlog -h @files[1..$#files] |") || die "Can't run rlog command" ;

# Create the locks associative array.  The elements in the array are
# of two types:
#
#  The name of the RCS file with a value of the total number of locks found
#            for that file,
# or
#
# The name of the rcs file concatenated with the version number of the lock.
# The value of this element is the name of the locker.

# The regular expressions used to split the rcs info may have to be changed.
# The current ones work for rcs 5.6.

$lock = 0;

while (<RLOG>)
{
	chop;
	next if /^$/; # ditch blank lines

	if ( $_ =~ /^RCS file: (.*)$/ )
	{
	   $curfile = $1;
	   next;
	}

	if ( $_ =~ /^locks: strict$/ )
	{
  	  $lock = 1 ;
	  next;
	}

	if ( $lock )
	{
	  # access list: is the line immediately following the list of locks.
	  if ( /^access list:/ )
	  { # we are done getting lock info for this file.
	    $lock = 0;
	  }
	  else
	  { # We are accumulating lock info.

	    # increment the lock count
	    $locks{$curfile}++;
	    # save the info on the version that is locked. $2 is the
            # version number $1 is the name of the locker.
	    $locks{"$curfile" . "$2"} = $1 
				if /[ 	]*([a-zA-Z._]*): ([0-9.]*)$/;

	    print "lock by $1 found on $curfile version $2" if defined $opt_d;

	  }
	}
}

# Lets go back to the starting directory and see if any locked files
# are ones we are interested in.

chdir $pwd;

# fo all of the file names (remember $files[0] is the directory name
foreach $i (@files[1..$#files])
{
  if ( defined $locks{$i . $ext} )
  { # well the file has at least one lock outstanding

     # find the base version number of our file
     &parse_cvs_entry($i,*entry);

     # is our version of this file locked?
     if ( defined $locks{$i . $ext . $entry{"version"}} )
     { # if so, it is by us?
	if ( $login ne ($by = $locks{$i . $ext . $entry{"version"}}) )
	{# crud somebody else has it locked.
	   $outstanding_lock++ ;
	   print "$by has file $i locked for version " , $entry{"version"};
	}
	else
	{ # yeah I have it locked.
	   print "You have a lock on file $i for version " , $entry{"version"}
		if defined $opt_v;
	}
     }
  }
}

exit $outstanding_lock;


### End of main program

sub parse_cvs_entry
{ # a very simple minded hack at parsing an entries file.
local ( $file, *entry ) = @_;
local ( @pp );


open(ENTRIES, "< CVS/Entries") || die "Can't open entries file";

while (<ENTRIES>)
 {
  if ( $_  =~ /^\/$file\// )
  {
	@pp = split('/');

	$entry{"name"} = $pp[1];
	$entry{"version"} = $pp[2];
	$entry{"dates"} = $pp[3];
	$entry{"name"} = $pp[4];
	$entry{"name"} = $pp[5];
	$entry{"sticky"} = $pp[6];
	return;
  }
 }
}
