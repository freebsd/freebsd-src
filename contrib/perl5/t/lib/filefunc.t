#!./perl

BEGIN {
    $^O = '';
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..1\n";

use File::Spec::Functions;

if (catfile('a','b','c') eq 'a/b/c') {
    print "ok 1\n";
} else {
    print "not ok 1\n";
}
