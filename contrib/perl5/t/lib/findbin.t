#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..1\n";

use FindBin qw($Bin);

print "not " unless $Bin =~ m,t[/.]lib\]?$,;
print "ok 1\n";
