#!./perl

print "1..56\n";

# Test glob operations.

$bar = "ok 1\n";
$foo = "ok 2\n";
{
    local(*foo) = *bar;
    print $foo;
}
print $foo;

$baz = "ok 3\n";
$foo = "ok 4\n";
{
    local(*foo) = 'baz';
    print $foo;
}
print $foo;

$foo = "ok 6\n";
{
    local(*foo);
    print $foo;
    $foo = "ok 5\n";
    print $foo;
}
print $foo;

# Test fake references.

$baz = "ok 7\n";
$bar = 'baz';
$foo = 'bar';
print $$$foo;

# Test real references.

$FOO = \$BAR;
$BAR = \$BAZ;
$BAZ = "ok 8\n";
print $$$FOO;

# Test references to real arrays.

@ary = (9,10,11,12);
$ref[0] = \@a;
$ref[1] = \@b;
$ref[2] = \@c;
$ref[3] = \@d;
for $i (3,1,2,0) {
    push(@{$ref[$i]}, "ok $ary[$i]\n");
}
print @a;
print ${$ref[1]}[0];
print @{$ref[2]}[0];
print @{'d'};

# Test references to references.

$refref = \\$x;
$x = "ok 13\n";
print $$$refref;

# Test nested anonymous lists.

$ref = [[],2,[3,4,5,]];
print scalar @$ref == 3 ? "ok 14\n" : "not ok 14\n";
print $$ref[1] == 2 ? "ok 15\n" : "not ok 15\n";
print ${$$ref[2]}[2] == 5 ? "ok 16\n" : "not ok 16\n";
print scalar @{$$ref[0]} == 0 ? "ok 17\n" : "not ok 17\n";

print $ref->[1] == 2 ? "ok 18\n" : "not ok 18\n";
print $ref->[2]->[0] == 3 ? "ok 19\n" : "not ok 19\n";

# Test references to hashes of references.

$refref = \%whatever;
$refref->{"key"} = $ref;
print $refref->{"key"}->[2]->[0] == 3 ? "ok 20\n" : "not ok 20\n";

# Test to see if anonymous subarrays spring into existence.

$spring[5]->[0] = 123;
$spring[5]->[1] = 456;
push(@{$spring[5]}, 789);
print join(':',@{$spring[5]}) eq "123:456:789" ? "ok 21\n" : "not ok 21\n";

# Test to see if anonymous subhashes spring into existence.

@{$spring2{"foo"}} = (1,2,3);
$spring2{"foo"}->[3] = 4;
print join(':',@{$spring2{"foo"}}) eq "1:2:3:4" ? "ok 22\n" : "not ok 22\n";

# Test references to subroutines.

sub mysub { print "ok 23\n" }
$subref = \&mysub;
&$subref;

$subrefref = \\&mysub2;
$$subrefref->("ok 24\n");
sub mysub2 { print shift }

# Test the ref operator.

print ref $subref	eq CODE  ? "ok 25\n" : "not ok 25\n";
print ref $ref		eq ARRAY ? "ok 26\n" : "not ok 26\n";
print ref $refref	eq HASH  ? "ok 27\n" : "not ok 27\n";

# Test anonymous hash syntax.

$anonhash = {};
print ref $anonhash	eq HASH  ? "ok 28\n" : "not ok 28\n";
$anonhash2 = {FOO => BAR, ABC => XYZ,};
print join('', sort values %$anonhash2) eq BARXYZ ? "ok 29\n" : "not ok 29\n";

# Test bless operator.

package MYHASH;

$object = bless $main'anonhash2;
print ref $object	eq MYHASH  ? "ok 30\n" : "not ok 30\n";
print $object->{ABC}	eq XYZ     ? "ok 31\n" : "not ok 31\n";

$object2 = bless {};
print ref $object2	eq MYHASH  ? "ok 32\n" : "not ok 32\n";

# Test ordinary call on object method.

&mymethod($object,33);

sub mymethod {
    local($THIS, @ARGS) = @_;
    die 'Got a "' . ref($THIS). '" instead of a MYHASH'
	unless ref $THIS eq MYHASH;
    print $THIS->{FOO} eq BAR  ? "ok $ARGS[0]\n" : "not ok $ARGS[0]\n";
}

# Test automatic destructor call.

$string = "not ok 34\n";
$object = "foo";
$string = "ok 34\n";
$main'anonhash2 = "foo";
$string = "";

DESTROY {
    return unless $string;
    print $string;

    # Test that the object has not already been "cursed".
    print ref shift ne HASH ? "ok 35\n" : "not ok 35\n";
}

# Now test inheritance of methods.

package OBJ;

@ISA = (BASEOBJ);

$main'object = bless {FOO => foo, BAR => bar};

package main;

# Test arrow-style method invocation.

print $object->doit("BAR") eq bar ? "ok 36\n" : "not ok 36\n";

# Test indirect-object-style method invocation.

$foo = doit $object "FOO";
print $foo eq foo ? "ok 37\n" : "not ok 37\n";

sub BASEOBJ'doit {
    local $ref = shift;
    die "Not an OBJ" unless ref $ref eq OBJ;
    $ref->{shift()};
}

package UNIVERSAL;
@ISA = 'LASTCHANCE';

package LASTCHANCE;
sub foo { print $_[1] }

package WHATEVER;
foo WHATEVER "ok 38\n";

#
# test the \(@foo) construct
#
package main;
@foo = (1,2,3);
@bar = \(@foo);
@baz = \(1,@foo,@bar);
print @bar == 3 ? "ok 39\n" : "not ok 39\n";
print grep(ref($_), @bar) == 3 ? "ok 40\n" : "not ok 40\n";
print @baz == 3 ? "ok 41\n" : "not ok 41\n";

my(@fuu) = (1,2,3);
my(@baa) = \(@fuu);
my(@bzz) = \(1,@fuu,@baa);
print @baa == 3 ? "ok 42\n" : "not ok 42\n";
print grep(ref($_), @baa) == 3 ? "ok 43\n" : "not ok 43\n";
print @bzz == 3 ? "ok 44\n" : "not ok 44\n";

# test for proper destruction of lexical objects

sub larry::DESTROY { print "# larry\nok 45\n"; }
sub curly::DESTROY { print "# curly\nok 46\n"; }
sub moe::DESTROY   { print "# moe\nok 47\n"; }

{
    my ($joe, @curly, %larry);
    my $moe = bless \$joe, 'moe';
    my $curly = bless \@curly, 'curly';
    my $larry = bless \%larry, 'larry';
    print "# leaving block\n";
}

print "# left block\n";

# another glob test

$foo = "not ok 48";
{ local(*bar) = "foo" }
$bar = "ok 48";
local(*bar) = *bar;
print "$bar\n";

$var = "ok 49";
$_   = \$var;
print $$_,"\n";

# test if reblessing during destruction results in more destruction

{
    package A;
    sub new { bless {}, shift }
    DESTROY { print "# destroying 'A'\nok 51\n" }
    package _B;
    sub new { bless {}, shift }
    DESTROY { print "# destroying '_B'\nok 50\n"; bless shift, 'A' }
    package main;
    my $b = _B->new;
}

# test if $_[0] is properly protected in DESTROY()

{
    my $i = 0;
    local $SIG{'__DIE__'} = sub {
	my $m = shift;
	if ($i++ > 4) {
	    print "# infinite recursion, bailing\nnot ok 52\n";
	    exit 1;
        }
	print "# $m";
	if ($m =~ /^Modification of a read-only/) { print "ok 52\n" }
    };
    package C;
    sub new { bless {}, shift }
    DESTROY { $_[0] = 'foo' }
    {
	print "# should generate an error...\n";
	my $c = C->new;
    }
    print "# good, didn't recurse\n";
}

# test if refgen behaves with autoviv magic

{
    my @a;
    $a[1] = "ok 53\n";
    print ${\$_} for @a;
}

# test global destruction

package FINALE;

{
    $ref3 = bless ["ok 56\n"];		# package destruction
    my $ref2 = bless ["ok 55\n"];	# lexical destruction
    local $ref1 = bless ["ok 54\n"];	# dynamic destruction
    1;					# flush any temp values on stack
}

DESTROY {
    print $_[0][0];
}
