#!./perl

# $RCSfile: eval.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:48 $

print "1..23\n";

eval 'print "ok 1\n";';

if ($@ eq '') {print "ok 2\n";} else {print "not ok 2\n";}

eval "\$foo\n    = # this is a comment\n'ok 3';";
print $foo,"\n";

eval "\$foo\n    = # this is a comment\n'ok 4\n';";
print $foo;

print eval '
$foo =;';		# this tests for a call through yyerror()
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

# check whether eval EXPR determines value of EXPR correctly

{
  my @a = qw(a b c d);
  my @b = eval @a;
  print "@b" eq '4' ? "ok 17\n" : "not ok 17\n";
  print $@ ? "not ok 18\n" : "ok 18\n";

  my $a = q[defined(wantarray) ? (wantarray ? ($b='A') : ($b='S')) : ($b='V')];
  my $b;
  @a = eval $a;
  print "@a" eq 'A' ? "ok 19\n" : "# $b\nnot ok 19\n";
  print   $b eq 'A' ? "ok 20\n" : "# $b\nnot ok 20\n";
  $_ = eval $a;
  print   $b eq 'S' ? "ok 21\n" : "# $b\nnot ok 21\n";
  eval $a;
  print   $b eq 'V' ? "ok 22\n" : "# $b\nnot ok 22\n";

  $b = 'wrong';
  $x = sub {
     my $b = "right";
     print eval('"$b"') eq $b ? "ok 23\n" : "not ok 23\n";
  };
  &$x();
}
