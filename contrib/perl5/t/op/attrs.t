#!./perl -w

# Regression tests for attributes.pm and the C< : attrs> syntax.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

sub NTESTS () ;

my ($test, $ntests);
BEGIN {$ntests=0}
$test=0;
my $failed = 0;

print "1..".NTESTS."\n";

$SIG{__WARN__} = sub { die @_ };

sub mytest {
    if (!$@ ne !$_[0] || $_[0] && $@ !~ $_[0]) {
	if ($@) {
	    my $x = $@;
	    $x =~ s/\n.*\z//s;
	    print "# Got: $x\n"
	}
	else {
	    print "# Got unexpected success\n";
	}
	if ($_[0]) {
	    print "# Expected: $_[0]\n";
	}
	else {
	    print "# Expected success\n";
	}
	$failed = 1;
	print "not ";
    }
    elsif (@_ == 3 && $_[1] ne $_[2]) {
	print "# Got: $_[1]\n";
	print "# Expected: $_[2]\n";
	$failed = 1;
	print "not ";
    }
    print "ok ",++$test,"\n";
}

eval 'sub t1 ($) : locked { $_[0]++ }';
mytest;
BEGIN {++$ntests}

eval 'sub t2 : locked { $_[0]++ }';
mytest;
BEGIN {++$ntests}

eval 'sub t3 ($) : locked ;';
mytest;
BEGIN {++$ntests}

eval 'sub t4 : locked ;';
mytest;
BEGIN {++$ntests}

my $anon1;
eval '$anon1 = sub ($) : locked:method { $_[0]++ }';
mytest;
BEGIN {++$ntests}

my $anon2;
eval '$anon2 = sub : locked : method { $_[0]++ }';
mytest;
BEGIN {++$ntests}

my $anon3;
eval '$anon3 = sub : method { $_[0]->[1] }';
mytest;
BEGIN {++$ntests}

eval 'sub e1 ($) : plugh ;';
mytest qr/^Invalid CODE attributes?: ["']?plugh["']? at/;
BEGIN {++$ntests}

eval 'sub e2 ($) : plugh(0,0) xyzzy ;';
mytest qr/^Invalid CODE attributes: ["']?plugh\(0,0\)["']? /;
BEGIN {++$ntests}

eval 'sub e3 ($) : plugh(0,0 xyzzy ;';
mytest qr/Unterminated attribute parameter in attribute list at/;
BEGIN {++$ntests}

eval 'sub e4 ($) : plugh + xyzzy ;';
mytest qr/Invalid separator character '[+]' in attribute list at/;
BEGIN {++$ntests}

eval 'my main $x : = 0;';
mytest;
BEGIN {++$ntests}

eval 'my $x : = 0;';
mytest;
BEGIN {++$ntests}

eval 'my $x ;';
mytest;
BEGIN {++$ntests}

eval 'my ($x) : = 0;';
mytest;
BEGIN {++$ntests}

eval 'my ($x) ;';
mytest;
BEGIN {++$ntests}

eval 'my ($x) : ;';
mytest;
BEGIN {++$ntests}

eval 'my ($x,$y) : = 0;';
mytest;
BEGIN {++$ntests}

eval 'my ($x,$y) ;';
mytest;
BEGIN {++$ntests}

eval 'my ($x,$y) : ;';
mytest;
BEGIN {++$ntests}

eval 'my ($x,$y) : plugh;';
mytest qr/^Invalid SCALAR attribute: ["']?plugh["']? at/;
BEGIN {++$ntests}

sub A::MODIFY_SCALAR_ATTRIBUTES { return }
eval 'my A $x : plugh;';
mytest qr/^SCALAR package attribute may clash with future reserved word: ["']?plugh["']? at/;
BEGIN {++$ntests}

eval 'my A $x : plugh plover;';
mytest qr/^SCALAR package attributes may clash with future reserved words: ["']?plugh["']? /;
BEGIN {++$ntests}

sub X::MODIFY_CODE_ATTRIBUTES { die "$_[0]" }
sub X::foo { 1 }
*Y::bar = \&X::foo;
*Y::bar = \&X::foo;	# second time for -w
eval 'package Z; sub Y::bar : locked';
mytest qr/^X at /;
BEGIN {++$ntests}

my @attrs = eval 'attributes::get \&Y::bar';
mytest '', "@attrs", "locked";
BEGIN {++$ntests}

@attrs = eval 'attributes::get $anon1';
mytest '', "@attrs", "locked method";
BEGIN {++$ntests}

sub Z::DESTROY { }
sub Z::FETCH_CODE_ATTRIBUTES { return 'Z' }
my $thunk = eval 'bless +sub : method locked { 1 }, "Z"';
mytest '', ref($thunk), "Z";
BEGIN {++$ntests}

@attrs = eval 'attributes::get $thunk';
mytest '', "@attrs", "locked method Z";
BEGIN {++$ntests}


# Other tests should be added above this line

sub NTESTS () { $ntests }

exit $failed;
