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

print "1..12\n";

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

