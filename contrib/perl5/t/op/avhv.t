#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

require Tie::Array;

package Tie::BasicArray;
@ISA = 'Tie::Array';
sub TIEARRAY  { bless [], $_[0] }
sub STORE     { $_[0]->[$_[1]] = $_[2] }
sub FETCH     { $_[0]->[$_[1]] }
sub FETCHSIZE { scalar(@{$_[0]})} 
sub STORESIZE { $#{$_[0]} = $_[1]+1 } 

package main;

print "1..28\n";

$sch = {
    'abc' => 1,
    'def' => 2,
    'jkl' => 3,
};

# basic normal array
$a = [];
$a->[0] = $sch;

$a->{'abc'} = 'ABC';
$a->{'def'} = 'DEF';
$a->{'jkl'} = 'JKL';

@keys = keys %$a;
@values = values %$a;

if ($#keys == 2 && $#values == 2) {print "ok 1\n";} else {print "not ok 1\n";}

$i = 0;		# stop -w complaints

while (($key,$value) = each %$a) {
    if ($key eq $keys[$i] && $value eq $values[$i] && $key eq lc($value)) {
	$key =~ y/a-z/A-Z/;
	$i++ if $key eq $value;
    }
}

if ($i == 3) {print "ok 2\n";} else {print "not ok 2\n";}

# quick check with tied array
tie @fake, 'Tie::StdArray';
$a = \@fake;
$a->[0] = $sch;

$a->{'abc'} = 'ABC';
if ($a->{'abc'} eq 'ABC') {print "ok 3\n";} else {print "not ok 3\n";}

# quick check with tied array
tie @fake, 'Tie::BasicArray';
$a = \@fake;
$a->[0] = $sch;

$a->{'abc'} = 'ABC';
if ($a->{'abc'} eq 'ABC') {print "ok 4\n";} else {print "not ok 4\n";}

# quick check with tied array & tied hash
require Tie::Hash;
tie %fake, Tie::StdHash;
%fake = %$sch;
$a->[0] = \%fake;

$a->{'abc'} = 'ABC';
if ($a->{'abc'} eq 'ABC') {print "ok 5\n";} else {print "not ok 5\n";}

# hash slice
my $slice = join('', 'x',@$a{'abc','def'},'x');
print "not " if $slice ne 'xABCx';
print "ok 6\n";

# evaluation in scalar context
my $avhv = [{}];
print "not " if %$avhv;
print "ok 7\n";

push @$avhv, "a";
print "not " if %$avhv;
print "ok 8\n";

$avhv = [];
eval { $a = %$avhv };
print "not " unless $@ and $@ =~ /^Can't coerce array into hash/;
print "ok 9\n";

$avhv = [{foo=>1, bar=>2}];
print "not " unless %$avhv =~ m,^\d+/\d+,;
print "ok 10\n";

# check if defelem magic works
sub f {
    print "not " unless $_[0] eq 'a';
    $_[0] = 'b';
    print "ok 11\n";
}
$a = [{key => 1}, 'a'];
f($a->{key});
print "not " unless $a->[1] eq 'b';
print "ok 12\n";

# check if exists() is behaving properly
$avhv = [{foo=>1,bar=>2,pants=>3}];
print "not " if exists $avhv->{bar};
print "ok 13\n";

$avhv->{pants} = undef;
print "not " unless exists $avhv->{pants};
print "ok 14\n";
print "not " if exists $avhv->{bar};
print "ok 15\n";

$avhv->{bar} = 10;
print "not " unless exists $avhv->{bar} and $avhv->{bar} == 10;
print "ok 16\n";

$v = delete $avhv->{bar};
print "not " unless $v == 10;
print "ok 17\n";

print "not " if exists $avhv->{bar};
print "ok 18\n";

$avhv->{foo} = 'xxx';
$avhv->{bar} = 'yyy';
$avhv->{pants} = 'zzz';
@x = delete @{$avhv}{'foo','pants'};
print "# @x\nnot " unless "@x" eq "xxx zzz";
print "ok 19\n";

print "not " unless "$avhv->{bar}" eq "yyy";
print "ok 20\n";

# hash assignment
%$avhv = ();
print "not " unless ref($avhv->[0]) eq 'HASH';
print "ok 21\n";

%hv = %$avhv;
print "not " if grep defined, values %hv;
print "ok 22\n";
print "not " if grep ref, keys %hv;
print "ok 23\n";

%$avhv = (foo => 29, pants => 2, bar => 0);
print "not " unless "@$avhv[1..3]" eq '29 0 2';
print "ok 24\n";

my $extra;
my @extra;
($extra, %$avhv) = ("moo", foo => 42, pants => 53, bar => "HIKE!");
print "not " unless "@$avhv[1..3]" eq '42 HIKE! 53' and $extra eq 'moo';
print "ok 25\n";

%$avhv = ();
(%$avhv, $extra) = (foo => 42, pants => 53, bar => "HIKE!");
print "not " unless "@$avhv[1..3]" eq '42 HIKE! 53' and !defined $extra;
print "ok 26\n";

@extra = qw(whatever and stuff);
%$avhv = ();
(%$avhv, @extra) = (foo => 42, pants => 53, bar => "HIKE!");
print "not " unless "@$avhv[1..3]" eq '42 HIKE! 53' and @extra == 0;
print "ok 27\n";

%$avhv = ();
(@extra, %$avhv) = (foo => 42, pants => 53, bar => "HIKE!");
print "not " unless ref $avhv->[0] eq 'HASH' and @extra == 6;
print "ok 28\n";
