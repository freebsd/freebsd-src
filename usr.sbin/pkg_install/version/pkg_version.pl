#! /usr/bin/perl
#
# Copyright 1998 Bruce A. Mah
#
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
# THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# pkg_version.pl
#
# A package version-checking utility for FreeBSD.
#
# $FreeBSD$
#

use Getopt::Std;

#
# Configuration global variables
#
$Version = '0.1';
$CurrentPackagesCommand = '/usr/sbin/pkg_info -aI';
$CatProgram = "cat ";
$FetchProgram = "fetch -o - ";

#$indexFile = "ftp://ftp.freebsd.org/pub/FreeBSD/ports-current/INDEX";
$IndexFile = 'file:/usr/ports/INDEX';
$ShowCommandsFlag = 0;
$DebugFlag = 0;
$VerboseFlag = 0;
$CommentChar = "#";
$LimitFlag = "";

#
# CompareVersions
#
# Try to figure out the relationship between two program version numbers.
# Detecting equality is easy, but determining order is a little difficult.
# This function returns -1, 0, or 1, in the same manner as <=> or cmp.
#
sub CompareVersions {
    local($v1, $v2);
    $v1 = $_[0];
    $v2 = $_[1];

    # Short-cut in case of equality
    if ($v1 eq $v2) {
	return 0;
    }

    # Loop over different components (the parts separated by dots).
    # If any component differs, we have the basis for an inequality.
    while (1) {
	($p1, $v1) = split(/\./, $v1, 2);
	($p2, $v2) = split(/\./, $v2, 2);

	# If we\'re out of components, they\'re equal (this probably won\'t
	# happen, since the short-cut case above should get this).
	if (($p1 eq "") && ($p2 eq "")) {
	    return 0;
	}
	# Check for numeric inequality.  We assume here that (for example)
	# 3.09 < 3.10.
	elsif ($p1 != $p2) {
	    return $p1 <=> $p2;
	}
	# Check for string inequality, given numeric equality.  This
	# handles version numbers of the form 3.4j < 3.4k.
	elsif ($p1 ne $p2) {
	    return $p1 cmp $p2;
	}
    }

}

#
# GetNameAndVersion
#
# Get the name and version number of a package. Returns a two element
# array, first element is name, second element is version number.
#
sub GetNameAndVersion {
    local($string);
    $string = $_[0];

    # If no hyphens then no version number
    return ($string, "") if $string !~ /-/;

    # Match (and group) everything in between two hyphens. Because the
    # regexp is 'greedy', the first .* will try and match everything up
    # to (but not including) the last hyphen
    $string =~ /(.*)-(.*)/;
    return ($1, $2);
}

#
# PrintHelp
#
# Print usage information
#
sub PrintHelp {
    print <<"EOF"
pkg_version $Version
Bruce A. Mah <bmah\@freebsd.org>

Usage: pkg_version [-c] [-d debug] [-h] [-v] [index]
-c              Show commands to update installed packages
-d debug	Debugging output (debug controls level of output)
-h		Help (this message)
-l limchar	Limit output
-v		Verbose output
index		URL or filename of index file
		(Default is $IndexFile)
EOF
}

#
# Parse command-line arguments, deal with them
#
if (!getopts('cdhl:v') || ($opt_h)) {
    &PrintHelp();
    exit;
}
if ($opt_c) {
    $ShowCommandsFlag = $opt_c;
}
if ($opt_d) {
    $DebugFlag = $opt_d;
}
if ($opt_l) {
    $LimitFlag = $opt_l;
}
if ($opt_v) {
    $VerboseFlag = 1;
}
if ($#ARGV >= 0) {
    $IndexFile = $ARGV[0];
}

# Gross hack to get around a bug in fetch(1).  When PR bin/7203 gets fixed,
# we can make a lot of this code go away...basically the problem is that
# we can't depend on "fetch -o -" to do the right thing with files in the
# filesystem.
if ($IndexFile =~ s-^file:/-/-) {
    $IndexPackagesCommand = $CatProgram . $IndexFile;
}
elsif ($IndexFile =~ m-^(http|ftp)://-) {
    $IndexPackagesCommand = $FetchProgram . $IndexFile;
}
else {
    $IndexPackagesCommand = $CatProgram . $IndexFile;
}

#
# Slurp in files
#
if ($DebugFlag) {
    print STDERR "$CurrentPackagesCommand\n";
}

open CURRENT, "$CurrentPackagesCommand|";
while (<CURRENT>) {
    ($packageString, $rest) = split;
    ($packageName, $packageVersion) = &GetNameAndVersion($packageString);
    $currentPackages{$packageName}{'name'} = $packageName;
    if (defined $currentPackages{$packageName}{'version'}) {
	$currentPackages{$packageName}{'version'} .= "," . $packageVersion;
    }
    else {
	$currentPackages{$packageName}{'version'} = $packageVersion;
    }
    $currentPackages{$packageName}{'refcount'}++;
}
close CURRENT;

if ($DebugFlag) {
    print STDERR "$IndexPackagesCommand\n";
}

open INDEX, "$IndexPackagesCommand|";
while (<INDEX>) {
    ($packageString, $packagePath, $rest) = split(/\|/);
    ($packageName, $packageVersion) = &GetNameAndVersion($packageString);
    $indexPackages{$packageName}{'name'} = $packageName;
    $indexPackages{$packageName}{'path'} = $packagePath;
    if (defined $indexPackages{$packageName}{'version'}) {
	$indexPackages{$packageName}{'version'} .= "," . $packageVersion;
    }
    else {
	$indexPackages{$packageName}{'version'} = $packageVersion;
    }
    $indexPackages{$packageName}{'refcount'}++;
}
close INDEX;

#
# Produce reports
#
foreach $packageName (sort keys %currentPackages) {
    $~ = "STDOUT_VERBOSE"  if $VerboseFlag;
    $~ = "STDOUT_COMMANDS" if $ShowCommandsFlag;

    $packageNameVer = "$packageName-$currentPackages{$packageName}{'version'}";

    if (defined $indexPackages{$packageName}{'version'}) {

	$indexVersion = $indexPackages{$packageName}{'version'};
	$currentVersion = $currentPackages{$packageName}{'version'};

	$indexRefcount = $indexPackages{$packageName}{'refcount'};
	$currentRefcount = $currentPackages{$packageName}{'refcount'};

	$packagePath = $indexPackages{$packageName}{'path'};
	
	if (($indexRefcount > 1) || ($currentRefcount > 1)) {
	    $versionCode = "?";
	    $Comment = "multiple versions (index has $indexVersion)";
	}
	else {

	    $rc = &CompareVersions($currentVersion, $indexVersion);
	    
	    if ($rc == 0) {
		next if $ShowCommandsFlag;
		$versionCode = "=";
		$Comment = "up-to-date";
	    }
	    elsif ($rc < 0) {
		$versionCode = "<";
		$Comment = "needs updating (index has $indexVersion)"
	    }
	    elsif ($rc > 0) {
		next if $ShowCommandsFlag;
		$versionCode = ">";
		$Comment = "succeeds index (index has $indexVersion)";
	    }
	    else {
		$versionCode = "?";
		$Comment = "Comparison failed";
	    }
	}
    }
    else {
	next if $ShowCommandsFlag;
	$versionCode = "?";
	$Comment = "unknown in index";
    }

    if ($LimitFlag) {
	write if $versionCode =~ m/[$LimitFlag]/o;
    } else {
	write;
    }
}

exit 0;

#
# Formats
#
# $CommentChar is in the formats because you can't put a literal '#' in
# a format specification

# General report (no output flags)
format STDOUT =
@<<<<<<<<<<<<<<<<<<<<<<<<<  @<
$packageName,              $versionCode
.
  ;

# Verbose report (-v flag)
format STDOUT_VERBOSE =
@<<<<<<<<<<<<<<<<<<<<<<<<<  @<  @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$packageNameVer,           $versionCode, $Comment
.
  ;

# Report that includes commands to update program (-c flag)
format STDOUT_COMMANDS =
@<
$CommentChar  
@< @<<<<<<<<<<<<<<<<<<<<<<<<
$CommentChar, $packageName
@< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$CommentChar, $Comment  
@<
$CommentChar
cd @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$packagePath
make && pkg_delete -f @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
              $packageNameVer
make install

.
  ;
