#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use File::Path;
use strict;

my $count = 0;
$^W = 1;

print "1..4\n";

# first check for stupid permissions second for full, so we clean up
# behind ourselves
for my $perm (0111,0777) {
    mkpath("foo/bar");
    chmod $perm, "foo", "foo/bar";

    print "not " unless -d "foo" && -d "foo/bar";
    print "ok ", ++$count, "\n";

    rmtree("foo");
    print "not " if -e "foo";
    print "ok ", ++$count, "\n";
}
