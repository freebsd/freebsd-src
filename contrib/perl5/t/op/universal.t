#!./perl
#
# check UNIVERSAL
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib' if -d '../lib';
}

print "1..72\n";

$a = {};
bless $a, "Bob";
print "not " unless $a->isa("Bob");
print "ok 1\n";

package Human;
sub eat {}

package Female;
@ISA=qw(Human);

package Alice;
@ISA=qw(Bob Female);
sub drink {}
sub new { bless {} }

$Alice::VERSION = 2.718;

package main;

my $i = 2;
sub test { print "not " unless shift; print "ok $i\n"; $i++; }

$a = new Alice;

test $a->isa("Alice");

test $a->isa("Bob");

test $a->isa("Female");

test $a->isa("Human");

test ! $a->isa("Male");

test $a->can("drink");

test $a->can("eat");

test ! $a->can("sleep");

my $b = 'abc';
my @refs = qw(SCALAR SCALAR     LVALUE      GLOB ARRAY HASH CODE);
my @vals = (  \$b,   \3.14, \substr($b,1,1), \*b,  [],  {}, sub {} );
for ($p=0; $p < @refs; $p++) {
    for ($q=0; $q < @vals; $q++) {
        test UNIVERSAL::isa($vals[$p], $refs[$q]) eq ($p==$q or $p+$q==1);
    };
};

test ! UNIVERSAL::can(23, "can");

test $a->can("VERSION");

test $a->can("can");
test ! $a->can("export_tags");	# a method in Exporter

test (eval { $a->VERSION }) == 2.718;

test ! (eval { $a->VERSION(2.719) }) &&
         $@ =~ /^Alice version 2.719 required--this is only version 2.718 at /;

test (eval { $a->VERSION(2.718) }) && ! $@;

my $subs = join ' ', sort grep { defined &{"UNIVERSAL::$_"} } keys %UNIVERSAL::;
if ('a' lt 'A') {
    test $subs eq "can isa VERSION";
} else {
    test $subs eq "VERSION can isa";
}

test $a->isa("UNIVERSAL");

# now use UNIVERSAL.pm and see what changes
eval "use UNIVERSAL";

test $a->isa("UNIVERSAL");

my $sub2 = join ' ', sort grep { defined &{"UNIVERSAL::$_"} } keys %UNIVERSAL::; 
# XXX import being here is really a bug
if ('a' lt 'A') {
    test $sub2 eq "can import isa VERSION";
} else {
    test $sub2 eq "VERSION can import isa";
}

eval 'sub UNIVERSAL::sleep {}';
test $a->can("sleep");

test ! UNIVERSAL::can($b, "can");

test ! $a->can("export_tags");	# a method in Exporter
