#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Tie::Handle;
tie *tst,Tie::StdHandle;

$f = 'tst';

print "1..13\n";

# my $file tests

unlink("afile.new") if -f "afile";
print "$!\nnot " unless open($f,"+>afile") && open($f, "+<", "afile");
print "ok 1\n";
print "$!\nnot " unless binmode($f);
print "ok 2\n";
print "not " unless -f "afile";
print "ok 3\n";
print "not " unless print $f "SomeData\n";
print "ok 4\n";
print "not " unless tell($f) == 9;
print "ok 5\n";
print "not " unless printf $f "Some %d value\n",1234;
print "ok 6\n";
print "not " unless seek($f,0,0);
print "ok 7\n";
$b = <$f>;
print "not " unless $b eq "SomeData\n";
print "ok 8\n";
print "not " if eof($f);
print "ok 9\n";
read($f,($b=''),4);
print "'$b' not " unless $b eq 'Some';
print "ok 10\n";
print "not " unless getc($f) eq ' ';
print "ok 11\n";
$b = <$f>;
print "not " unless eof($f);
print "ok 12\n";
print "not " unless close($f);
print "ok 13\n";
unlink("afile");
