#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

package Oscalar;
use overload ( 
				# Anonymous subroutines:
'+'	=>	sub {new Oscalar $ {$_[0]}+$_[1]},
'-'	=>	sub {new Oscalar
		       $_[2]? $_[1]-${$_[0]} : ${$_[0]}-$_[1]},
'<=>'	=>	sub {new Oscalar
		       $_[2]? $_[1]-${$_[0]} : ${$_[0]}-$_[1]},
'cmp'	=>	sub {new Oscalar
		       $_[2]? ($_[1] cmp ${$_[0]}) : (${$_[0]} cmp $_[1])},
'*'	=>	sub {new Oscalar ${$_[0]}*$_[1]},
'/'	=>	sub {new Oscalar 
		       $_[2]? $_[1]/${$_[0]} :
			 ${$_[0]}/$_[1]},
'%'	=>	sub {new Oscalar
		       $_[2]? $_[1]%${$_[0]} : ${$_[0]}%$_[1]},
'**'	=>	sub {new Oscalar
		       $_[2]? $_[1]**${$_[0]} : ${$_[0]}-$_[1]},

qw(
""	stringify
0+	numify)			# Order of arguments unsignificant
);

sub new {
  my $foo = $_[1];
  bless \$foo, $_[0];
}

sub stringify { "${$_[0]}" }
sub numify { 0 + "${$_[0]}" }	# Not needed, additional overhead
				# comparing to direct compilation based on
				# stringify

package main;

$test = 0;
$| = 1;
print "1..",&last,"\n";

sub test {
  $test++; 
  if (@_ > 1) {
    if ($_[0] eq $_[1]) {
      print "ok $test\n";
    } else {
      print "not ok $test: '$_[0]' ne '$_[1]'\n";
    }
  } else {
    if (shift) {
      print "ok $test\n";
    } else {
      print "not ok $test\n";
    } 
  }
}

$a = new Oscalar "087";
$b= "$a";

# All test numbers in comments are off by 1.
# So much for hard-wiring them in :-) To fix this:
test(1);			# 1

test ($b eq $a);		# 2
test ($b eq "087");		# 3
test (ref $a eq "Oscalar");	# 4
test ($a eq $a);		# 5
test ($a eq "087");		# 6

$c = $a + 7;

test (ref $c eq "Oscalar");	# 7
test (!($c eq $a));		# 8
test ($c eq "94");		# 9

$b=$a;

test (ref $a eq "Oscalar");	# 10

$b++;

test (ref $b eq "Oscalar");	# 11
test ( $a eq "087");		# 12
test ( $b eq "88");		# 13
test (ref $a eq "Oscalar");	# 14

$c=$b;
$c-=$a;

test (ref $c eq "Oscalar");	# 15
test ( $a eq "087");		# 16
test ( $c eq "1");		# 17
test (ref $a eq "Oscalar");	# 18

$b=1;
$b+=$a;

test (ref $b eq "Oscalar");	# 19
test ( $a eq "087");		# 20
test ( $b eq "88");		# 21
test (ref $a eq "Oscalar");	# 22

eval q[ package Oscalar; use overload ('++' => sub { $ {$_[0]}++;$_[0] } ) ];

$b=$a;

test (ref $a eq "Oscalar");	# 23

$b++;

test (ref $b eq "Oscalar");	# 24
test ( $a eq "087");		# 25
test ( $b eq "88");		# 26
test (ref $a eq "Oscalar");	# 27

package Oscalar;
$dummy=bless \$dummy;		# Now cache of method should be reloaded
package main;

$b=$a;
$b++;				

test (ref $b eq "Oscalar");	# 28
test ( $a eq "087");		# 29
test ( $b eq "88");		# 30
test (ref $a eq "Oscalar");	# 31


eval q[package Oscalar; use overload ('++' => sub { $ {$_[0]} += 2; $_[0] } ) ];

$b=$a;

test (ref $a eq "Oscalar");	# 32

$b++;

test (ref $b eq "Oscalar");	# 33
test ( $a eq "087");		# 34
test ( $b eq "88");		# 35
test (ref $a eq "Oscalar");	# 36

package Oscalar;
$dummy=bless \$dummy;		# Now cache of method should be reloaded
package main;

$b++;				

test (ref $b eq "Oscalar");	# 37
test ( $a eq "087");		# 38
test ( $b eq "90");		# 39
test (ref $a eq "Oscalar");	# 40

$b=$a;
$b++;

test (ref $b eq "Oscalar");	# 41
test ( $a eq "087");		# 42
test ( $b eq "89");		# 43
test (ref $a eq "Oscalar");	# 44


test ($b? 1:0);			# 45

eval q[ package Oscalar; use overload ('=' => sub {$main::copies++; 
						   package Oscalar;
						   local $new=$ {$_[0]};
						   bless \$new } ) ];

$b=new Oscalar "$a";

test (ref $b eq "Oscalar");	# 46
test ( $a eq "087");		# 47
test ( $b eq "087");		# 48
test (ref $a eq "Oscalar");	# 49

$b++;

test (ref $b eq "Oscalar");	# 50
test ( $a eq "087");		# 51
test ( $b eq "89");		# 52
test (ref $a eq "Oscalar");	# 53
test ($copies == 0);		# 54

$b+=1;

test (ref $b eq "Oscalar");	# 55
test ( $a eq "087");		# 56
test ( $b eq "90");		# 57
test (ref $a eq "Oscalar");	# 58
test ($copies == 0);		# 59

$b=$a;
$b+=1;

test (ref $b eq "Oscalar");	# 60
test ( $a eq "087");		# 61
test ( $b eq "88");		# 62
test (ref $a eq "Oscalar");	# 63
test ($copies == 0);		# 64

$b=$a;
$b++;

test (ref $b eq "Oscalar") || print ref $b,"=ref(b)\n";	# 65
test ( $a eq "087");		# 66
test ( $b eq "89");		# 67
test (ref $a eq "Oscalar");	# 68
test ($copies == 1);		# 69

eval q[package Oscalar; use overload ('+=' => sub {$ {$_[0]} += 3*$_[1];
						   $_[0] } ) ];
$c=new Oscalar;			# Cause rehash

$b=$a;
$b+=1;

test (ref $b eq "Oscalar");	# 70
test ( $a eq "087");		# 71
test ( $b eq "90");		# 72
test (ref $a eq "Oscalar");	# 73
test ($copies == 2);		# 74

$b+=$b;

test (ref $b eq "Oscalar");	# 75
test ( $b eq "360");		# 76
test ($copies == 2);		# 77
$b=-$b;

test (ref $b eq "Oscalar");	# 78
test ( $b eq "-360");		# 79
test ($copies == 2);		# 80

$b=abs($b);

test (ref $b eq "Oscalar");	# 81
test ( $b eq "360");		# 82
test ($copies == 2);		# 83

$b=abs($b);

test (ref $b eq "Oscalar");	# 84
test ( $b eq "360");		# 85
test ($copies == 2);		# 86

eval q[package Oscalar; 
       use overload ('x' => sub {new Oscalar ( $_[2] ? "_.$_[1]._" x $ {$_[0]}
					      : "_.${$_[0]}._" x $_[1])}) ];

$a=new Oscalar "yy";
$a x= 3;
test ($a eq "_.yy.__.yy.__.yy._"); # 87

eval q[package Oscalar; 
       use overload ('.' => sub {new Oscalar ( $_[2] ? 
					      "_.$_[1].__.$ {$_[0]}._"
					      : "_.$ {$_[0]}.__.$_[1]._")}) ];

$a=new Oscalar "xx";

test ("b${a}c" eq "_._.b.__.xx._.__.c._"); # 88

# Check inheritance of overloading;
{
  package OscalarI;
  @ISA = 'Oscalar';
}

$aI = new OscalarI "$a";
test (ref $aI eq "OscalarI");	# 89
test ("$aI" eq "xx");		# 90
test ($aI eq "xx");		# 91
test ("b${aI}c" eq "_._.b.__.xx._.__.c._");		# 92

# Here we test blessing to a package updates hash

eval "package Oscalar; no overload '.'";

test ("b${a}" eq "_.b.__.xx._"); # 93
$x="1";
bless \$x, Oscalar;
test ("b${a}c" eq "bxxc");	# 94
new Oscalar 1;
test ("b${a}c" eq "bxxc");	# 95

# Negative overloading:

$na = eval { ~$a };
test($@ =~ /no method found/);	# 96

# Check AUTOLOADING:

*Oscalar::AUTOLOAD = 
  sub { *{"Oscalar::$AUTOLOAD"} = sub {"_!_" . shift() . "_!_"} ;
	goto &{"Oscalar::$AUTOLOAD"}};

eval "package Oscalar; sub comple; use overload '~' => 'comple'";

$na = eval { ~$a };		# Hash was not updated
test($@ =~ /no method found/);	# 97

bless \$x, Oscalar;

$na = eval { ~$a };		# Hash updated
warn "`$na', $@" if $@;
test !$@;			# 98
test($na eq '_!_xx_!_');	# 99

$na = 0;

$na = eval { ~$aI };		# Hash was not updated
test($@ =~ /no method found/);	# 100

bless \$x, OscalarI;

$na = eval { ~$aI };
print $@;

test !$@;			# 101
test($na eq '_!_xx_!_');	# 102

eval "package Oscalar; sub rshft; use overload '>>' => 'rshft'";

$na = eval { $aI >> 1 };	# Hash was not updated
test($@ =~ /no method found/);	# 103

bless \$x, OscalarI;

$na = 0;

$na = eval { $aI >> 1 };
print $@;

test !$@;			# 104
test($na eq '_!_xx_!_');	# 105

# warn overload::Method($a, '0+'), "\n";
test (overload::Method($a, '0+') eq \&Oscalar::numify); # 106
test (overload::Method($aI,'0+') eq \&Oscalar::numify); # 107
test (overload::Overloaded($aI)); # 108
test (!overload::Overloaded('overload')); # 109

test (! defined overload::Method($aI, '<<')); # 110
test (! defined overload::Method($a, '<')); # 111

test (overload::StrVal($aI) =~ /^OscalarI=SCALAR\(0x[\da-fA-F]+\)$/); # 112
test (overload::StrVal(\$aI) eq "@{[\$aI]}"); # 113

# Check overloading by methods (specified deep in the ISA tree).
{
  package OscalarII;
  @ISA = 'OscalarI';
  sub Oscalar::lshft {"_<<_" . shift() . "_<<_"}
  eval "package OscalarI; use overload '<<' => 'lshft', '|' => 'lshft'";
}

$aaII = "087";
$aII = \$aaII;
bless $aII, 'OscalarII';
bless \$fake, 'OscalarI';		# update the hash
test(($aI | 3) eq '_<<_xx_<<_');	# 114
# warn $aII << 3;
test(($aII << 3) eq '_<<_087_<<_');	# 115

{
  BEGIN { $int = 7; overload::constant 'integer' => sub {$int++; shift}; }
  $out = 2**10;
}
test($int, 9);		# 116
test($out, 1024);		# 117

$foo = 'foo';
$foo1 = 'f\'o\\o';
{
  BEGIN { $q = $qr = 7; 
	  overload::constant 'q' => sub {$q++; push @q, shift, ($_[1] || 'none'); shift},
			     'qr' => sub {$qr++; push @qr, shift, ($_[1] || 'none'); shift}; }
  $out = 'foo';
  $out1 = 'f\'o\\o';
  $out2 = "a\a$foo,\,";
  /b\b$foo.\./;
}

test($out, 'foo');		# 118
test($out, $foo);		# 119
test($out1, 'f\'o\\o');		# 120
test($out1, $foo1);		# 121
test($out2, "a\afoo,\,");	# 122
test("@q", "foo q f'o\\\\o q a\\a qq ,\\, qq");	# 123
test($q, 11);			# 124
test("@qr", "b\\b qq .\\. qq");	# 125
test($qr, 9);			# 126

{
  $_ = '!<b>!foo!<-.>!';
  BEGIN { overload::constant 'q' => sub {push @q1, shift, ($_[1] || 'none'); "_<" . (shift) . ">_"},
			     'qr' => sub {push @qr1, shift, ($_[1] || 'none'); "!<" . (shift) . ">!"}; }
  $out = 'foo';
  $out1 = 'f\'o\\o';
  $out2 = "a\a$foo,\,";
  $res = /b\b$foo.\./;
  $a = <<EOF;
oups
EOF
  $b = <<'EOF';
oups1
EOF
  $c = bareword;
  m'try it';
  s'first part'second part';
  s/yet another/tail here/;
  tr/z-Z/z-Z/;
}

test($out, '_<foo>_');		# 117
test($out1, '_<f\'o\\o>_');		# 128
test($out2, "_<a\a>_foo_<,\,>_");	# 129
test("@q1", "foo q f'o\\\\o q a\\a qq ,\\, qq oups
 qq oups1
 q second part q tail here s z-Z tr z-Z tr");	# 130
test("@qr1", "b\\b qq .\\. qq try it q first part q yet another qq");	# 131
test($res, 1);			# 132
test($a, "_<oups
>_");	# 133
test($b, "_<oups1
>_");	# 134
test($c, "bareword");	# 135

{
  package symbolic;		# Primitive symbolic calculator
  use overload nomethod => \&wrap, '""' => \&str, '0+' => \&num,
      '=' => \&cpy, '++' => \&inc, '--' => \&dec;

  sub new { shift; bless ['n', @_] }
  sub cpy {
    my $self = shift;
    bless [@$self], ref $self;
  }
  sub inc { $_[0] = bless ['++', $_[0], 1]; }
  sub dec { $_[0] = bless ['--', $_[0], 1]; }
  sub wrap {
    my ($obj, $other, $inv, $meth) = @_;
    if ($meth eq '++' or $meth eq '--') {
      @$obj = ($meth, (bless [@$obj]), 1); # Avoid circular reference
      return $obj;
    }
    ($obj, $other) = ($other, $obj) if $inv;
    bless [$meth, $obj, $other];
  }
  sub str {
    my ($meth, $a, $b) = @{+shift};
    $a = 'u' unless defined $a;
    if (defined $b) {
      "[$meth $a $b]";
    } else {
      "[$meth $a]";
    }
  } 
  my %subr = ( 'n' => sub {$_[0]} );
  foreach my $op (split " ", $overload::ops{with_assign}) {
    $subr{$op} = $subr{"$op="} = eval "sub {shift() $op shift()}";
  }
  my @bins = qw(binary 3way_comparison num_comparison str_comparison);
  foreach my $op (split " ", "@overload::ops{ @bins }") {
    $subr{$op} = eval "sub {shift() $op shift()}";
  }
  foreach my $op (split " ", "@overload::ops{qw(unary func)}") {
    $subr{$op} = eval "sub {$op shift()}";
  }
  $subr{'++'} = $subr{'+'};
  $subr{'--'} = $subr{'-'};
  
  sub num {
    my ($meth, $a, $b) = @{+shift};
    my $subr = $subr{$meth} 
      or die "Do not know how to ($meth) in symbolic";
    $a = $a->num if ref $a eq __PACKAGE__;
    $b = $b->num if ref $b eq __PACKAGE__;
    $subr->($a,$b);
  }
  sub TIESCALAR { my $pack = shift; $pack->new(@_) }
  sub FETCH { shift }
  sub nop {  }		# Around a bug
  sub vars { my $p = shift; tie($_, $p), $_->nop foreach @_; }
  sub STORE { 
    my $obj = shift; 
    $#$obj = 1; 
    @$obj->[0,1] = ('=', shift);
  }
}

{
  my $foo = new symbolic 11;
  my $baz = $foo++;
  test( (sprintf "%d", $foo), '12');
  test( (sprintf "%d", $baz), '11');
  my $bar = $foo;
  $baz = ++$foo;
  test( (sprintf "%d", $foo), '13');
  test( (sprintf "%d", $bar), '12');
  test( (sprintf "%d", $baz), '13');
  my $ban = $foo;
  $baz = ($foo += 1);
  test( (sprintf "%d", $foo), '14');
  test( (sprintf "%d", $bar), '12');
  test( (sprintf "%d", $baz), '14');
  test( (sprintf "%d", $ban), '13');
  $baz = 0;
  $baz = $foo++;
  test( (sprintf "%d", $foo), '15');
  test( (sprintf "%d", $baz), '14');
  test( "$foo", '[++ [+= [++ [++ [n 11] 1] 1] 1] 1]');
}

{
  my $iter = new symbolic 2;
  my $side = new symbolic 1;
  my $cnt = $iter;
  
  while ($cnt) {
    $cnt = $cnt - 1;		# The "simple" way
    $side = (sqrt(1 + $side**2) - 1)/$side;
  }
  my $pi = $side*(2**($iter+2));
  test "$side", '[/ [- [sqrt [+ 1 [** [/ [- [sqrt [+ 1 [** [n 1] 2]]] 1] [n 1]] 2]]] 1] [/ [- [sqrt [+ 1 [** [n 1] 2]]] 1] [n 1]]]';
  test( (sprintf "%f", $pi), '3.182598');
}

{
  my $iter = new symbolic 2;
  my $side = new symbolic 1;
  my $cnt = $iter;
  
  while ($cnt--) {
    $side = (sqrt(1 + $side**2) - 1)/$side;
  }
  my $pi = $side*(2**($iter+2));
  test "$side", '[/ [- [sqrt [+ 1 [** [/ [- [sqrt [+ 1 [** [n 1] 2]]] 1] [n 1]] 2]]] 1] [/ [- [sqrt [+ 1 [** [n 1] 2]]] 1] [n 1]]]';
  test( (sprintf "%f", $pi), '3.182598');
}

{
  my ($a, $b);
  symbolic->vars($a, $b);
  my $c = sqrt($a**2 + $b**2);
  $a = 3; $b = 4;
  test( (sprintf "%d", $c), '5');
  $a = 12; $b = 5;
  test( (sprintf "%d", $c), '13');
}

{
  package symbolic1;		# Primitive symbolic calculator
  # Mutator inc/dec
  use overload nomethod => \&wrap, '""' => \&str, '0+' => \&num, '=' => \&cpy;

  sub new { shift; bless ['n', @_] }
  sub cpy {
    my $self = shift;
    bless [@$self], ref $self;
  }
  sub wrap {
    my ($obj, $other, $inv, $meth) = @_;
    if ($meth eq '++' or $meth eq '--') {
      @$obj = ($meth, (bless [@$obj]), 1); # Avoid circular reference
      return $obj;
    }
    ($obj, $other) = ($other, $obj) if $inv;
    bless [$meth, $obj, $other];
  }
  sub str {
    my ($meth, $a, $b) = @{+shift};
    $a = 'u' unless defined $a;
    if (defined $b) {
      "[$meth $a $b]";
    } else {
      "[$meth $a]";
    }
  } 
  my %subr = ( 'n' => sub {$_[0]} );
  foreach my $op (split " ", $overload::ops{with_assign}) {
    $subr{$op} = $subr{"$op="} = eval "sub {shift() $op shift()}";
  }
  my @bins = qw(binary 3way_comparison num_comparison str_comparison);
  foreach my $op (split " ", "@overload::ops{ @bins }") {
    $subr{$op} = eval "sub {shift() $op shift()}";
  }
  foreach my $op (split " ", "@overload::ops{qw(unary func)}") {
    $subr{$op} = eval "sub {$op shift()}";
  }
  $subr{'++'} = $subr{'+'};
  $subr{'--'} = $subr{'-'};
  
  sub num {
    my ($meth, $a, $b) = @{+shift};
    my $subr = $subr{$meth} 
      or die "Do not know how to ($meth) in symbolic";
    $a = $a->num if ref $a eq __PACKAGE__;
    $b = $b->num if ref $b eq __PACKAGE__;
    $subr->($a,$b);
  }
  sub TIESCALAR { my $pack = shift; $pack->new(@_) }
  sub FETCH { shift }
  sub nop {  }		# Around a bug
  sub vars { my $p = shift; tie($_, $p), $_->nop foreach @_; }
  sub STORE { 
    my $obj = shift; 
    $#$obj = 1; 
    @$obj->[0,1] = ('=', shift);
  }
}

{
  my $foo = new symbolic1 11;
  my $baz = $foo++;
  test( (sprintf "%d", $foo), '12');
  test( (sprintf "%d", $baz), '11');
  my $bar = $foo;
  $baz = ++$foo;
  test( (sprintf "%d", $foo), '13');
  test( (sprintf "%d", $bar), '12');
  test( (sprintf "%d", $baz), '13');
  my $ban = $foo;
  $baz = ($foo += 1);
  test( (sprintf "%d", $foo), '14');
  test( (sprintf "%d", $bar), '12');
  test( (sprintf "%d", $baz), '14');
  test( (sprintf "%d", $ban), '13');
  $baz = 0;
  $baz = $foo++;
  test( (sprintf "%d", $foo), '15');
  test( (sprintf "%d", $baz), '14');
  test( "$foo", '[++ [+= [++ [++ [n 11] 1] 1] 1] 1]');
}

{
  my $iter = new symbolic1 2;
  my $side = new symbolic1 1;
  my $cnt = $iter;
  
  while ($cnt) {
    $cnt = $cnt - 1;		# The "simple" way
    $side = (sqrt(1 + $side**2) - 1)/$side;
  }
  my $pi = $side*(2**($iter+2));
  test "$side", '[/ [- [sqrt [+ 1 [** [/ [- [sqrt [+ 1 [** [n 1] 2]]] 1] [n 1]] 2]]] 1] [/ [- [sqrt [+ 1 [** [n 1] 2]]] 1] [n 1]]]';
  test( (sprintf "%f", $pi), '3.182598');
}

{
  my $iter = new symbolic1 2;
  my $side = new symbolic1 1;
  my $cnt = $iter;
  
  while ($cnt--) {
    $side = (sqrt(1 + $side**2) - 1)/$side;
  }
  my $pi = $side*(2**($iter+2));
  test "$side", '[/ [- [sqrt [+ 1 [** [/ [- [sqrt [+ 1 [** [n 1] 2]]] 1] [n 1]] 2]]] 1] [/ [- [sqrt [+ 1 [** [n 1] 2]]] 1] [n 1]]]';
  test( (sprintf "%f", $pi), '3.182598');
}

{
  my ($a, $b);
  symbolic1->vars($a, $b);
  my $c = sqrt($a**2 + $b**2);
  $a = 3; $b = 4;
  test( (sprintf "%d", $c), '5');
  $a = 12; $b = 5;
  test( (sprintf "%d", $c), '13');
}

{
  package two_face;		# Scalars with separate string and
                                # numeric values.
  sub new { my $p = shift; bless [@_], $p }
  use overload '""' => \&str, '0+' => \&num, fallback => 1;
  sub num {shift->[1]}
  sub str {shift->[0]}
}

{
  my $seven = new two_face ("vii", 7);
  test( (sprintf "seven=$seven, seven=%d, eight=%d", $seven, $seven+1),
	'seven=vii, seven=7, eight=8');
  test( scalar ($seven =~ /i/), '1')
}

{
  package sorting;
  use overload 'cmp' => \&comp;
  sub new { my ($p, $v) = @_; bless \$v, $p }
  sub comp { my ($x,$y) = @_; ($$x * 3 % 10) <=> ($$y * 3 % 10) or $$x cmp $$y }
}
{
  my @arr = map sorting->new($_), 0..12;
  my @sorted1 = sort @arr;
  my @sorted2 = map $$_, @sorted1;
  test "@sorted2", '0 10 7 4 1 11 8 5 12 2 9 6 3';
}
{
  package iterator;
  use overload '<>' => \&iter;
  sub new { my ($p, $v) = @_; bless \$v, $p }
  sub iter { my ($x) = @_; return undef if $$x < 0; return $$x--; }
}

# XXX iterator overload not intended to work with CORE::GLOBAL?
if (defined &CORE::GLOBAL::glob) {
  test '1', '1';	# 175
  test '1', '1';	# 176
  test '1', '1';	# 177
}
else {
  my $iter = iterator->new(5);
  my $acc = '';
  my $out;
  $acc .= " $out" while $out = <${iter}>;
  test $acc, ' 5 4 3 2 1 0';	# 175
  $iter = iterator->new(5);
  test scalar <${iter}>, '5';	# 176
  $acc = '';
  $acc .= " $out" while $out = <$iter>;
  test $acc, ' 4 3 2 1 0';	# 177
}
{
  package deref;
  use overload '%{}' => \&hderef, '&{}' => \&cderef, 
    '*{}' => \&gderef, '${}' => \&sderef, '@{}' => \&aderef;
  sub new { my ($p, $v) = @_; bless \$v, $p }
  sub deref {
    my ($self, $key) = (shift, shift);
    my $class = ref $self;
    bless $self, 'deref::dummy'; # Disable overloading of %{} 
    my $out = $self->{$key};
    bless $self, $class;	# Restore overloading
    $out;
  }
  sub hderef {shift->deref('h')}
  sub aderef {shift->deref('a')}
  sub cderef {shift->deref('c')}
  sub gderef {shift->deref('g')}
  sub sderef {shift->deref('s')}
}
{
  my $deref = bless { h => { foo => 5 , fake => 23 },
		      c => sub {return shift() + 34},
		      's' => \123,
		      a => [11..13],
		      g => \*srt,
		    }, 'deref';
  # Hash:
  my @cont = sort %$deref;
  if ("\t" eq "\011") { # ascii
      test "@cont", '23 5 fake foo';	# 178
  } 
  else {                # ebcdic alpha-numeric sort order
      test "@cont", 'fake foo 23 5';	# 178
  }
  my @keys = sort keys %$deref;
  test "@keys", 'fake foo';	# 179
  my @val = sort values %$deref;
  test "@val", '23 5';		# 180
  test $deref->{foo}, 5;	# 181
  test defined $deref->{bar}, ''; # 182
  my $key;
  @keys = ();
  push @keys, $key while $key = each %$deref;
  @keys = sort @keys;
  test "@keys", 'fake foo';	# 183  
  test exists $deref->{bar}, ''; # 184
  test exists $deref->{foo}, 1; # 185
  # Code:
  test $deref->(5), 39;		# 186
  test &$deref(6), 40;		# 187
  sub xxx_goto { goto &$deref }
  test xxx_goto(7), 41;		# 188
  my $srt = bless { c => sub {$b <=> $a}
		  }, 'deref';
  *srt = \&$srt;
  my @sorted = sort srt 11, 2, 5, 1, 22;
  test "@sorted", '22 11 5 2 1'; # 189
  # Scalar
  test $$deref, 123;		# 190
  # Code
  @sorted = sort $srt 11, 2, 5, 1, 22;
  test "@sorted", '22 11 5 2 1'; # 191
  # Array
  test "@$deref", '11 12 13';	# 192
  test $#$deref, '2';		# 193
  my $l = @$deref;
  test $l, 3;			# 194
  test $deref->[2], '13';		# 195
  $l = pop @$deref;
  test $l, 13;			# 196
  $l = 1;
  test $deref->[$l], '12';	# 197
  # Repeated dereference
  my $double = bless { h => $deref,
		     }, 'deref';
  test $double->{foo}, 5;	# 198
}

{
  package two_refs;
  use overload '%{}' => \&gethash, '@{}' => sub { ${shift()} };
  sub new { 
    my $p = shift; 
    bless \ [@_], $p;
  }
  sub gethash {
    my %h;
    my $self = shift;
    tie %h, ref $self, $self;
    \%h;
  }

  sub TIEHASH { my $p = shift; bless \ shift, $p }
  my %fields;
  my $i = 0;
  $fields{$_} = $i++ foreach qw{zero one two three};
  sub STORE { 
    my $self = ${shift()};
    my $key = $fields{shift()};
    defined $key or die "Out of band access";
    $$self->[$key] = shift;
  }
  sub FETCH { 
    my $self = ${shift()};
    my $key = $fields{shift()};
    defined $key or die "Out of band access";
    $$self->[$key];
  }
}

my $bar = new two_refs 3,4,5,6;
$bar->[2] = 11;
test $bar->{two}, 11;		# 199
$bar->{three} = 13;
test $bar->[3], 13;		# 200

{
  package two_refs_o;
  @ISA = ('two_refs');
}

$bar = new two_refs_o 3,4,5,6;
$bar->[2] = 11;
test $bar->{two}, 11;		# 201
$bar->{three} = 13;
test $bar->[3], 13;		# 202

{
  package two_refs1;
  use overload '%{}' => sub { ${shift()}->[1] },
               '@{}' => sub { ${shift()}->[0] };
  sub new { 
    my $p = shift; 
    my $a = [@_];
    my %h;
    tie %h, $p, $a;
    bless \ [$a, \%h], $p;
  }
  sub gethash {
    my %h;
    my $self = shift;
    tie %h, ref $self, $self;
    \%h;
  }

  sub TIEHASH { my $p = shift; bless \ shift, $p }
  my %fields;
  my $i = 0;
  $fields{$_} = $i++ foreach qw{zero one two three};
  sub STORE { 
    my $a = ${shift()};
    my $key = $fields{shift()};
    defined $key or die "Out of band access";
    $a->[$key] = shift;
  }
  sub FETCH { 
    my $a = ${shift()};
    my $key = $fields{shift()};
    defined $key or die "Out of band access";
    $a->[$key];
  }
}

$bar = new two_refs_o 3,4,5,6;
$bar->[2] = 11;
test $bar->{two}, 11;		# 203
$bar->{three} = 13;
test $bar->[3], 13;		# 204

{
  package two_refs1_o;
  @ISA = ('two_refs1');
}

$bar = new two_refs1_o 3,4,5,6;
$bar->[2] = 11;
test $bar->{two}, 11;		# 205
$bar->{three} = 13;
test $bar->[3], 13;		# 206

{
  package B;
  use overload bool => sub { ${+shift} };
}

my $aaa;
{ my $bbbb = 0; $aaa = bless \$bbbb, B }

test !$aaa, 1;			# 207

unless ($aaa) {
  test 'ok', 'ok';		# 208
} else {
  test 'is not', 'ok';		# 208
}

# check that overload isn't done twice by join
{ my $c = 0;
  package Join;
  use overload '""' => sub { $c++ };
  my $x = join '', bless([]), 'pq', bless([]);
  main::test $x, '0pq1';		# 209
};

# Test module-specific warning
{
    # check the Odd number of arguments for overload::constant warning
    my $a = "" ;
    local $SIG{__WARN__} = sub {$a = $_[0]} ;
    $x = eval ' overload::constant "integer" ; ' ;
    test($a eq "") ; # 210
    use warnings 'overload' ;
    $x = eval ' overload::constant "integer" ; ' ;
    test($a =~ /^Odd number of arguments for overload::constant at/) ; # 211
}

{
    # check the `$_[0]' is not an overloadable type warning
    my $a = "" ;
    local $SIG{__WARN__} = sub {$a = $_[0]} ;
    $x = eval ' overload::constant "fred" => sub {} ; ' ;
    test($a eq "") ; # 212
    use warnings 'overload' ;
    $x = eval ' overload::constant "fred" => sub {} ; ' ;
    test($a =~ /^`fred' is not an overloadable type at/); # 213
}

{
    # check the `$_[1]' is not a code reference warning
    my $a = "" ;
    local $SIG{__WARN__} = sub {$a = $_[0]} ;
    $x = eval ' overload::constant "integer" => 1; ' ;
    test($a eq "") ; # 214
    use warnings 'overload' ;
    $x = eval ' overload::constant "integer" => 1; ' ;
    test($a =~ /^`1' is not a code reference at/); # 215
}

# make sure that we don't inifinitely recurse
{
  my $c = 0;
  package Recurse;
  use overload '""'    => sub { shift },
               '0+'    => sub { shift },
               'bool'  => sub { shift },
               fallback => 1;
  my $x = bless([]);
  main::test("$x" =~ /Recurse=ARRAY/);		# 216
  main::test($x);                               # 217
  main::test($x+0 =~ /Recurse=ARRAY/);		# 218
};

# Last test is:
sub last {218}
