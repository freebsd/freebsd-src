#!./perl

print "1..6\n";

@x = (1, 2, 3);
if (join(':',@x) eq '1:2:3') {print "ok 1\n";} else {print "not ok 1\n";}

if (join('',1,2,3) eq '123') {print "ok 2\n";} else {print "not ok 2\n";}

if (join(':',split(/ /,"1 2 3")) eq '1:2:3') {print "ok 3\n";} else {print "not ok 3\n";}

my $f = 'a';
$f = join ',', 'b', $f, 'e';
if ($f eq 'b,a,e') {print "ok 4\n";} else {print "# '$f'\nnot ok 4\n";}

$f = 'a';
$f = join ',', $f, 'b', 'e';
if ($f eq 'a,b,e') {print "ok 5\n";} else {print "not ok 5\n";}

$f = 'a';
$f = join $f, 'b', 'e', 'k';
if ($f eq 'baeak') {print "ok 6\n";} else {print "# '$f'\nnot ok 6\n";}
