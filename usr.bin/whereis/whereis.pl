#!/usr/bin/perl
#
# Copyright © 1995, 1996 Jörg Wunsch
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
# whereis -- search for binaries, man pages and source directories.
#
# Rewritten from scratch for FreeBSD after the 4.3BSD manual page.
#
# $FreeBSD$
#

sub usage
{
    print STDERR "usage: $0 [-bms] [-u] [-BMS dir... -f] name ...\n";
    exit 1;
}

sub scanopts
{
    local($i, $j);
  arg:
    while ($ARGV[$i] =~ /^-/) {
      opt:
	for ($j = 1; $j < length($ARGV[$i]); $j++) {
	    local($_) = substr($ARGV[$i], $j, 1);
	    local($what, @list);
	    $opt_b++, next opt if /b/;
	    $opt_m++, next opt if /m/;
	    $opt_s++, next opt if /s/;
	    $opt_u++, next opt if /u/;
	    &usage unless /[BMS]/;

	    # directory list processing
	    $what = $_; @list = ();
	    push(@list, substr($ARGV[$i], $j+1)) if $j+1 < length($ARGV[$i]);
	    $i++;
	    while ($i <= $#ARGV && $ARGV[$i] !~ /^-/) {
		push(@list, $ARGV[$i++]);
	    }
	    if ($what eq "B") {@binaries = @list;}
	    elsif ($what eq "M") {@manuals = @list;}
	    elsif ($what eq "S") {@sources = @list;}

	    $i++, last arg if $ARGV[$i] =~ /^-f$/;
	    next arg;
	}
	$i++;
    }
    &usage if $i > $#ARGV;

    while ($ARGV[$i]) {
	push(@names, $ARGV[$i++]);
    }
}


sub decolonify
{
    local($list) = @_;
    local($_, @rv);
    foreach(split(/:/, $list)) {
	push(@rv, $_);
    }
    return @rv;
}


&scanopts;

# default to all if no type requested
if ($opt_b + $opt_m + $opt_s == 0) {$opt_b = $opt_m = $opt_s = 1;}

if (!defined(@binaries)) {
    #
    # first, use default path, then append /usr/libexec and the user's path
    #
    local($cs_path) = `/usr/sbin/sysctl -n user.cs_path`;
    local(@list, %path);

    chop($cs_path);

    @list = &decolonify($cs_path);
    push(@list, "/usr/libexec");
    push(@list, &decolonify($ENV{'PATH'}));

    # resolve ~, remove duplicates
    foreach (@list) {
	s/^~/$ENV{'HOME'}/ if /^~/;
	push(@binaries, $_) if !$path{$_};
	$path{$_}++;
    }
}

if (!defined(@manuals)) {
    #
    # first, use default manpath, then append user's $MANPATH
    #
    local($usermanpath) = $ENV{'MANPATH'};
    delete $ENV{'MANPATH'};
    local($manpath) = `/usr/bin/manpath`;
    local(@list, %path, $i);

    chop($manpath);

    @list = &decolonify($manpath);
    push(@list, &decolonify($usermanpath));

    # remove duplicates
    foreach (@list) {
	push(@manuals, $_) if !$path{$_};
	$path{$_}++;
    }
}

if (!defined(@sources)) {
    #
    # default command sources
    #
    local($_);

    @sources = ("/usr/src/bin", "/usr/src/usr.bin", "/usr/src/sbin",
		"/usr/src/usr.sbin", "/usr/src/libexec",
		"/usr/src/gnu/bin", "/usr/src/gnu/usr.bin",
		"/usr/src/gnu/sbin", "/usr/src/gnu/usr.sbin",
		"/usr/src/gnu/libexec");

    #
    # if /usr/ports exists, look in all its subdirs, too
    #
    if (-d "/usr/ports" && opendir(PORTS, "/usr/ports")) {
	while ($_ = readdir(PORTS)) {
	    next if /^\.\.?$/;
	    next if /^distfiles$/; # magic
	    next if ! -d "/usr/ports/$_";
	    push(@sources, "/usr/ports/$_");
	}
	closedir(PORTS);
    }
}

if ($opt_m) {
    # construct a new MANPATH
    foreach (@manuals) {
	next if ! -d $_;
	if ($manpath) { $manpath .= ":$_"; }
	else { $manpath = $_; }
    }
}

#
# main loop
#
foreach $name (@names) {
    $name =~ s|^.*/||;		# strip leading path name component
    $name =~ s/,v$//; $name =~ s/^s\.//; # RCS or SCCS suffix/prefix
    $name =~ s/\.(Z|z|gz)$//;	# compression suffix
    $name =~ s/\.[^.]+//;	# any other suffix

    $line = "";
    $unusual = 0;

    if ($opt_b) {
	#
	# Binaries have to match exactly, and must be regular executable
	# files.
	#
	$unusual++;
	foreach (@binaries) {
	    $line .= " $_/$name", $unusual--, last if -f "$_/$name" && -x _;
	}
    }

    if ($opt_m) {
	#
	# Ask the man command to do the search for us.
	#
	$unusual++;
	chop($result = `man -S 1:8 -M $manpath -w $name 2> /dev/null`);
	if ($result ne '') {
	    $unusual--;
	    ($cat, $junk, $src) = split(/[() \t\n]+/, $result);
	    if ($src ne '') { $line .= " $src"; }
	    else { $line .= " $cat"; }
	}
    }

    if ($opt_s) {
	#
	# Sources match if a subdir with the exact name is found.
	#
	$found = 0;
	$unusual++;
	foreach (@sources) {
	    $line .= " $_/$name", $unusual--, $found++, last if -d "$_/$name";
	}
	#
	# If not yet found, ask locate(1) to do the search for us.
	# This will find sources for things like lpr, but take longer.
	# Do only match locate output that starts with one of our
	# source directories, and at least one further level of
	# subdirectories.
	#
	if (!$found && open(LOCATE, "locate */$name 2>/dev/null |")) {
	  locate_item:
	    while (chop($loc = <LOCATE>)) {
		foreach (@sources) {
		    $line .= " $loc", $unusual--, last locate_item
			if $loc =~ m|^$_/[^/]+/|;
		}
	    }
	    close(LOCATE);
	}
    }

    if ($opt_u) {
	print "$name:\n" if $unusual;
    } else {
	print "$name:$line\n";
    }
}

