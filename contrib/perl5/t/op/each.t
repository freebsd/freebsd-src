#!./perl

print "1..19\n";

$h{'abc'} = 'ABC';
$h{'def'} = 'DEF';
$h{'jkl','mno'} = "JKL\034MNO";
$h{'a',2,3,4,5} = join("\034",'A',2,3,4,5);
$h{'a'} = 'A';
$h{'b'} = 'B';
$h{'c'} = 'C';
$h{'d'} = 'D';
$h{'e'} = 'E';
$h{'f'} = 'F';
$h{'g'} = 'G';
$h{'h'} = 'H';
$h{'i'} = 'I';
$h{'j'} = 'J';
$h{'k'} = 'K';
$h{'l'} = 'L';
$h{'m'} = 'M';
$h{'n'} = 'N';
$h{'o'} = 'O';
$h{'p'} = 'P';
$h{'q'} = 'Q';
$h{'r'} = 'R';
$h{'s'} = 'S';
$h{'t'} = 'T';
$h{'u'} = 'U';
$h{'v'} = 'V';
$h{'w'} = 'W';
$h{'x'} = 'X';
$h{'y'} = 'Y';
$h{'z'} = 'Z';

@keys = keys %h;
@values = values %h;

if ($#keys == 29 && $#values == 29) {print "ok 1\n";} else {print "not ok 1\n";}

$i = 0;		# stop -w complaints

while (($key,$value) = each(%h)) {
    if ($key eq $keys[$i] && $value eq $values[$i]
        && (('a' lt 'A' && $key lt $value) || $key gt $value)) {
	$key =~ y/a-z/A-Z/;
	$i++ if $key eq $value;
    }
}

if ($i == 30) {print "ok 2\n";} else {print "not ok 2\n";}

@keys = ('blurfl', keys(%h), 'dyick');
if ($#keys == 31) {print "ok 3\n";} else {print "not ok 3\n";}

$size = ((split('/',scalar %h))[1]);
keys %h = $size * 5;
$newsize = ((split('/',scalar %h))[1]);
if ($newsize == $size * 8) {print "ok 4\n";} else {print "not ok 4\n";}
keys %h = 1;
$size = ((split('/',scalar %h))[1]);
if ($size == $newsize) {print "ok 5\n";} else {print "not ok 5\n";}
%h = (1,1);
$size = ((split('/',scalar %h))[1]);
if ($size == $newsize) {print "ok 6\n";} else {print "not ok 6\n";}
undef %h;
%h = (1,1);
$size = ((split('/',scalar %h))[1]);
if ($size == 8) {print "ok 7\n";} else {print "not ok 7\n";}

# test scalar each
%hash = 1..20;
$total = 0;
$total += $key while $key = each %hash;
print "# Scalar each is bad.\nnot " unless $total == 100;
print "ok 8\n";

for (1..3) { @foo = each %hash }
keys %hash;
$total = 0;
$total += $key while $key = each %hash;
print "# Scalar keys isn't resetting the iterator.\nnot " if $total != 100;
print "ok 9\n";

for (1..3) { @foo = each %hash }
$total = 0;
$total += $key while $key = each %hash;
print "# Iterator of each isn't being maintained.\nnot " if $total == 100;
print "ok 10\n";

for (1..3) { @foo = each %hash }
values %hash;
$total = 0;
$total += $key while $key = each %hash;
print "# Scalar values isn't resetting the iterator.\nnot " if $total != 100;
print "ok 11\n";

$size = (split('/', scalar %hash))[1];
keys(%hash) = $size / 2;
print "not " if $size != (split('/', scalar %hash))[1];
print "ok 12\n";
keys(%hash) = $size + 100;
print "not " if $size == (split('/', scalar %hash))[1];
print "ok 13\n";

print "not " if keys(%hash) != 10;
print "ok 14\n";

print keys(hash) != 10 ? "not ok 15\n" : "ok 15\n";

$i = 0;
%h = (a => A, b => B, c=> C, d => D, abc => ABC);
@keys = keys(h);
@values = values(h);
while (($key, $value) = each(h)) {
	if ($key eq $keys[$i] && $value eq $values[$i] && $key eq lc($value)) {
		$i++;
	}
}
if ($i == 5) { print "ok 16\n" } else { print "not ok\n" }

{
    package Obj;
    sub DESTROY { print "ok 18\n"; }
    {
	my $h = { A => bless [], __PACKAGE__ };
        while (my($k,$v) = each %$h) {
	    print "ok 17\n" if $k eq 'A' and ref($v) eq 'Obj';
	}
    }
    print "ok 19\n";
}

