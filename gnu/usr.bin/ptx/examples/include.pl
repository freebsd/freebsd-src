#!/usr/bin/perl --						# -*-Perl-*-
eval "exec /usr/bin/perl -S $0 $*"
    if $running_under_some_shell;

# Construct a permuted index for all system include files.
# Copyright (C) 1991 Free Software Foundation, Inc.
# Francois Pinard <pinard@iro.umontreal.ca>, June 1991.

# NOTE: about removing asm statements?
# NOTE: about removing strings?
# NOTE: about ignoring 0xHEXDIGITS, unchar/ushort/etc.

# Construct a sorted list of system include files.

opendir (DIR, "/usr/include");
@includes = sort grep (-f "/usr/include/$_", readdir (DIR));
opendir (DIR, "/usr/include/sys");
foreach (sort grep (-f "/usr/include/sys/$_", readdir (DIR))) {
    push (@includes, "sys/$_");
}
closedir (DIR);

# Launch the permuted indexer, with a list of ignore words. 

$ignore = "/tmp/incptx.$$";
open (IGNORE, "> $ignore");
print IGNORE join ("\n", split (' ', <<IGNORE)), "\n";
asm at at386 break bss case ch char continue copyright corporation
default define defined do double dst else endif enum extern file flag
float for goto i286 i386 ident if ifdef ifndef int interactive len
lint long m32 mpat num pdp11 printf ptr register return sco5 short siz
sizeof src static str struct sun switch sys systems type typedef u370
u3b u3b15 u3b2 u3b5 undef union unsigned vax void while win
IGNORE
close IGNORE;
exit 0;

open (OUTPUT, "| ptx -r -f -W '[a-zA-Z_][a-zA-Z_0-9]+' -F ... -i $ignore")
    || die "ptx did not start\n";
select (OUTPUT);

# Reformat all files, removing C comments and adding a reference field.

foreach $include (@includes)
{
    warn "Reading /usr/include/$include\n";
    open (INPUT, "/usr/include/$include");
    while (<INPUT>)
    {

	# Get rid of comments.

	$comment = $next_comment;
	if ($comment)
	{
	    $next_comment = !s,^.*\*/,,;
	}
	else
	{
	    s,/\*.*\*/,,g;
	    $next_comment = s,/\*.*,,;
	}
	next if $comment && $next_comment;

	# Remove extraneous white space.

	s/[ \t]+/ /g;
	s/ $//;
	next if /^$/;

	# Print the line with its reference.

	print "$include($.): ", $_;
    }
}

warn "All read, now ptx' game!\n";
close OUTPUT || die "ptx failed...\n";
unlink $ignore;
