#! /usr/bin/env perl
#
# mfc - perl script to generate patchsets from commit mail or message-id.
#
# Copyright (c) 2006 Florent Thoumie <flz@FreeBSD.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

# This perl scripts only uses programs that are part of the base system.
# Since some people use NO_FOO options, here's the list of used programs :
#  - cvs
#  - fetch
#  - perl (with getopt module)
#  - mkdir, cat, chmod, grep (hopefully everybody has them)
#  - cdiff or colordiff (optional)
#
#  This script is using 3 environment variables :
#  - MFCHOME: directory where patches, scripts and commit message will be stored.
#  - MFCCVSROOT: alternative CVSROOT used to generate diffs for new/dead files.
#  - MFCLOGIN: define this to your freefall login if you have commit rights.
#
# TODO: Look for XXX in the file.
#

use strict;
use warnings;

use Env;
use Env qw(MFCHOME MFCLOGIN MFCCVSROOT);
use Getopt::Std;

my $mfchome = $MFCHOME ? $MFCHOME : "/var/tmp/mfc";
my $mfclogin = $MFCLOGIN ? $MFCLOGIN : "";
my $cvsroot = $MFCCVSROOT ? $MFCCVSROOT : ':pserver:anoncvs@anoncvs.at.FreeBSD.org:/home/ncvs';

my $version = "1.0.1";
my %opt;
my $commit_author;
my $commit_date;
my %mfc_files = ( );
my %new_files = ( );
my %dead_files = ( );
my @msgids = ( );
my @logmsg = ( );
my @commitmail = ( );
my $commiturl;
my @prs;
my @submitted_by;
my @reviewed_by;
my @obtained_from;
my $cdiff;
my $answer;
my $mfc_func = \&mfc_headers;

my $first_log_line = 1;

sub init()
{
	# Look for pre-requisites.
	my @reqs = ( "fetch", "cvs", "mkdir", "cat", "chmod", "grep" );
	my $cmd;
	foreach (@reqs) {
		$cmd = `which $_`;
		die "$_ is missing. Please check pre-requisites." if ($cmd =~ /^$/);
	}
	$cdiff = `which cdiff`;
	$cdiff = `which colordiff` if ($cdiff =~ /^$/);

	# Parse command-line options.
	my $opt_string = 'f:hi:m:s:v';
	getopts( "$opt_string", \%opt ) or usage();
	usage() if !$opt{i} or $opt{h};
	@msgids = split / /, $opt{m} if (defined($opt{m}));
}

sub usage()
{
	print STDERR << "EOF";
$0 version $version 

Usage: $0 [-v] -h
       $0 [-v] -f file -i id
       $0 [-v] -m msg-id -i id
       $0 [-v] -s query -i id
Options:
  -f file    : commit mail file to use ('-' for stdin)
  -h         : this (help) message
  -i id      : identifier used to save commit log message and patch
  -m msg-id  : message-id referring to the original commit (you can use more than one)
  -s query   : search commit mail archives (a filename with his revision is a good search)
  -v         : be a little more verbose
Examples:
  $0 -m 200601081417.k08EH4EN027418 -i uscanner
  $0 -s "param.h 1.41" -i move_acpi
  $0 -f commit.txt -i id
  $0 -m "200601081417.k08EH4EN027418 200601110806.k0B86m9C054798" -i consecutive

Please report bugs to: Florent Thoumie <flz\@FreeBSD.org>
EOF
	exit 1;
}

sub previous_revision($)
{
	my ($rev) = @_;
	my @rev;

	# XXX - I'm not sure this is working as it should.
	return 0 if ($rev =~ /^1\.1$/);
	@rev = split '\.', $rev;
	return undef unless @rev;
	if (($#rev % 2) != 1) {
		pop @rev;
		return join ".", @rev;
	}
	if ($rev[-1] == 1) {
		pop @rev;
		return &previous_revision(join ".", @rev);
	} else {
		$rev[-1]--;
		return join ".", @rev;
	}
}

sub fetch_mail($)
{
	my $msgid = $_[0];
	my @years = ( "current", "2006", "2005", "2004", "2003", "2002", "2001", "2000", "1999", "1998", "1997", "1996", "1995", "1994" );
	my $url = "";

	$msgid =~ s/<//;
	$msgid =~ s/>//;
	$msgid =~ s/@.*//;

	# XXX - This should go away once my mid.cgi patches hits the doc tree.
	foreach (@years) {
		$url = `fetch -q -o - 'http://www.freebsd.org/cgi/mid.cgi?id=$msgid+$_/cvs-all&db=mid' | grep getmsg.cgi | head -n 1`;
		last if (!($url =~ /^$/));
	}
	if ($url =~ /^$/) {
		print "No mail found for Message-Id <$msgid>.\n";
		exit 1;
	}
	$url =~ s/.*HREF="(.*)".*/$1+raw/;
	$url =~ s/.*href="(.*)".*/$1/;
	$url =~ s/^.*\/cgi/http:\/\/www.freebsd.org\/cgi/;
	return $url;
}

sub search_mail($)
{
	my $query = $_[0];

	$query =~ s/\s+/+/g;

	# XXX - I guess we could take 5 first results instead of just the first
	# but it has been working correctly for each search I've made so ...
	my $result = `fetch -q -o - 'http://www.freebsd.org/cgi/search.cgi?words=$query&max=1&sort=score&index=recent&source=cvs-all' | grep getmsg.cgi`;

	$result =~ s/.*href="(.*)">.*/http:\/\/www.freebsd.org\/cgi\/$1+raw/;
	if ($result =~ /^$/) {
		print "No commit mail found for '$query'.\n";
		exit 1;
	}
	return $result;
}

sub fetch_diff($)
{
	my $name = $_[0];
	my $old = $mfc_files{$name}{"from"};
	my $new = $mfc_files{$name}{"to"};

	# CVSWeb uses rcsdiff instead of cvs rdiff, that's a problem for deleted and new files.
	if (exists($new_files{$name}) or exists($dead_files{$name})) {
		print "    Generating diff for $name using cvs rdiff...\n";
		system("cvs -d $cvsroot rdiff -u -r$old -r$new $name >> $mfchome/$opt{i}/patch 2>/dev/null");
	} else {
		print "    Fetching diff for $name from cvsweb.freebsd.org...\n";
		system("fetch -q -o - \"http://www.freebsd.org/cgi/cvsweb.cgi/$name.diff?r1=$old&r2=$new\" >> $mfchome/$opt{i}/patch");
	}
}

sub mfc_headers($)
{
	if ($_[0] =~ /^$/) {
		$mfc_func = \&mfc_author;
	} elsif ($_[0] =~ /^(\w+)\s+(\S+\s\S+\s\S+)$/) {
		# Skipped headers (probably a copy/paste from sobomax MFC reminder).
		mfc_author($_[0]);
	} else {
		if ($_[0] =~ /^Message-Id:\s*(\S+)$/ and ($opt{v} or $opt{s})) {
			print "Message-Id is $1.\n";
		}
	}
}

sub mfc_author($)
{
	if (!($_[0] =~ /^(\w+)\s+(\S+\s\S+\s\S+)$/)) {
		die "Can't determine commit author and date.";
	}
	$commit_author = $1;
	$commit_date = $2;	

	print "Committed by $commit_author on $commit_date.\n";

	$mfc_func = \&mfc_modified_files;
}

sub mfc_modified_files($)
{
	if ($_[0] =~ /^\s+Log:/) {
		$mfc_func = \&mfc_log;
	} else {
		# Nothing
	}
}

sub mfc_log($)
{
	if ($_[0] =~ /^\s*Revision\s+Changes\s+Path\s*$/) {
		$mfc_func = \&mfc_revisions;
	} else {
		push(@logmsg, $_[0]);
	}
}

sub mfc_revisions($)
{
	my $name;
	my $rev;
	my $prev;

	return if ($_[0] =~ /^$/);
	if (!($_[0] =~ /^\s+(\S+)\s+\S+\s+\S+\s+(\S+)/)) {
		# Probably two consecutive cut/paste commit mails.
		$mfc_func = \&mfc_headers;
		mfc_headers($_[0]);
		return;
	} else {
		$_[0] =~ /\s+(\S+)\s+\S+\s+\S+\s+(\S+)/;
		$name = $2;
		$rev = $1;

		$new_files{$name} = undef if ($_[0] =~ /\(new\)$/);
		$dead_files{$name} = undef if ($_[0] =~ /\(dead\)$/);
		
		if (defined($mfc_files{$name}{"from"})) {
			$prev = previous_revision($rev);
			if ($mfc_files{$name}{"to"} =~ /^$prev$/) {
				$mfc_files{$name}{"to"} = $rev;
			} else {
				die "Non-consecutive revisions found for $name.";
			}
		} else {
			$mfc_files{$name}{"to"} = $rev;
			$mfc_files{$name}{"from"} = previous_revision($rev);
		}
	}
}

sub strip_log(@) {
	my $tmp;

	while ($logmsg[$#logmsg] =~ /^\s*$/ or $logmsg[$#logmsg] =~ /^\s\s\w+(\s\w+)*:\s+\w+(\s+\w+)*/) {
		$tmp = pop(@logmsg);
		$tmp =~ s/^\s*//;
		chomp($tmp);
		if ($tmp =~ /^PR:\s+(.*)/) {
			push(@prs, $1);
		}
		if ($tmp =~ /^Submitted by:\s+(.*)/) {
			push(@submitted_by, $1);
		}
		if ($tmp =~ /^Reviewed by:\s+(.*)/) {
			push(@reviewed_by, $1);
		}
		if ($tmp =~ /^Obtained from:\s+(.*)/) {
			push(@obtained_from, $1);
		}
	}
}

sub print_epilog {
	my $tmp;

	if ($#prs >= 0) {
		$tmp = join(", ", @prs);
		chomp($tmp);
		print MSG "PR:\t\t$tmp\n";
	}
	if ($#submitted_by >= 0) {
		$tmp = join(", ", @submitted_by);
		chomp($tmp);
		print MSG "Submitted by:\t$tmp\n";
	}
	if ($#reviewed_by >= 0) {
		$tmp = join(", ", @reviewed_by);
		chomp($tmp);
		print MSG "Reviewed by:\t$tmp\n";
	}
	if ($#obtained_from >= 0) {
		$tmp = join(", ", @obtained_from);
		chomp($tmp);
		print MSG "Obtained from:\t$tmp\n";
	}
}

init();

if ($opt{s}) {
	print "Searching commit mail on www.freebsd.org...\n";
	$commiturl = search_mail($opt{s});
	print "Fetching commit mail from www.freebsd.org...\n";
	@commitmail = `fetch -q -o - $commiturl`;
	$mfc_func->($_) foreach (@commitmail);
	strip_log(@logmsg);
} elsif ($opt{f}) {
	open MAIL, $opt{f} || die "Can't open $opt{f} for reading.";
	@commitmail = <MAIL>;	
	close MAIL;
	$mfc_func->($_) foreach (@commitmail);
	strip_log(@logmsg);
} else { # $opt{m}
	foreach (@msgids) {
		print "Fetching commit mail from www.freebsd.org...\n";
		$commiturl = fetch_mail($_);
		@commitmail = `fetch -q -o - $commiturl`;
		$mfc_func->($_) foreach (@commitmail);
		strip_log(@logmsg);
	}
}

die "Doesn't seem you gave me a real commit mail." if ($mfc_func == \&mfc_headers);
die "No file affected by commit?" if (scalar(keys(%mfc_files)) == 0);

# Create directory and truncate patch file.
system("mkdir -p $mfchome/$opt{i}");
system("cat /dev/null > $mfchome/$opt{i}/patch");

if ($opt{v} or $opt{s}) {
	# Print files touched by commit(s).
	print "Files touched by commit(s):\n";
	print "    ", $_, ": rev ", $mfc_files{$_}{"from"}, " -> ", $mfc_files{$_}{"to"}, "\n" foreach (keys(%mfc_files));
}

if ($opt{s}) {
	print "Is it the commit you were looking for ? [Yn] ";
	$answer = <STDIN>;
	chomp($answer);
	if ($answer =~ /^[Nn]$/) {
		print "Sorry that I couldn't help you.\n";
		exit 0;
	}
}

# Generating patch.
print "Processing patch...\n";
fetch_diff($_) foreach (keys(%mfc_files));

if ($mfclogin) {
	# Create commit message from previous commit message.
	print "Processing commit message...\n";
	# Chop empty lines Template lines like "Approved by: (might be dangerous)".
	open MSG, "> $mfchome/$opt{i}/msg" || die "Can't open $mfchome/$opt{i}/msg for writing.";
	print MSG "MFC:\n\n";
	
	# Append merged file names and revisions to the commit message.
	print MSG $_ foreach (@logmsg);
	print MSG "\n";
	print MSG "      ", $_, ": rev ", $mfc_files{$_}{"from"}, " -> ", $mfc_files{$_}{"to"}, "\n" foreach (keys(%mfc_files));

	# Append useful info gathered from Submitted/Obtained/... lines.
	print MSG "\n";
	print_epilog();
	close MSG;

	# Create commit script.
	print "Processing commit script...\n";
	open SCRIPT, "> $mfchome/$opt{i}/script" || die "Can't open $mfchome/$opt{i}/script for writing.";
	print SCRIPT "#! /bin/sh\n\n";
	print SCRIPT "# This script has been automatically generated by $0.\n\n";
	print SCRIPT "export CVSROOT=\"$mfclogin\@ncvs.freebsd.org:/home/ncvs\"\n\n";

	if (scalar(keys(%new_files)) or scalar(keys(%dead_files))) {
		if (scalar(keys(%new_files))) {
			print SCRIPT "cvs add";
			print SCRIPT " \\\n  $_" foreach (keys(%new_files));
			print SCRIPT "\n";
		}
		if (scalar(keys(%dead_files))) {
			print SCRIPT "cvs rm -f";
			print SCRIPT " \\\n  $_" foreach (keys(%dead_files));
			print SCRIPT "\n";
		}
	}

	print SCRIPT "cvs diff";
	print SCRIPT " \\\n  $_" foreach (keys(%mfc_files));
	if ($cdiff =~ /^$/) {
		print SCRIPT "\n";
	} else {
		print SCRIPT " | $cdiff";
	}

	print SCRIPT "cvs ci";
	print SCRIPT " \\\n  $_" foreach (keys(%mfc_files));
	print SCRIPT "\n";

	close SCRIPT;
	system("chmod a+x $mfchome/$opt{i}/script");
}

print "Done, output directory is $mfchome/$opt{i}/\n";

exit 0;
