#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

BEGIN {
	$ENV{FOO} = "foo";
	$ENV{BAR} = "bar";
}

use Env qw(FOO $BAR);

$FOO .= "/bar";
$BAR .= "/baz";

print "1..2\n";

print "not " if $FOO ne 'foo/bar';
print "ok 1\n";

print "not " if $BAR ne 'bar/baz';
print "ok 2\n";

