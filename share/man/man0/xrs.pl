#!/usr/bin/perl
# Copyright (c) 1996 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
#
# xrs - detect unsorted cross references in section SEE ALSO
#
# Cross references in the SEE ALSO section should
# be sorted by section number, and then placed in alphabetical
# order and comma separated.  For example:
#
# ls(1),  ps(1),  group(5),  passwd(5).
#
# The last entry may be finished with a dot `.'
#
# or a source example:.
# .Sh SEE ALSO
# .Xr foo 1 ,
# .Xr bla 2 ,
# .Xr foobar 8
# .Sh HISTORY
#
# usage: xrs manpages ...
#
# $Id$

sub mysort {

    local(@c) = split($",$a);
    local(@d) = split($",$b);

    local($ret) = ($c[2] <=> $d[2]);
		
    return $ret if $ret;
    return $c[1] cmp $d[1];
}

sub compare {
    local(*a, *b) = @_;

    return 1 if ($#a != $#b);

    for($i = 0; $i <= $#a; $i++) {
	return 1 if
	    $a[$i] ne $b[$i];
    }

    for ($i = 0; $i < $#a; $i++) {
	return 1 if $a[$i] !~ /\s,\s*$/;
    }
    return 1 if $a[$#a] =~ /\s,\s*$/;
    return 1 if $a[$#a] =~ /^.Xr\s+\S+\s+\S+\s+[^.]$/;
    return 0;
}


while(<>) {
    if (/^\.Sh\s/ && /SEE\s+ALSO/) {
	$file = $ARGV;
	@a = ();
	while(<>) {
	    if (!/^\.(Xr|Fn)\s/) {
		if (!/^\.(Sh|Rs|\\"|Pp|br)\s*/ && !/^\s*$/) {
		    warn "Oops: $ARGV $_";
		}
		last;
	    }
	    push(@a, $_);
	}
	@b = sort mysort @a;
	if (&compare(*a,*b)) {
	    print "$file\n";
	    #print "@a\n@b\n";
	}
    }
    last if eof();
}
