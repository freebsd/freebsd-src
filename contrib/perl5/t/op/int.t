#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..7\n";

# compile time evaluation

if (int(1.234) == 1) {print "ok 1\n";} else {print "not ok 1\n";}

if (int(-1.234) == -1) {print "ok 2\n";} else {print "not ok 2\n";}

# run time evaluation

$x = 1.234;
if (int($x) == 1) {print "ok 3\n";} else {print "not ok 3\n";}
if (int(-$x) == -1) {print "ok 4\n";} else {print "not ok 4\n";}

$x = length("abc") % -10;
print $x == -7 ? "ok 5\n" : "# expected -7, got $x\nnot ok 5\n";

{
    use integer;
    $x = length("abc") % -10;
    $y = (3/-10)*-10;
    print $x+$y == 3 && abs($x) < 10 ? "ok 6\n" : "not ok 6\n";
}

# check bad strings still get converted

@x = ( 6, 8, 10);
print "not " if $x["1foo"] != 8;
print "ok 7\n";
