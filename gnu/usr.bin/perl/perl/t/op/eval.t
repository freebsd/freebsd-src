#!./perl

# $RCSfile: eval.t,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:30:02 $

print "1..16\n";

eval 'print "ok 1\n";';

if ($@ eq '') {print "ok 2\n";} else {print "not ok 2\n";}

eval "\$foo\n    = # this is a comment\n'ok 3';";
print $foo,"\n";

eval "\$foo\n    = # this is a comment\n'ok 4\n';";
print $foo;

print eval '
$foo =';		# this tests for a call through yyerror()
if ($@ =~ /line 2/) {print "ok 5\n";} else {print "not ok 5\n";}

print eval '$foo = /';	# this tests for a call through fatal()
if ($@ =~ /Search/) {print "ok 6\n";} else {print "not ok 6\n";}

print eval '"ok 7\n";';

# calculate a factorial with recursive evals

$foo = 5;
$fact = 'if ($foo <= 1) {1;} else {push(@x,$foo--); (eval $fact) * pop(@x);}';
$ans = eval $fact;
if ($ans == 120) {print "ok 8\n";} else {print "not ok 8\n";}

$foo = 5;
$fact = 'local($foo)=$foo; $foo <= 1 ? 1 : $foo-- * (eval $fact);';
$ans = eval $fact;
if ($ans == 120) {print "ok 9\n";} else {print "not ok 9 $ans\n";}

open(try,'>Op.eval');
print try 'print "ok 10\n"; unlink "Op.eval";',"\n";
close try;

do 'Op.eval'; print $@;

# Test the singlequoted eval optimizer

$i = 11;
for (1..3) {
    eval 'print "ok ", $i++, "\n"';
}

eval {
    print "ok 14\n";
    die "ok 16\n";
    1;
} || print "ok 15\n$@";


