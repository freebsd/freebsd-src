#!/usr/bin/perl -w
#-
# Copyright (c) 2001 Dag-Erling Coïdan Smørgrav
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#      $FreeBSD$
#

use strict;
use Data::Dumper;
use Fcntl;
use POSIX qw(isatty mktime strftime tzset);
use vars qw($TTY $NOW %MONTH %PR @EVENTS @COUNT @AGE);
use vars qw(%STATE %CATEGORY %OWNER %CLOSER);

%MONTH = (
    'Jan' => 1,
    'Feb' => 2,
    'Mar' => 3,
    'Apr' => 4,
    'May' => 5,
    'Jun' => 6,
    'Jul' => 7,
    'Aug' => 8,
    'Sep' => 9,
    'Oct' => 10,
    'Nov' => 11,
    'Dec' => 12,
);

@AGE = (
    [ 0,	7,	0 ],	# Less than one week
    [ 7,	30,	0 ],	# One week to one month
    [ 30,	90,	0 ],	# One to three months
    [ 90,	365,	0 ],	# Three months to a year
    [ 365,	1095,	0 ],	# One to three years
    [ 1095,	999999,	0 ],	# More than three years
);

sub GNATS_DIR			{ "/home/gnats" }
sub GNATS_TZ			{ "America/Los_Angeles" }
sub DATFILE			{ "/tmp/prstats.dat.$$" }
sub GNUPLOT			{ "|/usr/local/bin/gnuplot /dev/stdin" }
sub TIMEFMT			{ "%Y-%m-%d/%H:%M:%S" }

sub parse_date($) {
    my $date = shift;		# Date to parse

    my $year;
    my $month;
    my $day;
    my $hour;
    my $minute;
    my $second;

    $date =~ s/\s+/ /g;
    $date =~ s/^(Mon|Tue|Wed|Thu|Fri|Sat|Sun)\w*\s*//;
    if ($date =~ m/^(\w{3}) (\d\d?) (\d\d):(\d\d):(\d\d) [A-Z ]*(\d{4})$/) {
	($month, $day, $hour, $minute, $second, $year) =
	    ($1, $2, $3, $4, $5, $6);
    } else {
	die("Unrecognized date format: $date\n");
    }
    defined($month = $MONTH{$month})
	or die("Invalid month: $month\n");
    return mktime($second, $minute, $hour, $day, $month - 1, $year - 1900);
}

sub scan_pr($) {
    my $fn = shift;		# File name

    local *FILE;		# File handle
    my $pr = {};		# PR hash
    my $age;			# PR age

    sysopen(FILE, $fn, O_RDONLY)
	or die("$fn: open(): $!\n");
    while (<FILE>) {
	if (m/^>([A-Za-z-]+):\s+(.*?)\s*$/o ||
	    m/^(Category|Responsible|State-Changed-[A-Za-z-]+):\s+(.*?)\s*$/o) {
	    $pr->{lc($1)} = $2;
	}
    }
    
    exists($PR{$pr->{'number'}})
	and die("$fn: PR $pr->{'number'} already exists\n");

    if ($TTY) {
	print(" "x40, "\r", scalar(keys(%PR)),
	      " $pr->{'category'}/$pr->{'number'} ");
    }

    foreach ('arrival-date', 'closed-date', 'last-modified',
	     'state-changed-when') {
	if (defined($pr->{$_}) && length($pr->{$_})) {
	    $pr->{$_} = parse_date($pr->{$_});
	}
    }

    $pr->{'_created'} = $pr->{'arrival-date'};
    if ($pr->{'state'} eq 'closed') {
	$pr->{'_closed'} = $pr->{'closed-date'} || $pr->{'state-changed-when'};
	$pr->{'_closed_by'} = $pr->{'state-changed-by'};
	++$CLOSER{$pr->{'_closed_by'}};
    } else {
	$age = $pr->{'arrival-date'} / 86400;
	foreach (@AGE) {
	    if ($age >= $_->[0] && $age < $_->[1]) {
		++$_->[2];
		last;
	    }
	}
	++$CATEGORY{$pr->{'category'}};
	++$OWNER{$pr->{'responsible'}};
    }
    ++$STATE{$pr->{'state'}};

    $PR{$pr->{'number'}} = {
  	'category'	=> $pr->{'category'},
  	#'number'	=> $pr->{'number'},
  	'responsible'	=> $pr->{'responsible'},
  	'created'	=> $pr->{'created'},
  	'closed'	=> $pr->{'closed'},
  	'closer'	=> $pr->{'_closed_by'},
    };
    push(@EVENTS, [ $pr->{'_created'}, +1 ]);
    push(@EVENTS, [ $pr->{'_closed'}, -1 ])
	    if defined($pr->{'_closed'});
}

sub scan_recurse($);
sub scan_recurse($) {
    my $dn = shift;		# Directory name

    local *DIR;			# Directory handle
    my $entry;			# Entry
    
    opendir(DIR, $dn)
	or die("$dn: opendir(): $!\n");
    while ($entry = readdir(DIR)) {
	next if ($entry eq '.' || $entry eq '..');
	if (-d "$dn/$entry") {
	    scan_recurse("$dn/$entry");
	} elsif ($entry =~ m/^\d+$/) {
	    eval {
		scan_pr("$dn/$entry");
	    };
	}
    }
    closedir(DIR);
}

sub count_prs() {

    my $pr;			# Iterator
    my @events;			# Creations or closures
    my $event;			# Iterator
    my $count;			# PR count

    if ($TTY) {
	print(int(@EVENTS), " events\n");
    }
    @COUNT = ( [ 0, 0 ] );
    foreach $event (sort({ $a->[0] <=> $b->[0] } @EVENTS)) {
	if ($event->[0] == $COUNT[-1]->[0]) {
	    $COUNT[-1]->[1] += $event->[1];
	} else {
	    push(@COUNT, [ $event->[0], $COUNT[-1]->[1] + $event->[1] ]);
	}
    }
    if (@COUNT > 1) {
	$COUNT[0]->[0] = $COUNT[1]->[0] - 1;
	unshift(@COUNT, [ 0, 0 ]);
    }
}

sub gnuplot(@) {
    my @commands = @_;		# Commands

    my $pid;			# Child PID
    local *PIPE;		# Pipe

    open(PIPE, &GNUPLOT)
	or die("fork(): $!\n");
    print(PIPE join("\n", @commands, ""));
    close(PIPE);
    if ($? & 0x7f) {
        die("gnuplot caught a signal " . ($? & 0x7f) . "\n");
    } elsif ($?) {
        die("gunplot returned exit code " . ($? >> 8) . "\n");
    }
}

sub write_dat_file($) {
    my $fn = shift;		# File name
    
    local *FILE;		# File handle
    my $datum;			# Iterator
    
    sysopen(FILE, $fn, O_RDWR|O_CREAT|O_TRUNC, 0640)
	or die("$fn: open(): $!\n");
    foreach $datum (@COUNT) {
	print(FILE strftime(&TIMEFMT, localtime($datum->[0])),
	      " ", $datum->[1],
	      " ", $COUNT[-1]->[1],
	      "\n");
    }
    close(FILE);
}

sub graph_open_prs($$$$$) {
    my $datfn = shift;		# Data file name
    my $fn = shift;		# File name
    my $start = shift;		# Starting date
    my $end = shift;		# Ending date
    my $title = shift;		# Title

    my $tickfmt;		# Tick format
    my $timefmt;		# Time format

    if ($end - $start > 86400 * 30) {
	$tickfmt = "%Y-%m-%d";
    } else {
	$tickfmt = "%m-%d";
    }
    $start = strftime(&TIMEFMT, localtime($start));
    $end = strftime(&TIMEFMT, localtime($end));
    $timefmt = &TIMEFMT;
    gnuplot("
set term png small color
set xdata time
set timefmt '$timefmt'
set data style line
set grid
set output '$fn'
set format x '$tickfmt'
set xrange ['$start':'$end']
set yrange [0:*]
set title '$title'
plot '$datfn' using 1:2 title 'Open PRs'
");
}

sub pr_stat_summary() {

    my $n;			# Loop counter

    # Overall stats
    printf("Total PRs in database: %d\n", scalar(keys(%PR)));
    printf("Open PRs: %d\n", scalar(keys(%PR)) - $STATE{'closed'});
    print("\n");
    
    # Category ranking
    print("Number of PRs in each category:\n");
    foreach (sort({ $CATEGORY{$b} <=> $CATEGORY{$a} } keys(%CATEGORY))) {
	printf("%12s: %d\n", $_, $CATEGORY{$_});
    }
    print("\n");
    
    # State ranking
    print("Number of PRs in each state:\n");
    foreach (sort({ $STATE{$b} <=> $STATE{$a} } keys(%STATE))) {
	printf("%12s: %d\n", $_, $STATE{$_});
    }
    print("\n");

    # Closer ranking
    print("Top ten PR busters:\n");
    $n = 0;
    foreach (sort({ $CLOSER{$b} <=> $CLOSER{$a} } keys(%CLOSER))) {
	printf("    %2d. %s (%d)\n", ++$n, $_, $CLOSER{$_});
	last if ($n == 10);
    }
    print("\n");
    
    # Owner ranking
    print("Top ten owners of open PRs:\n");
    $n = 0;
    foreach (sort({ $OWNER{$b} <=> $OWNER{$a} } keys(%OWNER))) {
	next if (m/^freebsd-(bugs|doc|ports)$/);
	printf("    %2d. %s (%d)\n", ++$n, $_, $OWNER{$_});
	last if ($n == 10);
    }
    print("\n");
    
}

MAIN:{
    $| = 1;
    $TTY = isatty(*STDOUT);

    # Perl lacks strptime(), and its mktime() doesn't accept a
    # timezone argument, so we set our local timezone to that of the
    # FreeBSD cluster and use localtime() instead.
    $ENV{'TZ'} = &GNATS_TZ;
    tzset();
    $NOW = time();

    # Read and count PRs
    if (@ARGV) {
	foreach (@ARGV) {
	    scan_recurse(join('/', &GNATS_DIR, $_));
	}
    } else {
	scan_recurse(&GNATS_DIR);
    }
    if ($TTY) {
	print("\r", scalar(keys(%PR)), " problem reports scanned\n");
    }

    # Generate graphs
    if (0) {
    count_prs();
    write_dat_file(&DATFILE);
    graph_open_prs(&DATFILE, "week.png", $NOW - (86400 * 7) + 1, $NOW,
		   "Open FreeBSD problem reports (week view)");
    graph_open_prs(&DATFILE, "month.png", $NOW - (86400 * 30) + 1, $NOW,
		   "Open FreeBSD problem reports (month view)");
    graph_open_prs(&DATFILE, "year.png", $NOW - (86400 * 365) + 1, $NOW,
		   "Open FreeBSD problem reports (year view)");
    graph_open_prs(&DATFILE, "ever.png", $COUNT[1]->[0], $NOW,
		   "Open FreeBSD problem reports (project history)");
    graph_open_prs(&DATFILE, "drive.png", mktime(0, 0, 0, 29, 4, 101), $NOW,
		   "Open FreeBSD problem reports (drive progress)");
    unlink(&DATFILE);
    }

    # Print summary
    pr_stat_summary();
}
