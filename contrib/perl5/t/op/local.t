#!./perl

print "1..69\n";

sub foo {
    local($a, $b) = @_;
    local($c, $d);
    $c = "ok 3\n";
    $d = "ok 4\n";
    { local($a,$c) = ("ok 9\n", "ok 10\n"); ($x, $y) = ($a, $c); }
    print $a, $b;
    $c . $d;
}

$a = "ok 5\n";
$b = "ok 6\n";
$c = "ok 7\n";
$d = "ok 8\n";

print &foo("ok 1\n","ok 2\n");

print $a,$b,$c,$d,$x,$y;

# same thing, only with arrays and associative arrays

sub foo2 {
    local($a, @b) = @_;
    local(@c, %d);
    @c = "ok 13\n";
    $d{''} = "ok 14\n";
    { local($a,@c) = ("ok 19\n", "ok 20\n"); ($x, $y) = ($a, @c); }
    print $a, @b;
    $c[0] . $d{''};
}

$a = "ok 15\n";
@b = "ok 16\n";
@c = "ok 17\n";
$d{''} = "ok 18\n";

print &foo2("ok 11\n","ok 12\n");

print $a,@b,@c,%d,$x,$y;

eval 'local($$e)';
print +($@ =~ /Can't localize through a reference/) ? "" : "not ", "ok 21\n";

eval 'local(@$e)';
print +($@ =~ /Can't localize through a reference/) ? "" : "not ", "ok 22\n";

eval 'local(%$e)';
print +($@ =~ /Can't localize through a reference/) ? "" : "not ", "ok 23\n";

# Array and hash elements

@a = ('a', 'b', 'c');
{
    local($a[1]) = 'foo';
    local($a[2]) = $a[2];
    print +($a[1] eq 'foo') ? "" : "not ", "ok 24\n";
    print +($a[2] eq 'c') ? "" : "not ", "ok 25\n";
    undef @a;
}
print +($a[1] eq 'b') ? "" : "not ", "ok 26\n";
print +($a[2] eq 'c') ? "" : "not ", "ok 27\n";
print +(!defined $a[0]) ? "" : "not ", "ok 28\n";

@a = ('a', 'b', 'c');
{
    local($a[1]) = "X";
    shift @a;
}
print +($a[0].$a[1] eq "Xb") ? "" : "not ", "ok 29\n";

%h = ('a' => 1, 'b' => 2, 'c' => 3);
{
    local($h{'a'}) = 'foo';
    local($h{'b'}) = $h{'b'};
    print +($h{'a'} eq 'foo') ? "" : "not ", "ok 30\n";
    print +($h{'b'} == 2) ? "" : "not ", "ok 31\n";
    local($h{'c'});
    delete $h{'c'};
}
print +($h{'a'} == 1) ? "" : "not ", "ok 32\n";
print +($h{'b'} == 2) ? "" : "not ", "ok 33\n";
print +($h{'c'} == 3) ? "" : "not ", "ok 34\n";

# check for scope leakage
$a = 'outer';
if (1) { local $a = 'inner' }
print +($a eq 'outer') ? "" : "not ", "ok 35\n";

# see if localization works when scope unwinds
local $m = 5;
eval {
    for $m (6) {
	local $m = 7;
	die "bye";
    }
};
print $m == 5 ? "" : "not ", "ok 36\n";

# see if localization works on tied arrays
{
    package TA;
    sub TIEARRAY { bless [], $_[0] }
    sub STORE { print "# STORE [@_]\n"; $_[0]->[$_[1]] = $_[2] }
    sub FETCH { my $v = $_[0]->[$_[1]]; print "# FETCH [@_=$v]\n"; $v }
    sub CLEAR { print "# CLEAR [@_]\n"; @{$_[0]} = (); }
    sub FETCHSIZE { scalar(@{$_[0]}) }
    sub SHIFT { shift (@{$_[0]}) }
    sub EXTEND {}
}

tie @a, 'TA';
@a = ('a', 'b', 'c');
{
    local($a[1]) = 'foo';
    local($a[2]) = $a[2];
    print +($a[1] eq 'foo') ? "" : "not ", "ok 37\n";
    print +($a[2] eq 'c') ? "" : "not ", "ok 38\n";
    @a = ();
}
print +($a[1] eq 'b') ? "" : "not ", "ok 39\n";
print +($a[2] eq 'c') ? "" : "not ", "ok 40\n";
print +(!defined $a[0]) ? "" : "not ", "ok 41\n";

{
    package TH;
    sub TIEHASH { bless {}, $_[0] }
    sub STORE { print "# STORE [@_]\n"; $_[0]->{$_[1]} = $_[2] }
    sub FETCH { my $v = $_[0]->{$_[1]}; print "# FETCH [@_=$v]\n"; $v }
    sub DELETE { print "# DELETE [@_]\n"; delete $_[0]->{$_[1]}; }
    sub CLEAR { print "# CLEAR [@_]\n"; %{$_[0]} = (); }
}

# see if localization works on tied hashes
tie %h, 'TH';
%h = ('a' => 1, 'b' => 2, 'c' => 3);

{
    local($h{'a'}) = 'foo';
    local($h{'b'}) = $h{'b'};
    print +($h{'a'} eq 'foo') ? "" : "not ", "ok 42\n";
    print +($h{'b'} == 2) ? "" : "not ", "ok 43\n";
    local($h{'c'});
    delete $h{'c'};
}
print +($h{'a'} == 1) ? "" : "not ", "ok 44\n";
print +($h{'b'} == 2) ? "" : "not ", "ok 45\n";
print +($h{'c'} == 3) ? "" : "not ", "ok 46\n";

@a = ('a', 'b', 'c');
{
    local($a[1]) = "X";
    shift @a;
}
print +($a[0].$a[1] eq "Xb") ? "" : "not ", "ok 47\n";

# now try the same for %SIG

$SIG{TERM} = 'foo';
$SIG{INT} = \&foo;
$SIG{__WARN__} = $SIG{INT};
{
    local($SIG{TERM}) = $SIG{TERM};
    local($SIG{INT}) = $SIG{INT};
    local($SIG{__WARN__}) = $SIG{__WARN__};
    print +($SIG{TERM}		eq 'main::foo') ? "" : "not ", "ok 48\n";
    print +($SIG{INT}		eq \&foo) ? "" : "not ", "ok 49\n";
    print +($SIG{__WARN__}	eq \&foo) ? "" : "not ", "ok 50\n";
    local($SIG{INT});
    delete $SIG{__WARN__};
}
print +($SIG{TERM}	eq 'main::foo') ? "" : "not ", "ok 51\n";
print +($SIG{INT}	eq \&foo) ? "" : "not ", "ok 52\n";
print +($SIG{__WARN__}	eq \&foo) ? "" : "not ", "ok 53\n";

# and for %ENV

$ENV{_X_} = 'a';
$ENV{_Y_} = 'b';
$ENV{_Z_} = 'c';
{
    local($ENV{_X_}) = 'foo';
    local($ENV{_Y_}) = $ENV{_Y_};
    print +($ENV{_X_} eq 'foo') ? "" : "not ", "ok 54\n";
    print +($ENV{_Y_} eq 'b') ? "" : "not ", "ok 55\n";
    local($ENV{_Z_});
    delete $ENV{_Z_};
}
print +($ENV{_X_} eq 'a') ? "" : "not ", "ok 56\n";
print +($ENV{_Y_} eq 'b') ? "" : "not ", "ok 57\n";
print +($ENV{_Z_} eq 'c') ? "" : "not ", "ok 58\n";

# does implicit localization in foreach skip magic?

$_ = "ok 59,ok 60,";
my $iter = 0;
while (/(o.+?),/gc) {
    print "$1\n";
    foreach (1..1) { $iter++ }
    if ($iter > 2) { print "not ok 60\n"; last; }
}

{
    package UnderScore;
    sub TIESCALAR { bless \my $self, shift }
    sub FETCH { die "read  \$_ forbidden" }
    sub STORE { die "write \$_ forbidden" }
    tie $_, __PACKAGE__;
    my $t = 61;
    my @tests = (
	"Nesting"     => sub { print '#'; for (1..3) { print }
			       print "\n" },			1,
	"Reading"     => sub { print },				0,
	"Matching"    => sub { $x = /badness/ },		0,
	"Concat"      => sub { $_ .= "a" },			0,
	"Chop"        => sub { chop },				0,
	"Filetest"    => sub { -x },				0,
	"Assignment"  => sub { $_ = "Bad" },			0,
	# XXX whether next one should fail is debatable
	"Local \$_"   => sub { local $_  = 'ok?'; print },	0,
	"for local"   => sub { for("#ok?\n"){ print } },	1,
    );
    while ( ($name, $code, $ok) = splice(@tests, 0, 3) ) {
	print "# Testing $name\n";
	eval { &$code };
	print(($ok xor $@) ? "ok $t\n" : "not ok $t\n");
	++$t;
    }
    untie $_;
}

