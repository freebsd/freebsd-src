#!/usr/bin/perl -w
# 

BEGIN {
    chdir 't' if -d 't';
    @INC = '.'; 
    push @INC, '../lib';
}    

print "1..20\n";

use strict;

require Tie::SubstrHash;

my %a;

tie %a, 'Tie::SubstrHash', 3, 3, 3;

$a{abc} = 123;
$a{bcd} = 234;

print "not " unless $a{abc} == 123;
print "ok 1\n";

print "not " unless keys %a == 2;
print "ok 2\n";

delete $a{abc};

print "not " unless $a{bcd} == 234;
print "ok 3\n";

print "not " unless (values %a)[0] == 234;
print "ok 4\n";

eval { $a{abcd} = 123 };
print "not " unless $@ =~ /Key "abcd" is not 3 characters long/;
print "ok 5\n";

eval { $a{abc} = 1234 };
print "not " unless $@ =~ /Value "1234" is not 3 characters long/;
print "ok 6\n";

eval { $a = $a{abcd}; $a++  };
print "not " unless $@ =~ /Key "abcd" is not 3 characters long/;
print "ok 7\n";

@a{qw(abc cde)} = qw(123 345); 

print "not " unless $a{cde} == 345;
print "ok 8\n";

eval { $a{def} = 456 };
print "not " unless $@ =~ /Table is full \(3 elements\)/;
print "ok 9\n";

%a = ();

print "not " unless keys %a == 0;
print "ok 10\n";

# Tests 11..16 by Linc Madison.

my $hashsize = 119;                # arbitrary values from my data
my %test;
tie %test, "Tie::SubstrHash", 13, 86, $hashsize;

for (my $i = 1; $i <= $hashsize; $i++) {
        my $key1 = $i + 100_000;           # fix to uniform 6-digit numbers
        my $key2 = "abcdefg$key1";
        $test{$key2} = ("abcdefgh" x 10) . "$key1";
}

for (my $i = 1; $i <= $hashsize; $i++) {
        my $key1 = $i + 100_000;
        my $key2 = "abcdefg$key1";
	unless ($test{$key2}) {
		print "not ";
		last;
	}
}
print "ok 11\n";

print "not " unless Tie::SubstrHash::findgteprime(1) == 2;
print "ok 12\n";

print "not " unless Tie::SubstrHash::findgteprime(2) == 2;
print "ok 13\n";

print "not " unless Tie::SubstrHash::findgteprime(5.5) == 7;
print "ok 14\n";

print "not " unless Tie::SubstrHash::findgteprime(13) == 13;
print "ok 15\n";

print "not " unless Tie::SubstrHash::findgteprime(13.000001) == 17;
print "ok 16\n";

print "not " unless Tie::SubstrHash::findgteprime(114) == 127;
print "ok 17\n";

print "not " unless Tie::SubstrHash::findgteprime(1000) == 1009;
print "ok 18\n";

print "not " unless Tie::SubstrHash::findgteprime(1024) == 1031;
print "ok 19\n";

print "not " unless Tie::SubstrHash::findgteprime(10000) == 10007;
print "ok 20\n";

