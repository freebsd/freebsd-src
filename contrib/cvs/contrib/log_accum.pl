#! xPERL_PATHx
# -*-Perl-*-
#
#ident	"@(#)ccvs/contrib:$Name:  $:$Id: log_accum.pl,v 1.4 1996/03/06 15:27:09 woods Exp $"
#
# Perl filter to handle the log messages from the checkin of files in
# a directory.  This script will group the lists of files by log
# message, and mail a single consolidated log message at the end of
# the commit.
#
# This file assumes a pre-commit checking program that leaves the
# names of the first and last commit directories in a temporary file.
#
# Contributed by David Hampton <hampton@cisco.com>
#
# hacked greatly by Greg A. Woods <woods@planix.com>

# Usage: log_accum.pl [-d] [-s] [-M module] [[-m mailto] ...] [[-R replyto] ...] [-f logfile]
#	-d		- turn on debugging
#	-m mailto	- send mail to "mailto" (multiple)
#	-R replyto	- set the "Reply-To:" to "replyto" (multiple)
#	-M modulename	- set module name to "modulename"
#	-f logfile	- write commit messages to logfile too
#	-s		- *don't* run "cvs status -v" for each file

#
#	Configurable options
#

# set this to something that takes a whole message on stdin
$MAILER	       = "/usr/lib/sendmail -t";

#
#	End user configurable options.
#

# Constants (don't change these!)
#
$STATE_NONE    = 0;
$STATE_CHANGED = 1;
$STATE_ADDED   = 2;
$STATE_REMOVED = 3;
$STATE_LOG     = 4;

$LAST_FILE     = "/tmp/#cvs.lastdir";

$CHANGED_FILE  = "/tmp/#cvs.files.changed";
$ADDED_FILE    = "/tmp/#cvs.files.added";
$REMOVED_FILE  = "/tmp/#cvs.files.removed";
$LOG_FILE      = "/tmp/#cvs.files.log";

$FILE_PREFIX   = "#cvs.files";

#
#	Subroutines
#

sub cleanup_tmpfiles {
    local($wd, @files);

    $wd = `pwd`;
    chdir("/tmp") || die("Can't chdir('/tmp')\n");
    opendir(DIR, ".");
    push(@files, grep(/^$FILE_PREFIX\..*\.$id$/, readdir(DIR)));
    closedir(DIR);
    foreach (@files) {
	unlink $_;
    }
    unlink $LAST_FILE . "." . $id;

    chdir($wd);
}

sub write_logfile {
    local($filename, @lines) = @_;

    open(FILE, ">$filename") || die("Cannot open log file $filename.\n");
    print FILE join("\n", @lines), "\n";
    close(FILE);
}

sub append_to_logfile {
    local($filename, @lines) = @_;

    open(FILE, ">$filename") || die("Cannot open log file $filename.\n");
    print FILE join("\n", @lines), "\n";
    close(FILE);
}

sub format_names {
    local($dir, @files) = @_;
    local(@lines);

    $format = "\t%-" . sprintf("%d", length($dir)) . "s%s ";

    $lines[0] = sprintf($format, $dir, ":");

    if ($debug) {
	print STDERR "format_names(): dir = ", $dir, "; files = ", join(":", @files), ".\n";
    }
    foreach $file (@files) {
	if (length($lines[$#lines]) + length($file) > 65) {
	    $lines[++$#lines] = sprintf($format, " ", " ");
	}
	$lines[$#lines] .= $file . " ";
    }

    @lines;
}

sub format_lists {
    local(@lines) = @_;
    local(@text, @files, $lastdir);

    if ($debug) {
	print STDERR "format_lists(): ", join(":", @lines), "\n";
    }
    @text = ();
    @files = ();
    $lastdir = shift @lines;	# first thing is always a directory
    if ($lastdir !~ /.*\/$/) {
	die("Damn, $lastdir doesn't look like a directory!\n");
    }
    foreach $line (@lines) {
	if ($line =~ /.*\/$/) {
	    push(@text, &format_names($lastdir, @files));
	    $lastdir = $line;
	    @files = ();
	} else {
	    push(@files, $line);
	}
    }
    push(@text, &format_names($lastdir, @files));

    @text;
}

sub append_names_to_file {
    local($filename, $dir, @files) = @_;

    if (@files) {
	open(FILE, ">>$filename") || die("Cannot open file $filename.\n");
	print FILE $dir, "\n";
	print FILE join("\n", @files), "\n";
	close(FILE);
    }
}

sub read_line {
    local($line);
    local($filename) = @_;

    open(FILE, "<$filename") || die("Cannot open file $filename.\n");
    $line = <FILE>;
    close(FILE);
    chop($line);
    $line;
}

sub read_logfile {
    local(@text);
    local($filename, $leader) = @_;

    open(FILE, "<$filename");
    while (<FILE>) {
	chop;
	push(@text, $leader.$_);
    }
    close(FILE);
    @text;
}

sub build_header {
    local($header);
    local($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
    $header = sprintf("CVSROOT:\t%s\nModule name:\t%s\nChanges by:\t%s@%s\t%02d/%02d/%02d %02d:%02d:%02d",
		      $cvsroot,
		      $modulename,
		      $login, $hostdomain,
		      $year%100, $mon+1, $mday,
		      $hour, $min, $sec);
}

sub mail_notification {
    local(@text) = @_;

    # if only we had strftime()...  stuff stolen from perl's ctime.pl:
    local($[) = 0;

    @DoW = ('Sun','Mon','Tue','Wed','Thu','Fri','Sat');
    @MoY = ('Jan','Feb','Mar','Apr','May','Jun',
	    'Jul','Aug','Sep','Oct','Nov','Dec');

    # Determine what time zone is in effect.
    # Use GMT if TZ is defined as null, local time if TZ undefined.
    # There's no portable way to find the system default timezone.
    #
    $TZ = defined($ENV{'TZ'}) ? ( $ENV{'TZ'} ? $ENV{'TZ'} : 'GMT' ) : '';

    # Hack to deal with 'PST8PDT' format of TZ
    # Note that this can't deal with all the esoteric forms, but it
    # does recognize the most common: [:]STDoff[DST[off][,rule]]
    #
    if ($TZ =~ /^([^:\d+\-,]{3,})([+-]?\d{1,2}(:\d{1,2}){0,2})([^\d+\-,]{3,})?/) {
        $TZ = $isdst ? $4 : $1;
	$tzoff = sprintf("%05d", -($2) * 100);
    }

    # perl-4.036 doesn't have the $zone or $gmtoff...
    ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst, $zone, $gmtoff) =
        ($TZ eq 'GMT') ? gmtime(time) : localtime(time);

    $year += ($year < 70) ? 2000 : 1900;

    if ($gmtoff != 0) {
	$tzoff = sprintf("%05d", ($gmtoff / 60) * 100);
    }
    if ($zone ne '') {
	$TZ = $zone;
    }

    # ok, let's try....
    $rfc822date = sprintf("%s, %2d %s %4d %2d:%02d:%02d %s (%s)",
			  $DoW[$wday], $mday, $MoY[$mon], $year,
			  $hour, $min, $sec, $tzoff, $TZ);

    open(MAIL, "| $MAILER");
    print MAIL "Date:     " . $rfc822date . "\n";
    print MAIL "Subject:  CVS Update: " . $modulename . "\n";
    print MAIL "To:       " . $mailto . "\n";
    print MAIL "From:     " . $login . "@" . $hostdomain . "\n";
    print MAIL "Reply-To: " . $replyto . "\n";
    print MAIL "\n";
    print MAIL join("\n", @text), "\n";
    close(MAIL);
}

sub write_commitlog {
    local($logfile, @text) = @_;

    open(FILE, ">>$logfile");
    print FILE join("\n", @text), "\n";
    close(FILE);
}

#
#	Main Body
#

# Initialize basic variables
#
$debug = 0;
$id = getpgrp();		# note, you *must* use a shell which does setpgrp()
$state = $STATE_NONE;
$login = getlogin || (getpwuid($<))[0] || "nobody";
chop($hostname = `hostname`);
chop($domainname = `domainname`);
$hostdomain = $hostname . $domainname;
$cvsroot = $ENV{'CVSROOT'};
$do_status = 1;
$modulename = "";

# parse command line arguments (file list is seen as one arg)
#
while (@ARGV) {
    $arg = shift @ARGV;

    if ($arg eq '-d') {
	$debug = 1;
	print STDERR "Debug turned on...\n";
    } elsif ($arg eq '-m') {
	if ($mailto eq '') {
	    $mailto = shift @ARGV;
	} else {
	    $mailto = $mailto . ", " . shift @ARGV;
	}
    } elsif ($arg eq '-R') {
	if ($replyto eq '') {
	    $replyto = shift @ARGV;
	} else {
	    $replyto = $replyto . ", " . shift @ARGV;
	}
    } elsif ($arg eq '-M') {
	$modulename = shift @ARGV;
    } elsif ($arg eq '-s') {
	$do_status = 0;
    } elsif ($arg eq '-f') {
	($commitlog) && die("Too many '-f' args\n");
	$commitlog = shift @ARGV;
    } else {
	($donefiles) && die("Too many arguments!  Check usage.\n");
	$donefiles = 1;
	@files = split(/ /, $arg);
    }
}
($mailto) || die("No mail recipient specified (use -m)\n");
if ($replyto eq '') {
    $replyto = $login;
}

# for now, the first "file" is the repository directory being committed,
# relative to the $CVSROOT location
#
@path = split('/', $files[0]);

# XXX there are some ugly assumptions in here about module names and
# XXX directories relative to the $CVSROOT location -- really should
# XXX read $CVSROOT/CVSROOT/modules, but that's not so easy to do, since
# XXX we have to parse it backwards.
#
if ($modulename eq "") {
    $modulename = $path[0];	# I.e. the module name == top-level dir
}
if ($#path == 0) {
    $dir = ".";
} else {
    $dir = join('/', @path);
}
$dir = $dir . "/";

if ($debug) {
    print STDERR "module - ", $modulename, "\n";
    print STDERR "dir    - ", $dir, "\n";
    print STDERR "path   - ", join(":", @path), "\n";
    print STDERR "files  - ", join(":", @files), "\n";
    print STDERR "id     - ", $id, "\n";
}

# Check for a new directory first.  This appears with files set as follows:
#
#    files[0] - "path/name/newdir"
#    files[1] - "-"
#    files[2] - "New"
#    files[3] - "directory"
#
if ($files[2] =~ /New/ && $files[3] =~ /directory/) {
    local(@text);

    @text = ();
    push(@text, &build_header());
    push(@text, "");
    push(@text, $files[0]);
    push(@text, "");

    while (<STDIN>) {
	chop;			# Drop the newline
	push(@text, $_);
    }

    &mail_notification($mailto, @text);

    exit 0;
}

# Check for an import command.  This appears with files set as follows:
#
#    files[0] - "path/name"
#    files[1] - "-"
#    files[2] - "Imported"
#    files[3] - "sources"
#
if ($files[2] =~ /Imported/ && $files[3] =~ /sources/) {
    local(@text);

    @text = ();
    push(@text, &build_header());
    push(@text, "");
    push(@text, $files[0]);
    push(@text, "");

    while (<STDIN>) {
	chop;			# Drop the newline
	push(@text, $_);
    }

    &mail_notification(@text);

    exit 0;
}

# Iterate over the body of the message collecting information.
#
while (<STDIN>) {
    chop;			# Drop the newline

    if (/^In directory/) {
	push(@log_lines, $_);
	push(@log_lines, "");
	next;
    }

    if (/^Modified Files/) { $state = $STATE_CHANGED; next; }
    if (/^Added Files/)    { $state = $STATE_ADDED;   next; }
    if (/^Removed Files/)  { $state = $STATE_REMOVED; next; }
    if (/^Log Message/)    { $state = $STATE_LOG;     next; }

    s/^[ \t\n]+//;		# delete leading whitespace
    s/[ \t\n]+$//;		# delete trailing whitespace
    
    if ($state == $STATE_CHANGED) { push(@changed_files, split); }
    if ($state == $STATE_ADDED)   { push(@added_files,   split); }
    if ($state == $STATE_REMOVED) { push(@removed_files, split); }
    if ($state == $STATE_LOG)     { push(@log_lines,     $_); }
}

# Strip leading and trailing blank lines from the log message.  Also
# compress multiple blank lines in the body of the message down to a
# single blank line.
#
while ($#log_lines > -1) {
    last if ($log_lines[0] ne "");
    shift(@log_lines);
}
while ($#log_lines > -1) {
    last if ($log_lines[$#log_lines] ne "");
    pop(@log_lines);
}
for ($i = $#log_lines; $i > 0; $i--) {
    if (($log_lines[$i - 1] eq "") && ($log_lines[$i] eq "")) {
	splice(@log_lines, $i, 1);
    }
}

if ($debug) {
    print STDERR "Searching for log file index...";
}
# Find an index to a log file that matches this log message
#
for ($i = 0; ; $i++) {
    local(@text);

    last if (! -e "$LOG_FILE.$i.$id"); # the next available one
    @text = &read_logfile("$LOG_FILE.$i.$id", "");
    last if ($#text == -1);	# nothing in this file, use it
    last if (join(" ", @log_lines) eq join(" ", @text)); # it's the same log message as another
}
if ($debug) {
    print STDERR " found log file at $i.$id, now writing tmp files.\n";
}

# Spit out the information gathered in this pass.
#
&append_names_to_file("$CHANGED_FILE.$i.$id", $dir, @changed_files);
&append_names_to_file("$ADDED_FILE.$i.$id",   $dir, @added_files);
&append_names_to_file("$REMOVED_FILE.$i.$id", $dir, @removed_files);
&write_logfile("$LOG_FILE.$i.$id", @log_lines);

# Check whether this is the last directory.  If not, quit.
#
if ($debug) {
    print STDERR "Checking current dir against last dir.\n";
}
$_ = &read_line("$LAST_FILE.$id");

if ($_ ne $cvsroot . "/" . $files[0]) {
    if ($debug) {
	print STDERR sprintf("Current directory %s is not last directory %s.\n", $cvsroot . "/" .$files[0], $_);
    }
    exit 0;
}
if ($debug) {
    print STDERR sprintf("Current directory %s is last directory %s -- all commits done.\n", $files[0], $_);
}

#
#	End Of Commits!
#

# This is it.  The commits are all finished.  Lump everything together
# into a single message, fire a copy off to the mailing list, and drop
# it on the end of the Changes file.
#

#
# Produce the final compilation of the log messages
#
@text = ();
@status_txt = ();
push(@text, &build_header());
push(@text, "");

for ($i = 0; ; $i++) {
    last if (! -e "$LOG_FILE.$i.$id"); # we're done them all!
    @lines = &read_logfile("$CHANGED_FILE.$i.$id", "");
    if ($#lines >= 0) {
	push(@text, "Modified files:");
	push(@text, &format_lists(@lines));
    }
    @lines = &read_logfile("$ADDED_FILE.$i.$id", "");
    if ($#lines >= 0) {
	push(@text, "Added files:");
	push(@text, &format_lists(@lines));
    }
    @lines = &read_logfile("$REMOVED_FILE.$i.$id", "");
    if ($#lines >= 0) {
	push(@text, "Removed files:");
	push(@text, &format_lists(@lines));
    }
    if ($#text >= 0) {
	push(@text, "");
    }
    @lines = &read_logfile("$LOG_FILE.$i.$id", "\t");
    if ($#lines >= 0) {
	push(@text, "Log message:");
	push(@text, @lines);
	push(@text, "");
    }
    if ($do_status) {
	local(@changed_files);

	@changed_files = ();
	push(@changed_files, &read_logfile("$CHANGED_FILE.$i.$id", ""));
	push(@changed_files, &read_logfile("$ADDED_FILE.$i.$id", ""));
	push(@changed_files, &read_logfile("$REMOVED_FILE.$i.$id", ""));

	if ($debug) {
	    print STDERR "main: pre-sort changed_files = ", join(":", @changed_files), ".\n";
	}
	sort(@changed_files);
	if ($debug) {
	    print STDERR "main: post-sort changed_files = ", join(":", @changed_files), ".\n";
	}

	foreach $dofile (@changed_files) {
	    if ($dofile =~ /\/$/) {
		next;		# ignore the silly "dir" entries
	    }
	    if ($debug) {
		print STDERR "main(): doing 'cvs -nQq status -v $dofile'\n";
	    }
	    open(STATUS, "-|") || exec 'cvs', '-nQq', 'status', '-v', $dofile;
	    while (<STATUS>) {
		chop;
		push(@status_txt, $_);
	    }
	}
    }
}

# Write to the commitlog file
#
if ($commitlog) {
    &write_commitlog($commitlog, @text);
}

if ($#status_txt >= 0) {
    push(@text, @status_txt);
}

# Mailout the notification.
#
&mail_notification(@text);

# cleanup
#
if (! $debug) {
    &cleanup_tmpfiles();
}

exit 0;
