#!./perl

BEGIN {
    chdir 't' if -d 't';
    unshift @INC, "../lib";
}

print "1..22\n";

my $test = 1;

use v5.5.640;
require v5.5.640;
print "ok $test\n";  ++$test;

# printing characters should work
print v111;
print v107.32;
print "$test\n"; ++$test;

# hash keys too
$h{v111.107} = "ok";
print "$h{ok} $test\n"; ++$test;

# poetry optimization should also
sub v77 { "ok" }
$x = v77;
print "$x $test\n"; ++$test;

# but not when dots are involved
$x = v77.78.79;
print "not " unless $x eq "MNO";
print "ok $test\n";  ++$test;

print "not " unless v1.20.300.4000 eq "\x{1}\x{14}\x{12c}\x{fa0}";
print "ok $test\n";  ++$test;

#
# now do the same without the "v"
use 5.5.640;
require 5.5.640;
print "ok $test\n";  ++$test;

# hash keys too
$h{111.107.32} = "ok";
print "$h{ok } $test\n"; ++$test;

$x = 77.78.79;
print "not " unless $x eq "MNO";
print "ok $test\n";  ++$test;

print "not " unless 1.20.300.4000 eq "\x{1}\x{14}\x{12c}\x{fa0}";
print "ok $test\n";  ++$test;

# test sprintf("%vd"...) etc
print "not " unless sprintf("%vd", "Perl") eq '80.101.114.108';
print "ok $test\n";  ++$test;

print "not " unless sprintf("%vd", v1.22.333.4444) eq '1.22.333.4444';
print "ok $test\n";  ++$test;

print "not " unless sprintf("%vx", "Perl") eq '50.65.72.6c';
print "ok $test\n";  ++$test;

print "not " unless sprintf("%vX", 1.22.333.4444) eq '1.16.14D.115C';
print "ok $test\n";  ++$test;

print "not " unless sprintf("%*v#o", ":", "Perl") eq '0120:0145:0162:0154';
print "ok $test\n";  ++$test;

print "not " unless sprintf("%*vb", "##", v1.22.333.4444)
    eq '1##10110##101001101##1000101011100';
print "ok $test\n";  ++$test;

{
    use bytes;
    print "not " unless sprintf("%vd", "Perl") eq '80.101.114.108';
    print "ok $test\n";  ++$test;

    print "not " unless
        sprintf("%vd", 1.22.333.4444) eq '1.22.197.141.225.133.156';
    print "ok $test\n";  ++$test;

    print "not " unless sprintf("%vx", "Perl") eq '50.65.72.6c';
    print "ok $test\n";  ++$test;

    print "not " unless sprintf("%vX", v1.22.333.4444) eq '1.16.C5.8D.E1.85.9C';
    print "ok $test\n";  ++$test;

    print "not " unless sprintf("%*v#o", ":", "Perl") eq '0120:0145:0162:0154';
    print "ok $test\n";  ++$test;

    print "not " unless sprintf("%*vb", "##", v1.22.333.4444)
	eq '1##10110##11000101##10001101##11100001##10000101##10011100';
    print "ok $test\n";  ++$test;
}
