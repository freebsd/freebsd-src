#!./perl

#
# test method calls and autoloading.
#

print "1..26\n";

@A::ISA = 'B';
@B::ISA = 'C';

sub C::d {"C::d"}
sub D::d {"D::d"}

my $cnt = 0;
sub test {
  print "# got `$_[0]', expected `$_[1]'\nnot " unless $_[0] eq $_[1]; 
  # print "not " unless shift eq shift;
  print "ok ", ++$cnt, "\n"
}

test( A->d, "C::d");		# Update hash table;

*B::d = \&D::d;			# Import now.
test (A->d, "D::d");		# Update hash table;

{
    local @A::ISA = qw(C);	# Update hash table with split() assignment
    test (A->d, "C::d");
    $#A::ISA = -1;
    test (eval { A->d } || "fail", "fail");
}
test (A->d, "D::d");

{
    local *B::d;
    eval 'sub B::d {"B::d1"}';	# Import now.
    test (A->d, "B::d1");	# Update hash table;
    undef &B::d;
    test ((eval { A->d }, ($@ =~ /Undefined subroutine/)), 1);
}

test (A->d, "D::d");		# Back to previous state

eval 'sub B::d {"B::d2"}';	# Import now.
test (A->d, "B::d2");		# Update hash table;

# What follows is hardly guarantied to work, since the names in scripts
# are already linked to "pruned" globs. Say, `undef &B::d' if it were
# after `delete $B::{d}; sub B::d {}' would reach an old subroutine.

undef &B::d;
delete $B::{d};
test (A->d, "C::d");		# Update hash table;

eval 'sub B::d {"B::d3"}';	# Import now.
test (A->d, "B::d3");		# Update hash table;

delete $B::{d};
*dummy::dummy = sub {};		# Mark as updated
test (A->d, "C::d");

eval 'sub B::d {"B::d4"}';	# Import now.
test (A->d, "B::d4");		# Update hash table;

delete $B::{d};			# Should work without any help too
test (A->d, "C::d");

{
    local *C::d;
    test (eval { A->d } || "nope", "nope");
}
test (A->d, "C::d");

*A::x = *A::d;			# See if cache incorrectly follows synonyms
A->d;
test (eval { A->x } || "nope", "nope");

eval <<'EOF';
sub C::e;
BEGIN { *B::e = \&C::e }	# Shouldn't prevent AUTOLOAD in original pkg
sub Y::f;
$counter = 0;

@X::ISA = 'Y';
@Y::ISA = 'B';

sub B::AUTOLOAD {
  my $c = ++$counter;
  my $method = $B::AUTOLOAD; 
  my $msg = "B: In $method, $c";
  eval "sub $method { \$msg }";
  goto &$method;
}
sub C::AUTOLOAD {
  my $c = ++$counter;
  my $method = $C::AUTOLOAD; 
  my $msg = "C: In $method, $c";
  eval "sub $method { \$msg }";
  goto &$method;
}
EOF

test(A->e(), "C: In C::e, 1");	# We get a correct autoload
test(A->e(), "C: In C::e, 1");	# Which sticks

test(A->ee(), "B: In A::ee, 2"); # We get a generic autoload, method in top
test(A->ee(), "B: In A::ee, 2"); # Which sticks

test(Y->f(), "B: In Y::f, 3");	# We vivify a correct method
test(Y->f(), "B: In Y::f, 3");	# Which sticks

# This test is not intended to be reasonable. It is here just to let you
# know that you broke some old construction. Feel free to rewrite the test
# if your patch breaks it.

*B::AUTOLOAD = sub {
  my $c = ++$counter;
  my $method = $AUTOLOAD; 
  *$AUTOLOAD = sub { "new B: In $method, $c" };
  goto &$AUTOLOAD;
};

test(A->eee(), "new B: In A::eee, 4");	# We get a correct $autoload
test(A->eee(), "new B: In A::eee, 4");	# Which sticks

# this test added due to bug discovery
test(defined(@{"unknown_package::ISA"}) ? "defined" : "undefined", "undefined");
