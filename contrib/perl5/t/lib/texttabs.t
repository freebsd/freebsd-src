#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..3\n";

use Text::Tabs;

$tabstop = 4;

$s1 = "foo\tbar\tb\tb";
$s2 = expand $s1;
$s3 = unexpand $s2;

print "not " unless $s2 eq "foo bar b   b";
print "ok 1\n";

print "not " unless $s3 eq "foo bar b\tb";
print "ok 2\n";


$tabstop = 8;

print "not " unless unexpand("                    foo") eq "\t\t    foo";
print "ok 3\n";
