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

use Cwd;
use Getopt::Std;

#
# Configuration global variables
#
$AllCurrentPackagesCommand = '/usr/sbin/pkg_info -aI';
$SelectedCurrentPackagesCommand = '/usr/sbin/pkg_info -I';
$CatProgram = "cat ";
$FetchProgram = "fetch -o - ";
$OriginCommand = '/usr/sbin/pkg_info -qo';
$GetPkgNameCommand = 'make -V PKGNAME';

#$IndexFile = "ftp://ftp.freebsd.org/pub/FreeBSD/branches/-current/ports/INDEX";
$PortsDirectory = '/usr/ports';
$IndexFile = '/usr/ports/INDEX';
$ShowCommandsFlag = 0;
$DebugFlag = 0;
$VerboseFlag = 0;
$CommentChar = "#";
$LimitFlag = "";
$PreventFlag = "";

#
# CompareNumbers
#
# Try to figure out the relationship between two program version numbers.
# Detecting equality is easy, but determining order is a little difficult.
# This function returns -1, 0, or 1, in the same manner as <=> or cmp.
#
sub CompareNumbers {
    my ($v1, $v2) = @_;

    # Short-cut in case of equality
    if ($v1 eq $v2) {
	return 0;
    }

    # Loop over different components (the parts separated by dots).
    # If any component differs, we have the basis for an inequality.
    my @s1 = split(/\./, $v1);
    my @s2 = split(/\./, $v2);
    my ($c1, $c2);
    do {
	last unless @s1 || @s2;
	$c1 = shift @s1;
	$c2 = shift @s2;
    } while ($c1 eq $c2);

    # Look at the first components of the arrays that are left.
    # These will determine the result of the comparison.
    # Note that if either version doesn't have any components left,
    # it's implicitly treated as a "0".

    # Our next set of checks looks to see if either component has a
    # leading letter (there should be at most one leading letter per
    # component, so that "4.0b1" is allowed, but "4.0beta1" is not).
    if ($c1 =~ /^\D/) {
	if ($c2 =~ /^\D/) {

	    # Both have a leading letter, so do an alpha comparison
	    # on the letters.  This isn't ideal, since we're assuming
	    # that "1.0.b4" > "1.0.a2".  But it's about the best we can do, 
	    # without encoding some explicit policy.
	    my ($letter1, $letter2);
	    $letter1 = substr($c1, 0, 1);
	    $letter2 = substr($c2, 0, 1);

	    if ($letter1 ne $letter2) {
		return $letter1 cmp $letter2;
	    }
	    else {
		# The letters matched equally.  Delete the leading
		# letters and invoke ourselves on the remainining
		# characters, which according to the Porters Handbook
		# must be digits, so for example, "1.0.a9" < "1.0.a10".
		substr($c1, 0, 1) = "";
		substr($c2, 0, 1) = "";
		return &CompareNumbers($c1, $c2);		
	    }

	}
	else {
	    # $c1 begins with a letter, but $c2 doesn't.  Let $c2
	    # win the comparison, so that "1.0.b1" < "1.0.1".
	    return -1;
	}
    }
    else {
	if ($c2 =~ /^\D/) {
	    # $c2 begins with a letter but $c1 doesn't.  Let $c1
	    # win the comparison, as above.
	    return 1;
	}
	else {
	    # Neither component begins with a leading letter.
	    # Check for numeric inequality.  We assume here that (for example)
	    # "3.09" < "3.10", and that we aren't going to be asked to
	    # decide between "3.010" and "3.10".
	    if ($c1 != $c2) {
		return $c1 <=> $c2;
	    }

	    # String comparison, given numeric equality.  This
	    # handles comparisons of the form "3.4j" < "3.4k".  This form
	    # technically isn't allowed by the Porter's Handbook, but a
	    # number of ports in the FreeBSD Ports Collection as of this
	    # writing use it (graphics/jpeg and graphics/xv).  So we need
	    # to support it.
	    #
	    # What we actually do is to strip off the leading digits and
	    # invoke ourselves on the remainder.  This allows us to handle
	    # comparisons of the form "1.1p1" < "1.1p2".  Again, not
	    # technically allowed by the Porters Handbook, but lots of ports
	    # use it.
	    else {
		$c1 =~ s/\d+//;
		$c2 =~ s/\d+//;
		if ($c1 eq $c2) {
		    return 0;
		}
		elsif ($c1 eq "") {
		    return -1;
		}
		elsif ($c2 eq "") {
		    return 1;
		}
		else {
		    return &CompareNumbers($c1, $c2);
		}
	    }
	}
    }
}

#
# CompareVersions
#
# Try to figure out the relationship between two program "full
# versions", which is defined as the 
# ${PORTVERSION}[_${PORTREVISION}][,${PORTEPOCH}]
# part of a package's name.
#
# Key points:  ${PORTEPOCH} supercedes ${PORTVERSION}
# supercedes ${PORTREVISION}.  See the commit log for revision
# 1.349 of ports/Mk/bsd.port.mk for more information.
#
sub CompareVersions {
    local($fv1, $fv2, $v1, $v2, $r1, $r2, $e1, $e2, $rc);

    $fv1 = $_[0];
    $fv2 = $_[1];

    # Shortcut check for equality before invoking the parsing
    # routines.
    if ($fv1 eq $fv2) {
	return 0;
    }
    else {
	($v1, $r1, $e1) = &GetVersionComponents($fv1);
	($v2, $r2, $e2) = &GetVersionComponents($fv2);

	# Check epoch, port version, and port revision, in that
	# order.
	$rc = &CompareNumbers($e1, $e2);
	if ($rc == 0) {
	    $rc = &CompareNumbers($v1, $v2);
	    if ($rc == 0) {
		$rc = &CompareNumbers($r1, $r2);
	    }
	}

	return $rc;
    }
}

#
# GetVersionComponents
#
# Parse out the version number, revision number, and epoch number
# of a port's version string and return them as a three-element array.
#
# Syntax is:  ${PORTVERSION}[_${PORTREVISION}][,${PORTEPOCH}]
#
sub GetVersionComponents {
    local ($fullversion, $version, $revision, $epoch);

    $fullversion = $_[0];

    $fullversion =~ /([^_,]+)/;
    $version = $1;
    
    if ($fullversion =~ /_([^_,]+)/) {
	$revision = $1;
    }
    
    if ($fullversion =~ /,([^_,]+)/) {
	$epoch = $1;
    }

    return($version, $revision, $epoch);
}

#
# GetNameAndVersion
#
# Get the name and version number of a package. Returns a two element
# array, first element is name, second element is full version string.,
#
sub GetNameAndVersion {
    local($fullname, $name, $fullversion);
    $fullname = $_[0];

    # If no hyphens then no version numbers
    return ($fullname, "", "", "", "") if $fullname !~ /-/;

    # Match (and group) everything after hyphen(s). Because the
    # regexp is 'greedy', the first .* will try and match everything up
    # to (but not including) the last hyphen
    $fullname =~ /(.+)-(.+)/;
    $name = $1;
    $fullversion = $2;

    return ($name, $fullversion);
}

#
# PrintHelp
#
# Print usage information
#
sub PrintHelp {
    print <<"EOF"
Usage:	pkg_version [-c] [-d debug] [-h] [-l limchar] [-L limchar] [-s string] 
		    [-v] [index]
	pkg_version [-d debug] -t v1 v2
-c              Show commands to update installed packages
-d debug	Debugging output (debug controls level of output)
-h		Help (this message)
-l limchar	Limit output to status flags that match
-L limchar	Limit output to status flags that DON\'T match
-s string	Limit output to packages matching a string
-v		Verbose output
index		URL or filename of index file
		(Default is $IndexFile)

-t v1 v2	Test two version strings
EOF
}

#
# Parse command-line arguments, deal with them
#
if (!getopts('cdhl:L:s:tv') || ($opt_h)) {
    &PrintHelp();
    exit;
}
if ($opt_c) {
    $ShowCommandsFlag = $opt_c;
    $LimitFlag = "<?";	# note that if the user specifies -l, we
			# deal with this *after* setting a default
			# for $LimitFlag
}
if ($opt_d) {
    $DebugFlag = $opt_d;
}
if ($opt_l) {
    $LimitFlag = $opt_l;
}
if ($opt_L) {
    $PreventFlag = $opt_L;
}
if ($opt_t) {
    $TestFlag = 1;
}
if ($opt_s) {
    $StringFlag = $opt_s;
}
if ($opt_v) {
    $VerboseFlag = 1;
}
if ($#ARGV >= 0) {
    if ($TestFlag) {
	($test1, $test2) = @ARGV;
    }
    else {
	$IndexFile = $ARGV[0];
    }
}

# Handle test flag now
if ($TestFlag) {
    my $cmp = CompareVersions($test1, $test2);
    if ($cmp < 0) {
	print "<\n";
    }
    elsif ($cmp == 0) {
	print "=\n";
    }
    else {
	print ">\n";
    }
    exit(0);
}

# Determine what command to use to retrieve the index file.
if ($IndexFile =~ m-^((http|ftp)://|file:/)-) {
    $IndexPackagesCommand = $FetchProgram . $IndexFile;
}
else {
    $IndexPackagesCommand = $CatProgram . $IndexFile;
}

#
# Get the current list of installed packages
#
if ($StringFlag) {
    if ($DebugFlag) {
       print STDERR "$SelectedCurrentPackagesCommand *$StringFlag*\n";
    }
    open CURRENT, "$SelectedCurrentPackagesCommand \\*$StringFlag\\*|";
} else {
    if ($DebugFlag) {
       print STDERR "$AllCurrentPackagesCommand\n";
    }
    open CURRENT, "$AllCurrentPackagesCommand|";
}
while (<CURRENT>) {
    ($packageString, $rest) = split;

    ($packageName, $packageFullversion) = &GetNameAndVersion($packageString);
    $currentPackages{$packageString}{'name'} = $packageName;
    $currentPackages{$packageString}{'fullversion'} = $packageFullversion;
}
close CURRENT;

#
# Iterate over installed packages, get origin directory (if it
# exists) and PORTVERSION
#
$dir = cwd();
foreach $packageString (sort keys %currentPackages) {

    open ORIGIN, "$OriginCommand $packageString|";
    $origin = <ORIGIN>;
    close ORIGIN;

    # If there is an origin variable for this package, then store it.
    if ($origin ne "") {
	chomp $origin;

	# Try to get the version out of the makefile.
	# The chdir needs to be successful or our make -V invocation
	# will fail.
	unless (chdir "$PortsDirectory/$origin" and -r "Makefile") {
	    $currentPackages{$packageString}->{orphaned} = $origin;
	    next;
	}

	open PKGNAME, "$GetPkgNameCommand|";
	$pkgname = <PKGNAME>;
	close PKGNAME;

	if ($pkgname ne "") {
	    chomp $pkgname;

	    $pkgname =~ /(.+)-(.+)/;
	    $portversion = $2;
	    
	    $currentPackages{$packageString}{'origin'} = $origin;
	    $currentPackages{$packageString}{'portversion'} = $portversion;
	}
    }
}
chdir "$dir";

#
# Slurp in the index file
#
if ($DebugFlag) {
    print STDERR "$IndexPackagesCommand\n";
}

open INDEX, "$IndexPackagesCommand|";
while (<INDEX>) {
    ($packageString, $packagePath, $rest) = split(/\|/);

    ($packageName, $packageFullversion) = &GetNameAndVersion($packageString);
    $indexPackages{$packageName}{'name'} = $packageName;
    $indexPackages{$packageName}{'path'} = $packagePath;
    if (defined $indexPackages{$packageName}{'fullversion'}) {
	$indexPackages{$packageName}{'fullversion'} .= "|" . $packageFullversion;
    }
    else {
	$indexPackages{$packageName}{'fullversion'} = $packageFullversion;
    }
    $indexPackages{$packageName}{'refcount'}++;
}
close INDEX;

#
# If we're doing commands output, cripple the output so that users
# can't just pipe the output to sh(1) and expect this to work.
#
if ($ShowCommandsFlag) {
    print<<EOF
echo "The commands output of pkg_version cannot be executed without editing."
echo "You MUST save this output to a file and then edit it, taking into"
echo "account package dependencies and the fact that some packages cannot"
echo "or should not be upgraded." 
exit 1
EOF
}

#
# Produce reports
#
# Prior versions of pkg_version used commas (",") as delimiters
# when there were multiple versions of a package installed.
# The new package version number syntax uses commas as well,
# so we've used vertical bars ("|") internally, and convert them
# to commas before we output anything so the reports look the
# same as they did before.
#
foreach $packageString (sort keys %currentPackages) {
    $~ = "STDOUT_VERBOSE"  if $VerboseFlag;
    $~ = "STDOUT_COMMANDS" if $ShowCommandsFlag;

    $packageNameVer = $packageString;
    $packageName = $currentPackages{$packageString}{'name'};

    $currentVersion = $currentPackages{$packageString}{'fullversion'};

    if ($currentPackages{$packageString}->{orphaned}) {

	next if $ShowCommandsFlag;
	$versionCode = "?";
	$Comment = "orphaned: $currentPackages{$packageString}->{orphaned}";

    } elsif (defined $currentPackages{$packageString}{'portversion'}) {

	$portVersion = $currentPackages{$packageString}{'portversion'};

	$portPath = "$PortsDirectory/$currentPackages{$packageString}{'origin'}";

	# Do the comparison
	$rc = &CompareVersions($currentVersion, $portVersion);
	    
	if ($rc == 0) {
	    $versionCode = "=";
	    $Comment = "up-to-date with port";
	}
	elsif ($rc < 0) {
	    $versionCode = "<";
	    $Comment = "needs updating (port has $portVersion)";
	}
	elsif ($rc > 0) {
	    $versionCode = ">";
	    $Comment = "succeeds port (port has $portVersion)";
	}
	else {
	    $versionCode = "!";
	    $Comment = "Comparison failed";
	}
    }

    elsif (defined $indexPackages{$packageName}{'fullversion'}) {

	$indexVersion = $indexPackages{$packageName}{'fullversion'};
	$indexRefcount = $indexPackages{$packageName}{'refcount'};

	$portPath = $indexPackages{$packageName}{'path'};

	if ($indexRefcount > 1) {
	    $versionCode = "*";
	    $Comment = "multiple versions (index has $indexVersion)";
	    $Comment =~ s/\|/,/g;
	}
	else {

	    # Do the comparison
	    $rc = 
		&CompareVersions($currentVersion, $indexVersion);
	    
	    if ($rc == 0) {
		$versionCode = "=";
		$Comment = "up-to-date with index";
	    }
	    elsif ($rc < 0) {
		$versionCode = "<";
		$Comment = "needs updating (index has $indexVersion)"
	    }
	    elsif ($rc > 0) {
		$versionCode = ">";
		$Comment = "succeeds index (index has $indexVersion)";
	    }
	    else {
		$versionCode = "!";
		$Comment = "Comparison failed";
	    }
	}
    }
    else {
	next if $ShowCommandsFlag;
	$versionCode = "?";
	$Comment = "unknown in index";
    }

    # Having figured out what to print, now determine, based on the
    # $LimitFlag and $PreventFlag variables, if we should print or not.
    if ((not $LimitFlag) and (not $PreventFlag)) {
	write;
    } elsif ($PreventFlag) {
	if ($versionCode !~ m/[$PreventFlag]/o) {
	    if (not $LimitFlag) {
		write;
	    } else {
		write if $versionCode =~ m/[$LimitFlag]/o;
	    }
	}
    } else {
	# Must mean that there is a LimitFlag
	write if $versionCode =~ m/[$LimitFlag]/o;
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
@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  @<
$packageName,              $versionCode
.
  ;

# Verbose report (-v flag)
format STDOUT_VERBOSE =
@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  @<  @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$packageNameVer,           $versionCode, $Comment
.
  ;

# Report that includes commands to update program (-c flag)
format STDOUT_COMMANDS =
@<
$CommentChar  
@< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$CommentChar, $packageName
@< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$CommentChar, $Comment  
@<
$CommentChar
cd @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$portPath
make clean && make && pkg_delete -f @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
              $packageNameVer
make install clean

.
  ;
