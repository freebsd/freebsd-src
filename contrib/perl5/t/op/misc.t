#!./perl

# NOTE: Please don't add tests to this file unless they *need* to be run in
# separate executable and can't simply use eval.

chdir 't' if -d 't';
@INC = "../lib";
$ENV{PERL5LIB} = "../lib";

$|=1;

undef $/;
@prgs = split "\n########\n", <DATA>;
print "1..", scalar @prgs, "\n";

$tmpfile = "misctmp000";
1 while -f ++$tmpfile;
END { unlink $tmpfile if $tmpfile; }

$CAT = (($^O eq 'MSWin32') ? '.\perl -e "print <>"' : 'cat');

for (@prgs){
    my $switch;
    if (s/^\s*(-\w.*)//){
	$switch = $1;
    }
    my($prog,$expected) = split(/\nEXPECT\n/, $_);
    if ($^O eq 'MSWin32') {
      open TEST, "| .\\perl -I../lib $switch >$tmpfile 2>&1";
    }
    else {
      open TEST, "| sh -c './perl $switch' >$tmpfile 2>&1";
    }
    print TEST $prog, "\n";
    close TEST;
    $status = $?;
    $results = `$CAT $tmpfile`;
    $results =~ s/\n+$//;
# bison says 'parse error' instead of 'syntax error',
# various yaccs may or may not capitalize 'syntax'.
    $results =~ s/^(syntax|parse) error/syntax error/mig;
    $expected =~ s/\n+$//;
    if ( $results ne $expected){
	print STDERR "PROG: $switch\n$prog\n";
	print STDERR "EXPECTED:\n$expected\n";
	print STDERR "GOT:\n$results\n";
	print "not ";
    }
    print "ok ", ++$i, "\n";
}

__END__
()=()
########
$a = ":="; split /($a)/o, "a:=b:=c"; print "@_"
EXPECT
a := b := c
########
$cusp = ~0 ^ (~0 >> 1);
$, = " ";
print +($cusp - 1) % 8, $cusp % 8, -$cusp % 8, ($cusp + 1) % 8, "!\n";
EXPECT
7 0 0 1 !
########
$foo=undef; $foo->go;
EXPECT
Can't call method "go" on an undefined value at - line 1.
########
BEGIN
        {
	    "foo";
        }
########
$array[128]=1
########
$x=0x0eabcd; print $x->ref;
EXPECT
Can't call method "ref" without a package or object reference at - line 1.
########
chop ($str .= <STDIN>);
########
close ($banana);
########
$x=2;$y=3;$x<$y ? $x : $y += 23;print $x;
EXPECT
25
########
eval {sub bar {print "In bar";}}
########
system './perl -ne "print if eof" /dev/null'
########
chop($file = <>);
########
package N;
sub new {my ($obj,$n)=@_; bless \$n}  
$aa=new N 1;
$aa=12345;
print $aa;
EXPECT
12345
########
%@x=0;
EXPECT
Can't modify hash deref in repeat at - line 1, near "0;"
Execution of - aborted due to compilation errors.
########
$_="foo";
printf(STDOUT "%s\n", $_);
EXPECT
foo
########
push(@a, 1, 2, 3,)
########
quotemeta ""
########
for ("ABCDE") {
 &sub;
s/./&sub($&)/eg;
print;}
sub sub {local($_) = @_;
$_ x 4;}
EXPECT
Modification of a read-only value attempted at - line 3.
########
package FOO;sub new {bless {FOO => BAR}};
package main;
use strict vars;   
my $self = new FOO;
print $$self{FOO};
EXPECT
BAR
########
$_="foo";
s/.{1}//s;
print;
EXPECT
oo
########
print scalar ("foo","bar")
EXPECT
bar
########
sub by_number { $a <=> $b; };# inline function for sort below
$as_ary{0}="a0";
@ordered_array=sort by_number keys(%as_ary);
########
sub NewShell
{
  local($Host) = @_;
  my($m2) = $#Shells++;
  $Shells[$m2]{HOST} = $Host;
  return $m2;
}
 
sub ShowShell
{
  local($i) = @_;
}
 
&ShowShell(&NewShell(beach,Work,"+0+0"));
&ShowShell(&NewShell(beach,Work,"+0+0"));
&ShowShell(&NewShell(beach,Work,"+0+0"));
########
   {
       package FAKEARRAY;
   
       sub TIEARRAY
       { print "TIEARRAY @_\n"; 
         die "bomb out\n" unless $count ++ ;
         bless ['foo'] 
       }
       sub FETCH { print "fetch @_\n"; $_[0]->[$_[1]] }
       sub STORE { print "store @_\n"; $_[0]->[$_[1]] = $_[2] }
       sub DESTROY { print "DESTROY \n"; undef @{$_[0]}; }
   }
   
eval 'tie @h, FAKEARRAY, fred' ;
tie @h, FAKEARRAY, fred ;
EXPECT
TIEARRAY FAKEARRAY fred
TIEARRAY FAKEARRAY fred
DESTROY 
########
BEGIN { die "phooey\n" }
EXPECT
phooey
BEGIN failed--compilation aborted at - line 1.
########
BEGIN { 1/$zero }
EXPECT
Illegal division by zero at - line 1.
BEGIN failed--compilation aborted at - line 1.
########
BEGIN { undef = 0 }
EXPECT
Modification of a read-only value attempted at - line 1.
BEGIN failed--compilation aborted at - line 1.
########
{
    package foo;
    sub PRINT {
        shift;
        print join(' ', reverse @_)."\n";
    }
    sub PRINTF {
        shift;
	  my $fmt = shift;
        print sprintf($fmt, @_)."\n";
    }
    sub TIEHANDLE {
        bless {}, shift;
    }
    sub READLINE {
	"Out of inspiration";
    }
    sub DESTROY {
	print "and destroyed as well\n";
  }
  sub READ {
      shift;
      print STDOUT "foo->can(READ)(@_)\n";
      return 100; 
  }
  sub GETC {
      shift;
      print STDOUT "Don't GETC, Get Perl\n";
      return "a"; 
  }    
}
{
    local(*FOO);
    tie(*FOO,'foo');
    print FOO "sentence.", "reversed", "a", "is", "This";
    print "-- ", <FOO>, " --\n";
    my($buf,$len,$offset);
    $buf = "string";
    $len = 10; $offset = 1;
    read(FOO, $buf, $len, $offset) == 100 or die "foo->READ failed";
    getc(FOO) eq "a" or die "foo->GETC failed";
    printf "%s is number %d\n", "Perl", 1;
}
EXPECT
This is a reversed sentence.
-- Out of inspiration --
foo->can(READ)(string 10 1)
Don't GETC, Get Perl
Perl is number 1
and destroyed as well
########
my @a; $a[2] = 1; for (@a) { $_ = 2 } print "@a\n"
EXPECT
2 2 2
########
@a = ($a, $b, $c, $d) = (5, 6);
print "ok\n"
  if ($a[0] == 5 and $a[1] == 6 and !defined $a[2] and !defined $a[3]);
EXPECT
ok
########
print "ok\n" if (1E2<<1 == 200 and 3E4<<3 == 240000);
EXPECT
ok
########
print "ok\n" if ("\0" lt "\xFF");
EXPECT
ok
########
open(H,'op/misc.t'); # must be in the 't' directory
stat(H);
print "ok\n" if (-e _ and -f _ and -r _);
EXPECT
ok
########
sub thing { 0 || return qw(now is the time) }
print thing(), "\n";
EXPECT
nowisthetime
########
$ren = 'joy';
$stimpy = 'happy';
{ local $main::{ren} = *stimpy; print $ren, ' ' }
print $ren, "\n";
EXPECT
happy joy
########
$stimpy = 'happy';
{ local $main::{ren} = *stimpy; print ${'ren'}, ' ' }
print +(defined(${'ren'}) ? 'oops' : 'joy'), "\n";
EXPECT
happy joy
########
package p;
sub func { print 'really ' unless wantarray; 'p' }
sub groovy { 'groovy' }
package main;
print p::func()->groovy(), "\n"
EXPECT
really groovy
########
@list = ([ 'one', 1 ], [ 'two', 2 ]);
sub func { $num = shift; (grep $_->[1] == $num, @list)[0] }
print scalar(map &func($_), 1 .. 3), " ",
      scalar(map scalar &func($_), 1 .. 3), "\n";
EXPECT
2 3
########
($k, $s)  = qw(x 0);
@{$h{$k}} = qw(1 2 4);
for (@{$h{$k}}) { $s += $_; delete $h{$k} if ($_ == 2) }
print "bogus\n" unless $s == 7;
########
my $a = 'outer';
eval q[ my $a = 'inner'; eval q[ print "$a " ] ];
eval { my $x = 'peace'; eval q[ print "$x\n" ] }
EXPECT
inner peace
########
-w
$| = 1;
sub foo {
    print "In foo1\n";
    eval 'sub foo { print "In foo2\n" }';
    print "Exiting foo1\n";
}
foo;
foo;
EXPECT
In foo1
Subroutine foo redefined at (eval 1) line 1.
Exiting foo1
In foo2
########
$s = 0;
map {#this newline here tickles the bug
$s += $_} (1,2,4);
print "eat flaming death\n" unless ($s == 7);
########
sub foo { local $_ = shift; split; @_ }
@x = foo(' x  y  z ');
print "you die joe!\n" unless "@x" eq 'x y z';
########
/(?{"{"})/	# Check it outside of eval too
EXPECT
Sequence (?{...}) not terminated or not {}-balanced at - line 1, within pattern
/(?{"{"})/: Sequence (?{...}) not terminated or not {}-balanced at - line 1.
########
/(?{"{"}})/	# Check it outside of eval too
EXPECT
Unmatched right bracket at (re_eval 1) line 1, at end of line
syntax error at (re_eval 1) line 1, near ""{"}"
Compilation failed in regexp at - line 1.
########
BEGIN { @ARGV = qw(a b c) }
BEGIN { print "argv <@ARGV>\nbegin <",shift,">\n" }
END { print "end <",shift,">\nargv <@ARGV>\n" }
INIT { print "init <",shift,">\n" }
EXPECT
argv <a b c>
begin <a>
init <b>
end <c>
argv <>
########
-l
# fdopen from a system descriptor to a system descriptor used to close
# the former.
open STDERR, '>&=STDOUT' or die $!;
select STDOUT; $| = 1; print fileno STDOUT;
select STDERR; $| = 1; print fileno STDERR;
EXPECT
1
2
########
-w
sub testme { my $a = "test"; { local $a = "new test"; print $a }}
EXPECT
Can't localize lexical variable $a at - line 2.
########
package X;
sub ascalar { my $r; bless \$r }
sub DESTROY { print "destroyed\n" };
package main;
*s = ascalar X;
EXPECT
destroyed
########
package X;
sub anarray { bless [] }
sub DESTROY { print "destroyed\n" };
package main;
*a = anarray X;
EXPECT
destroyed
########
package X;
sub ahash { bless {} }
sub DESTROY { print "destroyed\n" };
package main;
*h = ahash X;
EXPECT
destroyed
########
package X;
sub aclosure { my $x; bless sub { ++$x } }
sub DESTROY { print "destroyed\n" };
package main;
*c = aclosure X;
EXPECT
destroyed
########
package X;
sub any { bless {} }
my $f = "FH000"; # just to thwart any future optimisations
sub afh { select select ++$f; my $r = *{$f}{IO}; delete $X::{$f}; bless $r }
sub DESTROY { print "destroyed\n" }
package main;
$x = any X; # to bump sv_objcount. IO objs aren't counted??
*f = afh X;
EXPECT
destroyed
destroyed
########
BEGIN {
  $| = 1;
  $SIG{__WARN__} = sub {
    eval { print $_[0] };
    die "bar\n";
  };
  warn "foo\n";
}
EXPECT
foo
bar
BEGIN failed--compilation aborted at - line 8.
########
use strict;
my $foo = "ZZZ\n";
END { print $foo }
EXPECT
ZZZ
########
eval '
use strict;
my $foo = "ZZZ\n";
END { print $foo }
';
EXPECT
ZZZ
