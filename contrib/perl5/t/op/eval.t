#!./perl

print "1..40\n";

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

my $b = 'wrong';
my $X = sub {
   my $b = "right";
   print eval('"$b"') eq $b ? "ok 24\n" : "not ok 24\n";
};
&$X();


# check navigation of multiple eval boundaries to find lexicals

my $x = 25;
eval <<'EOT'; die if $@;
  print "# $x\n";	# clone into eval's pad
  sub do_eval1 {
     eval $_[0]; die if $@;
  }
EOT
do_eval1('print "ok $x\n"');
$x++;
do_eval1('eval q[print "ok $x\n"]');
$x++;
do_eval1('sub { eval q[print "ok $x\n"] }->()');
$x++;

# calls from within eval'' should clone outer lexicals

eval <<'EOT'; die if $@;
  sub do_eval2 {
     eval $_[0]; die if $@;
  }
do_eval2('print "ok $x\n"');
$x++;
do_eval2('eval q[print "ok $x\n"]');
$x++;
do_eval2('sub { eval q[print "ok $x\n"] }->()');
$x++;
EOT

# calls outside eval'' should NOT clone lexicals from called context

$main::x = 'ok';
eval <<'EOT'; die if $@;
  # $x unbound here
  sub do_eval3 {
     eval $_[0]; die if $@;
  }
EOT
do_eval3('print "$x ' . $x . '\n"');
$x++;
do_eval3('eval q[print "$x ' . $x . '\n"]');
$x++;
do_eval3('sub { eval q[print "$x ' . $x . '\n"] }->()');
$x++;

# can recursive subroutine-call inside eval'' see its own lexicals?
sub recurse {
  my $l = shift;
  if ($l < $x) {
     ++$l;
     eval 'print "# level $l\n"; recurse($l);';
     die if $@;
  }
  else {
    print "ok $l\n";
  }
}
{
  local $SIG{__WARN__} = sub { die "not ok $x\n" if $_[0] =~ /^Deep recurs/ };
  recurse($x-5);
}
$x++;

# do closures created within eval bind correctly?
eval <<'EOT';
  sub create_closure {
    my $self = shift;
    return sub {
       print $self;
    };
  }
EOT
create_closure("ok $x\n")->();
$x++;

# does lexical search terminate correctly at subroutine boundary?
$main::r = "ok $x\n";
sub terminal { eval 'print $r' }
{
   my $r = "not ok $x\n";
   eval 'terminal($r)';
}
$x++;

# Have we cured panic which occurred with require/eval in die handler ?
$SIG{__DIE__} = sub { eval {1}; die shift }; 
eval { die "ok ".$x++,"\n" }; 
print $@;

# does scalar eval"" pop stack correctly?
{
    my $c = eval "(1,2)x10";
    print $c eq '2222222222' ? "ok $x\n" : "# $c\nnot ok $x\n";
    $x++;
}

# return from eval {} should clear $@ correctly
{
    my $status = eval {
	eval { die };
	print "# eval { return } test\n";
	return; # removing this changes behavior
    };
    print "not " if $@;
    print "ok $x\n";
    $x++;
}

# ditto for eval ""
{
    my $status = eval q{
	eval q{ die };
	print "# eval q{ return } test\n";
	return; # removing this changes behavior
    };
    print "not " if $@;
    print "ok $x\n";
    $x++;
}
