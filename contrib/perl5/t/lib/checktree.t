#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..1\n";

use File::CheckTree;

# We assume that we run from the perl "t" directory.

validate q{
    lib              -d || die
    lib/checktree.t  -f || die
};

print "ok 1\n";
