#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..2\n";

use File::Find;

# hope we will eventually find ourself
find(sub { print "ok 1\n" if $_ eq 'filefind.t'; }, ".");
finddepth(sub { print "ok 2\n" if $_ eq 'filefind.t'; }, ".");
