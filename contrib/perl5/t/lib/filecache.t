#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..1\n";

use FileCache;

# This is really not a complete test as I don't bother to open enough
# files to make real swapping of open filedescriptor happen.

$path = "foo";
cacheout $path;

print $path "\n";

close $path;

print "not " unless -f $path;
print "ok 1\n";

unlink $path;
