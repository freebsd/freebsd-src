#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib' if -d '../lib';
}

use Data::Dumper;

print "1..1\n";

package Foo;
use overload '""' => 'as_string';

sub new { bless { foo => "bar" }, shift }
sub as_string { "%%%%" }

package main;

my $f = Foo->new;

print "#\$f=$f\n";

$_ = Dumper($f);
s/^/#/mg;
print $_;

print "not " unless /bar/ && /Foo/;
print "ok 1\n";

