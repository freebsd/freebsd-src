#!/usr/local/bin/perl -w
#
#
# Perl filter to handle pre-commit checking of files.  This program
# records the last directory where commits will be taking place for
# use by the log_accumulate script.  For new file, it forcing the
# existence of a RCS "Id" keyword in the first ten lines of the file.
# For existing files, it checks version number in the "Id" line to
# prevent losing changes because an old version of a file was copied
# into the direcory.
#
# Possible future enhancements:
#
#
#    Check for cruft left by unresolved conflicts.  Search for
#    "^<<<<<<<$", "^-------$", and "^>>>>>>>$".
#
#    Look for a copyright and automagically update it to the
#    current year.
#
# Contributed by David Hampton <hampton@cisco.com>
#

############################################################
#
# Configurable options
#
############################################################
#
# Check each file (except dot files) for an RCS "Id" keyword.
#
$check_id = 1;

#
# Record the directory for later use by the log_accumulate stript.
#
$record_directory = 1;

############################################################
#
# Constants
#
############################################################
$LAST_FILE     = "/tmp/#cvs.lastdir";
$ENTRIES       = "CVS/Entries";

$NoId = "
%s - Does not contain a line with the keyword \"Id:\".
    Please see the template files for an example.\n";

# Protect string from substitution by RCS.
$NoName = "
%s - The ID line should contain only \"\$\I\d\:\ \$\" for a newly created file.\n";

$BadName = "
%s - The file name '%s' in the ID line does not match
    the actual filename.\n";

$BadVersion = "
%s - How dare you!!  You replaced your copy of the file '%s',
    which was based upon version %s, with an %s version based
    upon %s.  Please move your '%s' out of the way, perform an
    update to get the current version, and them merge your changes
    into that file.\n";

############################################################
#
# Subroutines
#
############################################################

sub write_line {
    local($filename, $line) = @_;
    open(FILE, ">$filename") || die("Cannot open $filename, stopped");
    print(FILE $line, "\n");
    close(FILE);
}

sub check_version {
    local($i, $id, $rname, $version);
    local($filename, $cvsversion) = @_;

    open(FILE, $filename) || die("Cannot open $filename, stopped");
    for ($i = 1; $i < 10; $i++) {
	$pos = -1;
	last if eof(FILE);
	$line = <FILE>;
	$pos = index($line, "Id: ");
	last if ($pos >= 0);
    }

    if ($pos == -1) {
	printf($NoId, $filename);
	return(1);
    }

    ($id, $rname, $version) = split(' ', substr($line, $pos));
    if ($cvsversion{$filename} == 0) {
	if ($rname ne "\$") {
	    printf($NoName, $filename);
	    return(1);
	}
	return(0);
    }

    if ($rname ne "$filename,v") {
	printf($BadName, $filename, substr($rname, 0, length($rname)-2));
	return(1);
    }
    if ($cvsversion{$filename} < $version) {
	printf($BadVersion, $filename, $filename, $cvsversion{$filename},
	       "newer", $version, $filename);
	return(1);
    }
    if ($cvsversion{$filename} > $version) {
	printf($BadVersion, $filename, $filename, $cvsversion{$filename},
	       "older", $version, $filename);
	return(1);
    }
    return(0);
}

#############################################################
#
# Main Body
#
############################################################

$id = getpgrp();
#print("ARGV - ", join(":", @ARGV), "\n");
#print("id   - ", id, "\n");

#
# Suck in the Entries file
#
open(ENTRIES, $ENTRIES) || die("Cannot open $ENTRIES.\n");
while (<ENTRIES>) {
    local($filename, $version) = split('/', substr($_, 1));
    $cvsversion{$filename} = $version;
}

#
# Now check each file name passed in, except for dot files.  Dot files
# are considered to be administrative files by this script.
#
if ($check_id != 0) {
    $failed = 0;
    $directory = $ARGV[0];
    shift @ARGV;
    foreach $arg (@ARGV) {
	next if (index($arg, ".") == 0);
	$failed += &check_version($arg);
    }
    if ($failed) {
	print "\n";
	exit(1);
    }
}

#
# Record this directory as the last one checked.  This will be used
# by the log_accumulate script to determine when it is processing
# the final directory of a multi-directory commit.
#
if ($record_directory != 0) {
    &write_line("$LAST_FILE.$id", $directory);
}
exit(0);
