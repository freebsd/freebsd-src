#!/usr/bin/perl
# Convert forth source files to a giant C string

$now = localtime;

print <<EOF
/*******************************************************************
** s o f t c o r e . c
** Forth Inspired Command Language - 
** Words from CORE set written in FICL
** Author: John Sadler (john_sadler\@alum.mit.edu)
** Created: 27 December 1997
** Last update: $now
*******************************************************************/
/*
** This file contains definitions that are compiled into the
** system dictionary by the first virtual machine to be created.
** Created automagically by ficl/softwords/softcore.pl 
*/


#include "ficl.h"

static char softWords[] = 
EOF
;

$commenting = 0;

while (<>) {
    s"\n$"";            # remove EOL
    s"\t"    "g;        # replace each tab with 4 spaces
    s/\"/\\\"/g;        # escape quotes

    next if /^\s*\\\s*$/;# toss empty comments
    next if /^\s*$/;    # toss empty lines

    if (/^\\\s\*\*/)  {	# emit / ** lines as C comments
        s"^\\ "";
        if ($commenting == 0) {
	    print "/*\n";
	}
        $commenting = 1;
        print "$_\n";
        next;
    }

    if ($commenting == 1) {
	print "*/\n";
    }

    $commenting = 0;

    if (/^\\\s#/)  {	# pass commented preprocessor directives
        s"^\\ "";
        print "$_\n";
        next;
    }

    next if /^\s*\\ /; # toss all other comments
    s"\\\s+.*$"" ;     # lop off trailing \ comments
    s"\s+$" ";         # remove trailing space
    #
    # emit all other lines as quoted string fragments
    #
    $out = "    \"" . $_ . " \\n\"";
    print "$out\n";
}

if ($commenting == 1) {
    print "*/\n";
}

print <<EOF
    "quit ";


void ficlCompileSoftCore(FICL_VM *pVM)
{
    int ret = ficlExec(pVM, softWords);
    if (ret == VM_ERREXIT)
        assert(FALSE);
    return;
}


EOF
;

