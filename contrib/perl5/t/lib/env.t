#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

BEGIN {
	$ENV{FOO} = "foo";
}

use Env qw(FOO);

$FOO .= "/bar";

print "1..1\n";
print "not " if $FOO ne 'foo/bar';
print "ok 1\n";
