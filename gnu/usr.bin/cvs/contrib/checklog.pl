#!/usr/bin/perl
# Copyright (c) Wolfram Schneider <wosch@freebsd.org>. June 1996, Berlin.
#
# checklog - extract your commits from commitlogs archive
#
# checklog username /a/cvs/CVSROOT/commitlogs/*[a-y]
# zcat /a/cvs/CVSROOT/commitlogs/*.gz | checklog [username]
#
# $Id: checklog.pl,v 1.2 1996/06/27 12:54:25 wosch Exp $

# your name or first argument
if ($ARGV[0]) {
    $name = $ARGV[0]; shift @ARGV; 
    warn "Is this really a username: `$name' ?\n" 
	unless $name =~ /^[a-z0-9]+$/;
} else {
    $name = `whoami`; chop $name;
}

# date string 96/02/18 10:44:59
$date = '[0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]';

$flag = 0;
while(<>) {
    if (/^[a-z]/) {		  # start of a commit
	if (m%^$name\s+$date$%o) { # it's  *your* commit
	    $flag = 1;
	} else {
	    $flag = 0;
	}
    }

    print if $flag;
}
