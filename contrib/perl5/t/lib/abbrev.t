#!./perl

print "1..7\n";

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Text::Abbrev;

print "ok 1\n";

# old style as reference
local(%x);
my @z = qw(list edit send abort gripe listen);
abbrev(*x, @z);
my $r = join ':', sort keys %x; 
print "not " if exists $x{'l'}   ||
                exists $x{'li'}  ||
                exists $x{'lis'};
print "ok 2\n";

print "not " unless $x{'list'}   eq 'list' &&
                    $x{'liste'}  eq 'listen' &&
                    $x{'listen'} eq 'listen';
print "ok 3\n";

print "not " unless $x{'a'}     eq 'abort' &&
                    $x{'ab'}    eq 'abort' &&
                    $x{'abo'}   eq 'abort' &&
                    $x{'abor'}  eq 'abort' &&
                    $x{'abort'} eq 'abort';
print "ok 4\n";

my $test = 5;

# wantarray
my %y = abbrev @z;
my $s = join ':', sort keys %y;
print (($r eq $s)?"ok $test\n":"not ok $test\n"); $test++;

my $y = abbrev @z;
$s = join ':', sort keys %$y;
print (($r eq $s)?"ok $test\n":"not ok $test\n"); $test++;

%y = ();
abbrev \%y, @z;

$s = join ':', sort keys %y;
print (($r eq $s)?"ok $test\n":"not ok $test\n"); $test++;
