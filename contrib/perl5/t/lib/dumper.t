#!./perl -w
#
# testsuite for Data::Dumper
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bData\/Dumper\b/) {
      print "1..0 # Skip: Data::Dumper was not built\n";
      exit 0;
    }
}

use Data::Dumper;
use Config;
my $Is_ebcdic = defined($Config{'ebcdic'}) && $Config{'ebcdic'} eq 'define';

$Data::Dumper::Pad = "#";
my $TMAX;
my $XS;
my $TNUM = 0;
my $WANT = '';

sub TEST {
  my $string = shift;
  my $t = eval $string;
  ++$TNUM;
  $t =~ s/([A-Z]+)\(0x[0-9a-f]+\)/$1(0xdeadbeef)/g
      if ($WANT =~ /deadbeef/);
  if ($Is_ebcdic) {
      # these data need massaging with non ascii character sets
      # because of hashing order differences
      $WANT = join("\n",sort(split(/\n/,$WANT)));
      $WANT =~ s/\,$//mg;
      $t    = join("\n",sort(split(/\n/,$t)));
      $t    =~ s/\,$//mg;
  }
  print( ($t eq $WANT and not $@) ? "ok $TNUM\n"
	: "not ok $TNUM\n--Expected--\n$WANT\n--Got--\n$@$t\n");

  ++$TNUM;
  eval "$t";
  print $@ ? "not ok $TNUM\n# \$@ says: $@\n" : "ok $TNUM\n";

  $t = eval $string;
  ++$TNUM;
  $t =~ s/([A-Z]+)\(0x[0-9a-f]+\)/$1(0xdeadbeef)/g
      if ($WANT =~ /deadbeef/);
  if ($Is_ebcdic) {
      # here too there are hashing order differences
      $WANT = join("\n",sort(split(/\n/,$WANT)));
      $WANT =~ s/\,$//mg;
      $t    = join("\n",sort(split(/\n/,$t)));
      $t    =~ s/\,$//mg;
  }
  print( ($t eq $WANT and not $@) ? "ok $TNUM\n"
	: "not ok $TNUM\n--Expected--\n$WANT\n--Got--\n$@$t\n");
}

if (defined &Data::Dumper::Dumpxs) {
  print "### XS extension loaded, will run XS tests\n";
  $TMAX = 186; $XS = 1;
}
else {
  print "### XS extensions not loaded, will NOT run XS tests\n";
  $TMAX = 93; $XS = 0;
}

print "1..$TMAX\n";

#############
#############

@c = ('c');
$c = \@c;
$b = {};
$a = [1, $b, $c];
$b->{a} = $a;
$b->{b} = $a->[1];
$b->{c} = $a->[2];

############# 1
##
$WANT = <<'EOT';
#$a = [
#       1,
#       {
#         'a' => $a,
#         'b' => $a->[1],
#         'c' => [
#                  'c'
#                ]
#       },
#       $a->[1]{'c'}
#     ];
#$b = $a->[1];
#$c = $a->[1]{'c'};
EOT

TEST q(Data::Dumper->Dump([$a,$b,$c], [qw(a b c)]));
TEST q(Data::Dumper->Dumpxs([$a,$b,$c], [qw(a b c)])) if $XS;


############# 7
##
$WANT = <<'EOT';
#@a = (
#       1,
#       {
#         'a' => [],
#         'b' => {},
#         'c' => [
#                  'c'
#                ]
#       },
#       []
#     );
#$a[1]{'a'} = \@a;
#$a[1]{'b'} = $a[1];
#$a[2] = $a[1]{'c'};
#$b = $a[1];
EOT

$Data::Dumper::Purity = 1;         # fill in the holes for eval
TEST q(Data::Dumper->Dump([$a, $b], [qw(*a b)])); # print as @a
TEST q(Data::Dumper->Dumpxs([$a, $b], [qw(*a b)])) if $XS;

############# 13
##
$WANT = <<'EOT';
#%b = (
#       'a' => [
#                1,
#                {},
#                [
#                  'c'
#                ]
#              ],
#       'b' => {},
#       'c' => []
#     );
#$b{'a'}[1] = \%b;
#$b{'b'} = \%b;
#$b{'c'} = $b{'a'}[2];
#$a = $b{'a'};
EOT

TEST q(Data::Dumper->Dump([$b, $a], [qw(*b a)])); # print as %b
TEST q(Data::Dumper->Dumpxs([$b, $a], [qw(*b a)])) if $XS;

############# 19
##
$WANT = <<'EOT';
#$a = [
#  1,
#  {
#    'a' => [],
#    'b' => {},
#    'c' => []
#  },
#  []
#];
#$a->[1]{'a'} = $a;
#$a->[1]{'b'} = $a->[1];
#$a->[1]{'c'} = \@c;
#$a->[2] = \@c;
#$b = $a->[1];
EOT

$Data::Dumper::Indent = 1;
TEST q(
       $d = Data::Dumper->new([$a,$b], [qw(a b)]);
       $d->Seen({'*c' => $c});
       $d->Dump;
      );
if ($XS) {
  TEST q(
	 $d = Data::Dumper->new([$a,$b], [qw(a b)]);
	 $d->Seen({'*c' => $c});
	 $d->Dumpxs;
	);
}


############# 25
##
$WANT = <<'EOT';
#$a = [
#       #0
#       1,
#       #1
#       {
#         a => $a,
#         b => $a->[1],
#         c => [
#                #0
#                'c'
#              ]
#       },
#       #2
#       $a->[1]{c}
#     ];
#$b = $a->[1];
EOT

$d->Indent(3);
$d->Purity(0)->Quotekeys(0);
TEST q( $d->Reset; $d->Dump );

TEST q( $d->Reset; $d->Dumpxs ) if $XS;

############# 31
##
$WANT = <<'EOT';
#$VAR1 = [
#  1,
#  {
#    'a' => [],
#    'b' => {},
#    'c' => [
#      'c'
#    ]
#  },
#  []
#];
#$VAR1->[1]{'a'} = $VAR1;
#$VAR1->[1]{'b'} = $VAR1->[1];
#$VAR1->[2] = $VAR1->[1]{'c'};
EOT

TEST q(Dumper($a));
TEST q(Data::Dumper::DumperX($a)) if $XS;

############# 37
##
$WANT = <<'EOT';
#[
#  1,
#  {
#    a => $VAR1,
#    b => $VAR1->[1],
#    c => [
#      'c'
#    ]
#  },
#  $VAR1->[1]{c}
#]
EOT

{
  local $Data::Dumper::Purity = 0;
  local $Data::Dumper::Quotekeys = 0;
  local $Data::Dumper::Terse = 1;
  TEST q(Dumper($a));
  TEST q(Data::Dumper::DumperX($a)) if $XS;
}


############# 43
##
$WANT = <<'EOT';
#$VAR1 = {
#  "abc\0'\efg" => "mno\0",
#  "reftest" => \\1
#};
EOT

$foo = { "abc\000\'\efg" => "mno\000",
         "reftest" => \\1,
       };
{
  local $Data::Dumper::Useqq = 1;
  TEST q(Dumper($foo));
}

  $WANT = <<"EOT";
#\$VAR1 = {
#  'abc\0\\'\efg' => 'mno\0',
#  'reftest' => \\\\1
#};
EOT

  {
    local $Data::Dumper::Useqq = 1;
    TEST q(Data::Dumper::DumperX($foo)) if $XS;   # cheat
  }



#############
#############

{
  package main;
  use Data::Dumper;
  $foo = 5;
  @foo = (-10,\*foo);
  %foo = (a=>1,b=>\$foo,c=>\@foo);
  $foo{d} = \%foo;
  $foo[2] = \%foo;

############# 49
##
  $WANT = <<'EOT';
#$foo = \*::foo;
#*::foo = \5;
#*::foo = [
#           #0
#           -10,
#           #1
#           do{my $o},
#           #2
#           {
#             'a' => 1,
#             'b' => do{my $o},
#             'c' => [],
#             'd' => {}
#           }
#         ];
#*::foo{ARRAY}->[1] = $foo;
#*::foo{ARRAY}->[2]{'b'} = *::foo{SCALAR};
#*::foo{ARRAY}->[2]{'c'} = *::foo{ARRAY};
#*::foo{ARRAY}->[2]{'d'} = *::foo{ARRAY}->[2];
#*::foo = *::foo{ARRAY}->[2];
#@bar = @{*::foo{ARRAY}};
#%baz = %{*::foo{ARRAY}->[2]};
EOT

  $Data::Dumper::Purity = 1;
  $Data::Dumper::Indent = 3;
  TEST q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz']));
  TEST q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz'])) if $XS;

############# 55
##
  $WANT = <<'EOT';
#$foo = \*::foo;
#*::foo = \5;
#*::foo = [
#  -10,
#  do{my $o},
#  {
#    'a' => 1,
#    'b' => do{my $o},
#    'c' => [],
#    'd' => {}
#  }
#];
#*::foo{ARRAY}->[1] = $foo;
#*::foo{ARRAY}->[2]{'b'} = *::foo{SCALAR};
#*::foo{ARRAY}->[2]{'c'} = *::foo{ARRAY};
#*::foo{ARRAY}->[2]{'d'} = *::foo{ARRAY}->[2];
#*::foo = *::foo{ARRAY}->[2];
#$bar = *::foo{ARRAY};
#$baz = *::foo{ARRAY}->[2];
EOT

  $Data::Dumper::Indent = 1;
  TEST q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz']));
  TEST q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz'])) if $XS;

############# 61
##
  $WANT = <<'EOT';
#@bar = (
#  -10,
#  \*::foo,
#  {}
#);
#*::foo = \5;
#*::foo = \@bar;
#*::foo = {
#  'a' => 1,
#  'b' => do{my $o},
#  'c' => [],
#  'd' => {}
#};
#*::foo{HASH}->{'b'} = *::foo{SCALAR};
#*::foo{HASH}->{'c'} = \@bar;
#*::foo{HASH}->{'d'} = *::foo{HASH};
#$bar[2] = *::foo{HASH};
#%baz = %{*::foo{HASH}};
#$foo = $bar[1];
EOT

  TEST q(Data::Dumper->Dump([\\@foo, \\%foo, \\*foo], ['*bar', '*baz', '*foo']));
  TEST q(Data::Dumper->Dumpxs([\\@foo, \\%foo, \\*foo], ['*bar', '*baz', '*foo'])) if $XS;

############# 67
##
  $WANT = <<'EOT';
#$bar = [
#  -10,
#  \*::foo,
#  {}
#];
#*::foo = \5;
#*::foo = $bar;
#*::foo = {
#  'a' => 1,
#  'b' => do{my $o},
#  'c' => [],
#  'd' => {}
#};
#*::foo{HASH}->{'b'} = *::foo{SCALAR};
#*::foo{HASH}->{'c'} = $bar;
#*::foo{HASH}->{'d'} = *::foo{HASH};
#$bar->[2] = *::foo{HASH};
#$baz = *::foo{HASH};
#$foo = $bar->[1];
EOT

  TEST q(Data::Dumper->Dump([\\@foo, \\%foo, \\*foo], ['bar', 'baz', 'foo']));
  TEST q(Data::Dumper->Dumpxs([\\@foo, \\%foo, \\*foo], ['bar', 'baz', 'foo'])) if $XS;

############# 73
##
  $WANT = <<'EOT';
#$foo = \*::foo;
#@bar = (
#  -10,
#  $foo,
#  {
#    a => 1,
#    b => \5,
#    c => \@bar,
#    d => $bar[2]
#  }
#);
#%baz = %{$bar[2]};
EOT

  $Data::Dumper::Purity = 0;
  $Data::Dumper::Quotekeys = 0;
  TEST q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz']));
  TEST q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz'])) if $XS;

############# 79
##
  $WANT = <<'EOT';
#$foo = \*::foo;
#$bar = [
#  -10,
#  $foo,
#  {
#    a => 1,
#    b => \5,
#    c => $bar,
#    d => $bar->[2]
#  }
#];
#$baz = $bar->[2];
EOT

  TEST q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz']));
  TEST q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz'])) if $XS;

}

#############
#############
{
  package main;
  @dogs = ( 'Fido', 'Wags' );
  %kennel = (
            First => \$dogs[0],
            Second =>  \$dogs[1],
           );
  $dogs[2] = \%kennel;
  $mutts = \%kennel;
  $mutts = $mutts;         # avoid warning
  
############# 85
##
  $WANT = <<'EOT';
#%kennels = (
#  First => \'Fido',
#  Second => \'Wags'
#);
#@dogs = (
#  ${$kennels{First}},
#  ${$kennels{Second}},
#  \%kennels
#);
#%mutts = %kennels;
EOT

  TEST q(
	 $d = Data::Dumper->new([\\%kennel, \\@dogs, $mutts],
				[qw(*kennels *dogs *mutts)] );
	 $d->Dump;
	);
  if ($XS) {
    TEST q(
	   $d = Data::Dumper->new([\\%kennel, \\@dogs, $mutts],
				  [qw(*kennels *dogs *mutts)] );
	   $d->Dumpxs;
	  );
  }
  
############# 91
##
  $WANT = <<'EOT';
#%kennels = %kennels;
#@dogs = @dogs;
#%mutts = %kennels;
EOT

  TEST q($d->Dump);
  TEST q($d->Dumpxs) if $XS;
  
############# 97
##
  $WANT = <<'EOT';
#%kennels = (
#  First => \'Fido',
#  Second => \'Wags'
#);
#@dogs = (
#  ${$kennels{First}},
#  ${$kennels{Second}},
#  \%kennels
#);
#%mutts = %kennels;
EOT

  
  TEST q($d->Reset; $d->Dump);
  if ($XS) {
    TEST q($d->Reset; $d->Dumpxs);
  }

############# 103
##
  $WANT = <<'EOT';
#@dogs = (
#  'Fido',
#  'Wags',
#  {
#    First => \$dogs[0],
#    Second => \$dogs[1]
#  }
#);
#%kennels = %{$dogs[2]};
#%mutts = %{$dogs[2]};
EOT

  TEST q(
	 $d = Data::Dumper->new([\\@dogs, \\%kennel, $mutts],
				[qw(*dogs *kennels *mutts)] );
	 $d->Dump;
	);
  if ($XS) {
    TEST q(
	   $d = Data::Dumper->new([\\@dogs, \\%kennel, $mutts],
				  [qw(*dogs *kennels *mutts)] );
	   $d->Dumpxs;
	  );
  }
  
############# 109
##
  TEST q($d->Reset->Dump);
  if ($XS) {
    TEST q($d->Reset->Dumpxs);
  }

############# 115
##
  $WANT = <<'EOT';
#@dogs = (
#  'Fido',
#  'Wags',
#  {
#    First => \'Fido',
#    Second => \'Wags'
#  }
#);
#%kennels = (
#  First => \'Fido',
#  Second => \'Wags'
#);
EOT

  TEST q(
	 $d = Data::Dumper->new( [\@dogs, \%kennel], [qw(*dogs *kennels)] );
	 $d->Deepcopy(1)->Dump;
	);
  if ($XS) {
    TEST q($d->Reset->Dumpxs);
  }
  
}

{

sub z { print "foo\n" }
$c = [ \&z ];

############# 121
##
  $WANT = <<'EOT';
#$a = $b;
#$c = [
#  $b
#];
EOT

TEST q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'b' => \&z})->Dump;);
TEST q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'b' => \&z})->Dumpxs;)
	if $XS;

############# 127
##
  $WANT = <<'EOT';
#$a = \&b;
#$c = [
#  \&b
#];
EOT

TEST q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'*b' => \&z})->Dump;);
TEST q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'*b' => \&z})->Dumpxs;)
	if $XS;

############# 133
##
  $WANT = <<'EOT';
#*a = \&b;
#@c = (
#  \&b
#);
EOT

TEST q(Data::Dumper->new([\&z,$c],['*a','*c'])->Seen({'*b' => \&z})->Dump;);
TEST q(Data::Dumper->new([\&z,$c],['*a','*c'])->Seen({'*b' => \&z})->Dumpxs;)
	if $XS;

}

{
  $a = [];
  $a->[1] = \$a->[0];

############# 139
##
  $WANT = <<'EOT';
#@a = (
#  undef,
#  do{my $o}
#);
#$a[1] = \$a[0];
EOT

TEST q(Data::Dumper->new([$a],['*a'])->Purity(1)->Dump;);
TEST q(Data::Dumper->new([$a],['*a'])->Purity(1)->Dumpxs;)
	if $XS;
}

{
  $a = \\\\\'foo';
  $b = $$$a;

############# 145
##
  $WANT = <<'EOT';
#$a = \\\\\'foo';
#$b = ${${$a}};
EOT

TEST q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dump;);
TEST q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dumpxs;)
	if $XS;
}

{
  $a = [{ a => \$b }, { b => undef }];
  $b = [{ c => \$b }, { d => \$a }];

############# 151
##
  $WANT = <<'EOT';
#$a = [
#  {
#    a => \[
#        {
#          c => do{my $o}
#        },
#        {
#          d => \[]
#        }
#      ]
#  },
#  {
#    b => undef
#  }
#];
#${$a->[0]{a}}->[0]->{c} = $a->[0]{a};
#${${$a->[0]{a}}->[1]->{d}} = $a;
#$b = ${$a->[0]{a}};
EOT

TEST q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dump;);
TEST q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dumpxs;)
	if $XS;
}

{
  $a = [[[[\\\\\'foo']]]];
  $b = $a->[0][0];
  $c = $${$b->[0][0]};

############# 157
##
  $WANT = <<'EOT';
#$a = [
#  [
#    [
#      [
#        \\\\\'foo'
#      ]
#    ]
#  ]
#];
#$b = $a->[0][0];
#$c = ${${$a->[0][0][0][0]}};
EOT

TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Purity(1)->Dump;);
TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Purity(1)->Dumpxs;)
	if $XS;
}

{
    $f = "pearl";
    $e = [        $f ];
    $d = { 'e' => $e };
    $c = [        $d ];
    $b = { 'c' => $c };
    $a = { 'b' => $b };

############# 163
##
  $WANT = <<'EOT';
#$a = {
#  b => {
#    c => [
#      {
#        e => 'ARRAY(0xdeadbeef)'
#      }
#    ]
#  }
#};
#$b = $a->{b};
#$c = $a->{b}{c};
EOT

TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(4)->Dump;);
TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(4)->Dumpxs;)
	if $XS;

############# 169
##
  $WANT = <<'EOT';
#$a = {
#  b => 'HASH(0xdeadbeef)'
#};
#$b = $a->{b};
#$c = [
#  'HASH(0xdeadbeef)'
#];
EOT

TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(1)->Dump;);
TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(1)->Dumpxs;)
	if $XS;
}

{
    $a = \$a;
    $b = [$a];

############# 175
##
  $WANT = <<'EOT';
#$b = [
#  \$b->[0]
#];
EOT

TEST q(Data::Dumper->new([$b],['b'])->Purity(0)->Dump;);
TEST q(Data::Dumper->new([$b],['b'])->Purity(0)->Dumpxs;)
	if $XS;

############# 181
##
  $WANT = <<'EOT';
#$b = [
#  \do{my $o}
#];
#${$b->[0]} = $b->[0];
EOT


TEST q(Data::Dumper->new([$b],['b'])->Purity(1)->Dump;);
TEST q(Data::Dumper->new([$b],['b'])->Purity(1)->Dumpxs;)
	if $XS;
}
