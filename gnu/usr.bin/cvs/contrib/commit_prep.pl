#! xPERL_PATHx
# -*-Perl-*-
#
#ident	"@(#)cvs/contrib:$Name:  $:$Id: commit_prep.pl,v 1.2 1995/07/10 02:01:29 kfogel Exp $"
#
# Perl filter to handle pre-commit checking of files.  This program
# records the last directory where commits will be taking place for
# use by the log_accum.pl script.  For new files, it forces the
# existence of a RCS "Id" keyword in the first ten lines of the file.
# For existing files, it checks version number in the "Id" line to
# prevent losing changes because an old version of a file was copied
# into the direcory.
#
# Possible future enhancements:
#
#    Check for cruft left by unresolved conflicts.  Search for
#    "^<<<<<<<$", "^-------$", and "^>>>>>>>$".
#
#    Look for a copyright and automagically update it to the
#    current year.  [[ bad idea!  -- woods ]]
#
#
# Contributed by David Hampton <hampton@cisco.com>
#
# Hacked on lots by Greg A. Woods <woods@web.net>

#
#	Configurable options
#

# Constants (remember to protect strings from RCS keyword substitution)
#
$LAST_FILE     = "/tmp/#cvs.lastdir"; # must match name in log_accum.pl
$ENTRIES       = "CVS/Entries";

# Patterns to find $Log keywords in files
#
$LogString1 = "\\\$\\Log: .* \\\$";
$LogString2 = "\\\$\\Log\\\$";
$NoLog = "%s - contains an RCS \$Log keyword.  It must not!\n";

# pattern to match an RCS Id keyword line with an existing ID
#
$IDstring = "\"@\\(#\\)[^:]*:.*\\\$\Id: .*\\\$\"";
$NoId = "
%s - Does not contain a properly formatted line with the keyword \"Id:\".
	I.e. no lines match \"" . $IDstring . "\".
	Please see the template files for an example.\n";

# pattern to match an RCS Id keyword line for a new file (i.e. un-expanded)
#
$NewId = "\"@(#)[^:]*:.*\\$\Id\\$\"";

$NoName = "
%s - The ID line should contain only \"@(#)module/path:\$Name\$:\$\Id\$\"
	for a newly created file.\n";

$BadName = "
%s - The file name '%s' in the ID line does not match
	the actual filename.\n";

$BadVersion = "
%s - How dare you!!!  You replaced your copy of the file '%s',
	which was based upon version %s, with an %s version based
	upon %s.  Please move your '%s' out of the way, perform an
	update to get the current version, and them merge your changes
	into that file, then try the commit again.\n";

#
#	Subroutines
#

sub write_line {
    local($filename, $line) = @_;
    open(FILE, ">$filename") || die("Cannot open $filename, stopped");
    print(FILE $line, "\n");
    close(FILE);
}

sub check_version {
    local($i, $id, $rname, $version);
    local($filename, $cvsversion) = @_;

    open(FILE, "<$filename") || return(0);

    @all_lines = ();
    $idpos = -1;
    $newidpos = -1;
    for ($i = 0; <FILE>; $i++) {
	chop;
	push(@all_lines, $_);
	if ($_ =~ /$IDstring/) {
	    $idpos = $i;
	}
	if ($_ =~ /$NewId/) {
	    $newidpos = $i;
	}
    }

    if (grep(/$LogString1/, @all_lines) || grep(/$LogString2/, @all_lines)) {
	print STDERR sprintf($NoLog, $filename);
	return(1);
    }

    if ($debug != 0) {
	print STDERR sprintf("file = %s, version = %d.\n", $filename, $cvsversion{$filename});
    }

    if ($cvsversion{$filename} == 0) {
	if ($newidpos != -1 && $all_lines[$newidpos] !~ /$NewId/) {
	    print STDERR sprintf($NoName, $filename);
	    return(1);
	}
	return(0);
    }

    if ($idpos == -1) {
	print STDERR sprintf($NoId, $filename);
	return(1);
    }

    $line = $all_lines[$idpos];
    $pos = index($line, "Id: ");
    if ($debug != 0) {
	print STDERR sprintf("%d in '%s'.\n", $pos, $line);
    }
    ($id, $rname, $version) = split(' ', substr($line, $pos));
    if ($rname ne "$filename,v") {
	print STDERR sprintf($BadName, $filename, substr($rname, 0, length($rname)-2));
	return(1);
    }
    if ($cvsversion{$filename} < $version) {
	print STDERR sprintf($BadVersion, $filename, $filename, $cvsversion{$filename},
			     "newer", $version, $filename);
	return(1);
    }
    if ($cvsversion{$filename} > $version) {
	print STDERR sprintf($BadVersion, $filename, $filename, $cvsversion{$filename},
			     "older", $version, $filename);
	return(1);
    }
    return(0);
}

#
#	Main Body	
#

$id = getpgrp();		# You *must* use a shell that does setpgrp()!

# Check each file (except dot files) for an RCS "Id" keyword.
#
$check_id = 0;

# Record the directory for later use by the log_accumulate stript.
#
$record_directory = 0;

# parse command line arguments
#
while (@ARGV) {
    $arg = shift @ARGV;

    if ($arg eq '-d') {
	$debug = 1;
	print STDERR "Debug turned on...\n";
    } elsif ($arg eq '-c') {
	$check_id = 1;
    } elsif ($arg eq '-r') {
	$record_directory = 1;
    } else {
	push(@files, $arg);
    }
}

$directory = shift @files;

if ($debug != 0) {
    print STDERR "dir   - ", $directory, "\n";
    print STDERR "files - ", join(":", @files), "\n";
    print STDERR "id    - ", $id, "\n";
}

# Suck in the CVS/Entries file
#
open(ENTRIES, $ENTRIES) || die("Cannot open $ENTRIES.\n");
while (<ENTRIES>) {
    local($filename, $version) = split('/', substr($_, 1));
    $cvsversion{$filename} = $version;
}

# Now check each file name passed in, except for dot files.  Dot files
# are considered to be administrative files by this script.
#
if ($check_id != 0) {
    $failed = 0;
    foreach $arg (@files) {
	if (index($arg, ".") == 0) {
	    next;
	}
	$failed += &check_version($arg);
    }
    if ($failed) {
	print STDERR "\n";
	exit(1);
    }
}

# Record this directory as the last one checked.  This will be used
# by the log_accumulate script to determine when it is processing
# the final directory of a multi-directory commit.
#
if ($record_directory != 0) {
    &write_line("$LAST_FILE.$id", $directory);
}
exit(0);
