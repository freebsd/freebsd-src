#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bData\/Dumper\b/) {
      print "1..0 # Skip: Data::Dumper was not built\n";
      exit 0;
    }
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

