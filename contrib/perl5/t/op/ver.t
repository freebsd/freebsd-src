#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..28\n";

my $test = 1;

use v5.5.640;
require v5.5.640;
print "ok $test\n";  ++$test;

# printing characters should work
if (ord("\t") == 9) { # ASCII
    print v111;
    print v107.32;
    print "$test\n"; ++$test;

    # hash keys too
    $h{v111.107} = "ok";
    print "$h{ok} $test\n"; ++$test;
}
else { # EBCDIC
    print v150;
    print v146.64;
    print "$test\n"; ++$test;

    # hash keys too
    $h{v150.146} = "ok";
    print "$h{ok} $test\n"; ++$test;
}

# poetry optimization should also
sub v77 { "ok" }
$x = v77;
print "$x $test\n"; ++$test;

# but not when dots are involved
if (ord("\t") == 9) { # ASCII
    $x = v77.78.79;
}
else {
    $x = v212.213.214;
}
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
if (ord("\t") == 9) { # ASCII
    $h{111.107.32} = "ok";
}
else {
    $h{150.146.64} = "ok";
}
print "$h{ok } $test\n"; ++$test;

if (ord("\t") == 9) { # ASCII
    $x = 77.78.79;
}
else {
    $x = 212.213.214;
}
print "not " unless $x eq "MNO";
print "ok $test\n";  ++$test;

print "not " unless 1.20.300.4000 eq "\x{1}\x{14}\x{12c}\x{fa0}";
print "ok $test\n";  ++$test;

# test sprintf("%vd"...) etc
if (ord("\t") == 9) { # ASCII
    print "not " unless sprintf("%vd", "Perl") eq '80.101.114.108';
}
else {
    print "not " unless sprintf("%vd", "Perl") eq '215.133.153.147';
}
print "ok $test\n";  ++$test;

print "not " unless sprintf("%vd", v1.22.333.4444) eq '1.22.333.4444';
print "ok $test\n";  ++$test;

if (ord("\t") == 9) { # ASCII
    print "not " unless sprintf("%vx", "Perl") eq '50.65.72.6c';
}
else {
    print "not " unless sprintf("%vx", "Perl") eq 'd7.85.99.93';
}
print "ok $test\n";  ++$test;

print "not " unless sprintf("%vX", 1.22.333.4444) eq '1.16.14D.115C';
print "ok $test\n";  ++$test;

if (ord("\t") == 9) { # ASCII
    print "not " unless sprintf("%*v#o", ":", "Perl") eq '0120:0145:0162:0154';
}
else {
    print "not " unless sprintf("%*v#o", ":", "Perl") eq '0327:0205:0231:0223';
}
print "ok $test\n";  ++$test;

print "not " unless sprintf("%*vb", "##", v1.22.333.4444)
    eq '1##10110##101001101##1000101011100';
print "ok $test\n";  ++$test;

print "not " unless sprintf("%vd", join("", map { chr }
					    unpack "U*", v2001.2002.2003))
		    eq '2001.2002.2003';
print "ok $test\n";  ++$test;

{
    use bytes;
    if (ord("\t") == 9) { # ASCII
        print "not " unless sprintf("%vd", "Perl") eq '80.101.114.108';
    }
    else {
        print "not " unless sprintf("%vd", "Perl") eq '215.133.153.147';
    }
    print "ok $test\n";  ++$test;

    print "not " unless
        sprintf("%vd", 1.22.333.4444) eq '1.22.197.141.225.133.156';
    print "ok $test\n";  ++$test;

    if (ord("\t") == 9) { # ASCII
        print "not " unless sprintf("%vx", "Perl") eq '50.65.72.6c';
    }
    else {
        print "not " unless sprintf("%vx", "Perl") eq 'd7.85.99.93';
    }
    print "ok $test\n";  ++$test;

    print "not " unless sprintf("%vX", v1.22.333.4444) eq '1.16.C5.8D.E1.85.9C';
    print "ok $test\n";  ++$test;

    if (ord("\t") == 9) { # ASCII
        print "not " unless sprintf("%*v#o", ":", "Perl") eq '0120:0145:0162:0154';
    }
    else {
        print "not " unless sprintf("%*v#o", ":", "Perl") eq '0327:0205:0231:0223';
    }
    print "ok $test\n";  ++$test;

    print "not " unless sprintf("%*vb", "##", v1.22.333.4444)
	eq '1##10110##11000101##10001101##11100001##10000101##10011100';
    print "ok $test\n";  ++$test;
}

{
    # bug id 20000323.056

    print "not " unless "\x{41}" eq +v65;
    print "ok $test\n";
    $test++;

    print "not " unless "\x41" eq +v65;
    print "ok $test\n";
    $test++;

    print "not " unless "\x{c8}" eq +v200;
    print "ok $test\n";
    $test++;

    print "not " unless "\xc8" eq +v200;
    print "ok $test\n";
    $test++;

    print "not " unless "\x{221b}" eq v8731;
    print "ok $test\n";
    $test++;
}
