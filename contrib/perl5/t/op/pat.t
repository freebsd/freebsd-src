#!./perl
#
# This is a home for regular expression tests that don't fit into
# the format supported by op/regexp.t.  If you want to add a test
# that does fit that format, add it to op/re_tests, not here.

print "1..231\n";

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}
eval 'use Config';          #  Defaults assumed if this fails

$x = "abc\ndef\n";

if ($x =~ /^abc/) {print "ok 1\n";} else {print "not ok 1\n";}
if ($x !~ /^def/) {print "ok 2\n";} else {print "not ok 2\n";}

$* = 1;
if ($x =~ /^def/) {print "ok 3\n";} else {print "not ok 3\n";}
$* = 0;

$_ = '123';
if (/^([0-9][0-9]*)/) {print "ok 4\n";} else {print "not ok 4\n";}

if ($x =~ /^xxx/) {print "not ok 5\n";} else {print "ok 5\n";}
if ($x !~ /^abc/) {print "not ok 6\n";} else {print "ok 6\n";}

if ($x =~ /def/) {print "ok 7\n";} else {print "not ok 7\n";}
if ($x !~ /def/) {print "not ok 8\n";} else {print "ok 8\n";}

if ($x !~ /.def/) {print "ok 9\n";} else {print "not ok 9\n";}
if ($x =~ /.def/) {print "not ok 10\n";} else {print "ok 10\n";}

if ($x =~ /\ndef/) {print "ok 11\n";} else {print "not ok 11\n";}
if ($x !~ /\ndef/) {print "not ok 12\n";} else {print "ok 12\n";}

$_ = 'aaabbbccc';
if (/(a*b*)(c*)/ && $1 eq 'aaabbb' && $2 eq 'ccc') {
	print "ok 13\n";
} else {
	print "not ok 13\n";
}
if (/(a+b+c+)/ && $1 eq 'aaabbbccc') {
	print "ok 14\n";
} else {
	print "not ok 14\n";
}

if (/a+b?c+/) {print "not ok 15\n";} else {print "ok 15\n";}

$_ = 'aaabccc';
if (/a+b?c+/) {print "ok 16\n";} else {print "not ok 16\n";}
if (/a*b+c*/) {print "ok 17\n";} else {print "not ok 17\n";}

$_ = 'aaaccc';
if (/a*b?c*/) {print "ok 18\n";} else {print "not ok 18\n";}
if (/a*b+c*/) {print "not ok 19\n";} else {print "ok 19\n";}

$_ = 'abcdef';
if (/bcd|xyz/) {print "ok 20\n";} else {print "not ok 20\n";}
if (/xyz|bcd/) {print "ok 21\n";} else {print "not ok 21\n";}

if (m|bc/*d|) {print "ok 22\n";} else {print "not ok 22\n";}

if (/^$_$/) {print "ok 23\n";} else {print "not ok 23\n";}

$* = 1;		# test 3 only tested the optimized version--this one is for real
if ("ab\ncd\n" =~ /^cd/) {print "ok 24\n";} else {print "not ok 24\n";}
$* = 0;

$XXX{123} = 123;
$XXX{234} = 234;
$XXX{345} = 345;

@XXX = ('ok 25','not ok 25', 'ok 26','not ok 26','not ok 27');
while ($_ = shift(@XXX)) {
    ?(.*)? && (print $1,"\n");
    /not/ && reset;
    /not ok 26/ && reset 'X';
}

while (($key,$val) = each(%XXX)) {
    print "not ok 27\n";
    exit;
}

print "ok 27\n";

'cde' =~ /[^ab]*/;
'xyz' =~ //;
if ($& eq 'xyz') {print "ok 28\n";} else {print "not ok 28\n";}

$foo = '[^ab]*';
'cde' =~ /$foo/;
'xyz' =~ //;
if ($& eq 'xyz') {print "ok 29\n";} else {print "not ok 29\n";}

$foo = '[^ab]*';
'cde' =~ /$foo/;
'xyz' =~ /$null/;
if ($& eq 'xyz') {print "ok 30\n";} else {print "not ok 30\n";}

$_ = 'abcdefghi';
/def/;		# optimized up to cmd
if ("$`:$&:$'" eq 'abc:def:ghi') {print "ok 31\n";} else {print "not ok 31\n";}

/cde/ + 0;	# optimized only to spat
if ("$`:$&:$'" eq 'ab:cde:fghi') {print "ok 32\n";} else {print "not ok 32\n";}

/[d][e][f]/;	# not optimized
if ("$`:$&:$'" eq 'abc:def:ghi') {print "ok 33\n";} else {print "not ok 33\n";}

$_ = 'now is the {time for all} good men to come to.';
/ {([^}]*)}/;
if ($1 eq 'time for all') {print "ok 34\n";} else {print "not ok 34 $1\n";}

$_ = 'xxx {3,4}  yyy   zzz';
print /( {3,4})/ ? "ok 35\n" : "not ok 35\n";
print $1 eq '   ' ? "ok 36\n" : "not ok 36\n";
print /( {4,})/ ? "not ok 37\n" : "ok 37\n";
print /( {2,3}.)/ ? "ok 38\n" : "not ok 38\n";
print $1 eq '  y' ? "ok 39\n" : "not ok 39\n";
print /(y{2,3}.)/ ? "ok 40\n" : "not ok 40\n";
print $1 eq 'yyy ' ? "ok 41\n" : "not ok 41\n";
print /x {3,4}/ ? "not ok 42\n" : "ok 42\n";
print /^xxx {3,4}/ ? "not ok 43\n" : "ok 43\n";

$_ = "now is the time for all good men to come to.";
@words = /(\w+)/g;
print join(':',@words) eq "now:is:the:time:for:all:good:men:to:come:to"
    ? "ok 44\n"
    : "not ok 44\n";

@words = ();
while (/\w+/g) {
    push(@words, $&);
}
print join(':',@words) eq "now:is:the:time:for:all:good:men:to:come:to"
    ? "ok 45\n"
    : "not ok 45\n";

@words = ();
pos = 0;
while (/to/g) {
    push(@words, $&);
}
print join(':',@words) eq "to:to"
    ? "ok 46\n"
    : "not ok 46 `@words'\n";

pos $_ = 0;
@words = /to/g;
print join(':',@words) eq "to:to"
    ? "ok 47\n"
    : "not ok 47 `@words'\n";

$_ = "abcdefghi";

$pat1 = 'def';
$pat2 = '^def';
$pat3 = '.def.';
$pat4 = 'abc';
$pat5 = '^abc';
$pat6 = 'abc$';
$pat7 = 'ghi';
$pat8 = '\w*ghi';
$pat9 = 'ghi$';

$t1=$t2=$t3=$t4=$t5=$t6=$t7=$t8=$t9=0;

for $iter (1..5) {
    $t1++ if /$pat1/o;
    $t2++ if /$pat2/o;
    $t3++ if /$pat3/o;
    $t4++ if /$pat4/o;
    $t5++ if /$pat5/o;
    $t6++ if /$pat6/o;
    $t7++ if /$pat7/o;
    $t8++ if /$pat8/o;
    $t9++ if /$pat9/o;
}

$x = "$t1$t2$t3$t4$t5$t6$t7$t8$t9";
print $x eq '505550555' ? "ok 48\n" : "not ok 48 $x\n";

$xyz = 'xyz';
print "abc" =~ /^abc$|$xyz/ ? "ok 49\n" : "not ok 49\n";

# perl 4.009 says "unmatched ()"
eval '"abc" =~ /a(bc$)|$xyz/; $result = "$&:$1"';
print $@ eq "" ? "ok 50\n" : "not ok 50\n";
print $result eq "abc:bc" ? "ok 51\n" : "not ok 51\n";


$_="abcfooabcbar";
$x=/abc/g;
print $` eq "" ? "ok 52\n" : "not ok 52\n" if $x;
$x=/abc/g;
print $` eq "abcfoo" ? "ok 53\n" : "not ok 53\n" if $x;
$x=/abc/g;
print $x == 0 ? "ok 54\n" : "not ok 54\n";
pos = 0;
$x=/ABC/gi;
print $` eq "" ? "ok 55\n" : "not ok 55\n" if $x;
$x=/ABC/gi;
print $` eq "abcfoo" ? "ok 56\n" : "not ok 56\n" if $x;
$x=/ABC/gi;
print $x == 0 ? "ok 57\n" : "not ok 57\n";
pos = 0;
$x=/abc/g;
print $' eq "fooabcbar" ? "ok 58\n" : "not ok 58\n" if $x;
$x=/abc/g;
print $' eq "bar" ? "ok 59\n" : "not ok 59\n" if $x;
$_ .= '';
@x=/abc/g;
print scalar @x == 2 ? "ok 60\n" : "not ok 60\n";

$_ = "abdc";
pos $_ = 2;
/\Gc/gc;
print "not " if (pos $_) != 2;
print "ok 61\n";
/\Gc/g;
print "not " if defined pos $_;
print "ok 62\n";

$out = 1;
'abc' =~ m'a(?{ $out = 2 })b';
print "not " if $out != 2;
print "ok 63\n";

$out = 1;
'abc' =~ m'a(?{ $out = 3 })c';
print "not " if $out != 1;
print "ok 64\n";

$_ = 'foobar1 bar2 foobar3 barfoobar5 foobar6';
@out = /(?<!foo)bar./g;
print "not " if "@out" ne 'bar2 barf';
print "ok 65\n";

# Tests which depend on REG_INFTY
$reg_infty = defined $Config{reg_infty} ? $Config{reg_infty} : 32767;
$reg_infty_m = $reg_infty - 1; $reg_infty_p = $reg_infty + 1;

# As well as failing if the pattern matches do unexpected things, the
# next three tests will fail if you should have picked up a lower-than-
# default value for $reg_infty from Config.pm, but have not.

undef $@;
print "not " if eval q(('aaa' =~ /(a{1,$reg_infty_m})/)[0] ne 'aaa') || $@;
print "ok 66\n";

undef $@;
print "not " if eval q(('a' x $reg_infty_m) !~ /a{$reg_infty_m}/) || $@;
print "ok 67\n";

undef $@;
print "not " if eval q(('a' x ($reg_infty_m - 1)) =~ /a{$reg_infty_m}/) || $@;
print "ok 68\n";

undef $@;
eval "'aaa' =~ /a{1,$reg_infty}/";
print "not " if $@ !~ m%^\QQuantifier in {,} bigger than%;
print "ok 69\n";

eval "'aaa' =~ /a{1,$reg_infty_p}/";
print "not "
	if $@ !~ m%^\QQuantifier in {,} bigger than%;
print "ok 70\n";
undef $@;

# Poke a couple more parse failures

$context = 'x' x 256;
eval qq("${context}y" =~ /(?<=$context)y/);
print "not " if $@ !~ m%^\QLookbehind longer than 255 not%;
print "ok 71\n";

# removed test
print "ok 72\n";

# Long Monsters
$test = 73;
for $l (125, 140, 250, 270, 300000, 30) { # Ordered to free memory
  $a = 'a' x $l;
  print "# length=$l\nnot " unless "ba$a=" =~ /a$a=/;
  print "ok $test\n";
  $test++;
  
  print "not " if "b$a=" =~ /a$a=/;
  print "ok $test\n";
  $test++;
}

# 20000 nodes, each taking 3 words per string, and 1 per branch
$long_constant_len = join '|', 12120 .. 32645;
$long_var_len = join '|', 8120 .. 28645;
%ans = ( 'ax13876y25677lbc' => 1,
	 'ax13876y25677mcb' => 0, # not b.
	 'ax13876y35677nbc' => 0, # Num too big
	 'ax13876y25677y21378obc' => 1,
	 'ax13876y25677y21378zbc' => 0,	# Not followed by [k-o]
	 'ax13876y25677y21378y21378kbc' => 1,
	 'ax13876y25677y21378y21378kcb' => 0, # Not b.
	 'ax13876y25677y21378y21378y21378kbc' => 0, # 5 runs
       );

for ( keys %ans ) {
  print "# const-len `$_' not =>  $ans{$_}\nnot " 
    if $ans{$_} xor /a(?=([yx]($long_constant_len)){2,4}[k-o]).*b./o;
  print "ok $test\n";
  $test++;
  print "# var-len   `$_' not =>  $ans{$_}\nnot " 
    if $ans{$_} xor /a(?=([yx]($long_var_len)){2,4}[k-o]).*b./o;
  print "ok $test\n";
  $test++;
}

$_ = " a (bla()) and x(y b((l)u((e))) and b(l(e)e)e";
$expect = "(bla()) ((l)u((e))) (l(e)e)";

sub matchit { 
  m/
     (
       \( 
       (?{ $c = 1 })		# Initialize
       (?:
	 (?(?{ $c == 0 })       # PREVIOUS iteration was OK, stop the loop
	   (?!
	   )			# Fail: will unwind one iteration back
	 )	    
	 (?:
	   [^()]+		# Match a big chunk
	   (?=
	     [()]
	   )			# Do not try to match subchunks
	 |
	   \( 
	   (?{ ++$c })
	 |
	   \) 
	   (?{ --$c })
	 )
       )+			# This may not match with different subblocks
     )
     (?(?{ $c != 0 })
       (?!
       )			# Fail
     )				# Otherwise the chunk 1 may succeed with $c>0
   /xg;
}

@ans = ();
push @ans, $res while $res = matchit;

print "# ans='@ans'\n# expect='$expect'\nnot " if "@ans" ne "1 1 1";
print "ok $test\n";
$test++;

@ans = matchit;

print "# ans='@ans'\n# expect='$expect'\nnot " if "@ans" ne $expect;
print "ok $test\n";
$test++;

print "not " unless "abc" =~ /^(??{"a"})b/;
print "ok $test\n";
$test++;

my $matched;
$matched = qr/\((?:(?>[^()]+)|(??{$matched}))*\)/;

@ans = @ans1 = ();
push(@ans, $res), push(@ans1, $&) while $res = m/$matched/g;

print "# ans='@ans'\n# expect='$expect'\nnot " if "@ans" ne "1 1 1";
print "ok $test\n";
$test++;

print "# ans1='@ans1'\n# expect='$expect'\nnot " if "@ans1" ne $expect;
print "ok $test\n";
$test++;

@ans = m/$matched/g;

print "# ans='@ans'\n# expect='$expect'\nnot " if "@ans" ne $expect;
print "ok $test\n";
$test++;

@ans = ('a/b' =~ m%(.*/)?(.*)%);	# Stack may be bad
print "not " if "@ans" ne 'a/ b';
print "ok $test\n";
$test++;

$code = '{$blah = 45}';
$blah = 12;
eval { /(?$code)/ };
print "not " unless $@ and $@ =~ /not allowed at runtime/ and $blah == 12;
print "ok $test\n";
$test++;

for $code ('{$blah = 45}','=xx') {
  $blah = 12;
  $res = eval { "xx" =~ /(?$code)/o };
  if ($code eq '=xx') {
    print "#'$@','$res','$blah'\nnot " unless not $@ and $res;
  } else {
    print "#'$@','$res','$blah'\nnot " unless $@ and $@ =~ /not allowed at runtime/ and $blah == 12;    
  }
  print "ok $test\n";
  $test++;
}

$code = '{$blah = 45}';
$blah = 12;
eval "/(?$code)/";			
print "not " if $blah != 45;
print "ok $test\n";
$test++;

$blah = 12;
/(?{$blah = 45})/;			
print "not " if $blah != 45;
print "ok $test\n";
$test++;

$x = 'banana';
$x =~ /.a/g;
print "not " unless pos($x) == 2;
print "ok $test\n";
$test++;

$x =~ /.z/gc;
print "not " unless pos($x) == 2;
print "ok $test\n";
$test++;

sub f {
    my $p = $_[0];
    return $p;
}

$x =~ /.a/g;
print "not " unless f(pos($x)) == 4;
print "ok $test\n";
$test++;

$x = $^R = 67;
'foot' =~ /foo(?{$x = 12; 75})[t]/;
print "not " unless $^R eq '75';
print "ok $test\n";
$test++;

$x = $^R = 67;
'foot' =~ /foo(?{$x = 12; 75})[xy]/;
print "not " unless $^R eq '67' and $x eq '12';
print "ok $test\n";
$test++;

$x = $^R = 67;
'foot' =~ /foo(?{ $^R + 12 })((?{ $x = 12; $^R + 17 })[xy])?/;
print "not " unless $^R eq '79' and $x eq '12';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/i eq '(?i-xsm:\bv$)';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/s eq '(?s-xim:\bv$)';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/m eq '(?m-xis:\bv$)';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/x eq '(?x-ism:\bv$)';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/xism eq '(?msix:\bv$)';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/ eq '(?-xism:\bv$)';
print "ok $test\n";
$test++;

$_ = 'xabcx';
foreach $ans ('', 'c') {
  /(?<=(?=a)..)((?=c)|.)/g;
  print "# \$1  ='$1'\n# \$ans='$ans'\nnot " unless $1 eq $ans;
  print "ok $test\n";
  $test++;
}

$_ = 'a';
foreach $ans ('', 'a', '') {
  /^|a|$/g;
  print "# \$&  ='$&'\n# \$ans='$ans'\nnot " unless $& eq $ans;
  print "ok $test\n";
  $test++;
}

sub prefixify {
  my($v,$a,$b,$res) = @_; 
  $v =~ s/\Q$a\E/$b/; 
  print "not " unless $res eq $v; 
  print "ok $test\n";
  $test++;
}
prefixify('/a/b/lib/arch', "/a/b/lib", 'X/lib', 'X/lib/arch');
prefixify('/a/b/man/arch', "/a/b/man", 'X/man', 'X/man/arch');

$_ = 'var="foo"';
/(\")/;
print "not " unless $1 and /$1/;
print "ok $test\n";
$test++;

$a=qr/(?{++$b})/; 
$b = 7;
/$a$a/; 
print "not " unless $b eq '9'; 
print "ok $test\n";
$test++;

$c="$a"; 
/$a$a/; 
print "not " unless $b eq '11'; 
print "ok $test\n";
$test++;

{
  use re "eval"; 
  /$a$c$a/; 
  print "not " unless $b eq '14'; 
  print "ok $test\n";
  $test++;

  local $lex_a = 2;
  my $lex_a = 43;
  my $lex_b = 17;
  my $lex_c = 27;
  my $lex_res = ($lex_b =~ qr/$lex_b(?{ $lex_c = $lex_a++ })/);
  print "not " unless $lex_res eq '1';
  print "ok $test\n";
  $test++;
  print "not " unless $lex_a eq '44';
  print "ok $test\n";
  $test++;
  print "not " unless $lex_c eq '43';
  print "ok $test\n";
  $test++;


  no re "eval"; 
  $match = eval { /$a$c$a/ };
  print "not " 
    unless $b eq '14' and $@ =~ /Eval-group not allowed/ and not $match;
  print "ok $test\n";
  $test++;
}

{
  local $lex_a = 2;
  my $lex_a = 43;
  my $lex_b = 17;
  my $lex_c = 27;
  my $lex_res = ($lex_b =~ qr/17(?{ $lex_c = $lex_a++ })/);
  print "not " unless $lex_res eq '1';
  print "ok $test\n";
  $test++;
  print "not " unless $lex_a eq '44';
  print "ok $test\n";
  $test++;
  print "not " unless $lex_c eq '43';
  print "ok $test\n";
  $test++;
}

{
  package aa;
  $c = 2;
  $::c = 3;
  '' =~ /(?{ $c = 4 })/;
  print "not " unless $c == 4;
}
print "ok $test\n";
$test++;
print "not " unless $c == 3;
print "ok $test\n";
$test++;  
  
sub must_warn_pat {
    my $warn_pat = shift;
    return sub { print "not " unless $_[0] =~ /$warn_pat/ }
}

sub must_warn {
    my ($warn_pat, $code) = @_;
    local %SIG;
    eval 'BEGIN { use warnings; $SIG{__WARN__} = $warn_pat };' . $code;
    print "ok $test\n";
    $test++;
}


sub make_must_warn {
    my $warn_pat = shift;
    return sub { must_warn(must_warn_pat($warn_pat)) }
}

my $for_future = make_must_warn('reserved for future extensions');

&$for_future('q(a:[b]:) =~ /[x[:foo:]]/');

#&$for_future('q(a=[b]=) =~ /[x[=foo=]]/');
print "ok $test\n"; $test++; # now a fatal croak

#&$for_future('q(a.[b].) =~ /[x[.foo.]]/');
print "ok $test\n"; $test++; # now a fatal croak

# test if failure of patterns returns empty list
$_ = 'aaa';
@_ = /bbb/;
print "not " if @_;
print "ok $test\n";
$test++;

@_ = /bbb/g;
print "not " if @_;
print "ok $test\n";
$test++;

@_ = /(bbb)/;
print "not " if @_;
print "ok $test\n";
$test++;

@_ = /(bbb)/g;
print "not " if @_;
print "ok $test\n";
$test++;

/a(?=.$)/;
print "not " if $#+ != 0 or $#- != 0;
print "ok $test\n";
$test++;

print "not " if $+[0] != 2 or $-[0] != 1;
print "ok $test\n";
$test++;

print "not " 
   if defined $+[1] or defined $-[1] or defined $+[2] or defined $-[2];
print "ok $test\n";
$test++;

/a(a)(a)/;
print "not " if $#+ != 2 or $#- != 2;
print "ok $test\n";
$test++;

print "not " if $+[0] != 3 or $-[0] != 0;
print "ok $test\n";
$test++;

print "not " if $+[1] != 2 or $-[1] != 1;
print "ok $test\n";
$test++;

print "not " if $+[2] != 3 or $-[2] != 2;
print "ok $test\n";
$test++;

print "not " 
   if defined $+[3] or defined $-[3] or defined $+[4] or defined $-[4];
print "ok $test\n";
$test++;

/.(a)(b)?(a)/;
print "not " if $#+ != 3 or $#- != 3;
print "ok $test\n";
$test++;

print "not " if $+[0] != 3 or $-[0] != 0;
print "ok $test\n";
$test++;

print "not " if $+[1] != 2 or $-[1] != 1;
print "ok $test\n";
$test++;

print "not " if $+[3] != 3 or $-[3] != 2;
print "ok $test\n";
$test++;

print "not " 
   if defined $+[2] or defined $-[2] or defined $+[4] or defined $-[4];
print "ok $test\n";
$test++;

/.(a)/;
print "not " if $#+ != 1 or $#- != 1;
print "ok $test\n";
$test++;

print "not " if $+[0] != 2 or $-[0] != 0;
print "ok $test\n";
$test++;

print "not " if $+[1] != 2 or $-[1] != 1;
print "ok $test\n";
$test++;

print "not " 
   if defined $+[2] or defined $-[2] or defined $+[3] or defined $-[3];
print "ok $test\n";
$test++;

eval { $+[0] = 13; };
print "not " 
   if $@ !~ /^Modification of a read-only value attempted/;
print "ok $test\n";
$test++;

eval { $-[0] = 13; };
print "not " 
   if $@ !~ /^Modification of a read-only value attempted/;
print "ok $test\n";
$test++;

eval { @+ = (7, 6, 5); };
print "not " 
   if $@ !~ /^Modification of a read-only value attempted/;
print "ok $test\n";
$test++;

eval { @- = qw(foo bar); };
print "not " 
   if $@ !~ /^Modification of a read-only value attempted/;
print "ok $test\n";
$test++;

/.(a)(ba*)?/;
print "#$#-..$#+\nnot " if $#+ != 2 or $#- != 1;
print "ok $test\n";
$test++;

$_ = 'aaa';
pos = 1;
@a = /\Ga/g;
print "not " unless "@a" eq "a a";
print "ok $test\n";
$test++;

$str = 'abcde';
pos $str = 2;

print "not " if $str =~ /^\G/;
print "ok $test\n";
$test++;

print "not " if $str =~ /^.\G/;
print "ok $test\n";
$test++;

print "not " unless $str =~ /^..\G/;
print "ok $test\n";
$test++;

print "not " if $str =~ /^...\G/;
print "ok $test\n";
$test++;

print "not " unless $str =~ /.\G./ and $& eq 'bc';
print "ok $test\n";
$test++;

print "not " unless $str =~ /\G../ and $& eq 'cd';
print "ok $test\n";
$test++;

undef $foo; undef $bar;
print "#'$str','$foo','$bar'\nnot "
    unless $str =~ /b(?{$foo = $_; $bar = pos})c/ 
	and $foo eq 'abcde' and $bar eq 2;
print "ok $test\n";
$test++;

undef $foo; undef $bar;
pos $str = undef;
print "#'$str','$foo','$bar'\nnot "
    unless $str =~ /b(?{$foo = $_; $bar = pos})c/g 
	and $foo eq 'abcde' and $bar eq 2 and pos $str eq 3;
print "ok $test\n";
$test++;

$_ = $str;

undef $foo; undef $bar;
print "#'$str','$foo','$bar'\nnot "
    unless /b(?{$foo = $_; $bar = pos})c/ 
	and $foo eq 'abcde' and $bar eq 2;
print "ok $test\n";
$test++;

undef $foo; undef $bar;
print "#'$str','$foo','$bar'\nnot "
    unless /b(?{$foo = $_; $bar = pos})c/g 
	and $foo eq 'abcde' and $bar eq 2 and pos eq 3;
print "ok $test\n";
$test++;

undef $foo; undef $bar;
pos = undef;
1 while /b(?{$foo = $_; $bar = pos})c/g;
print "#'$str','$foo','$bar'\nnot "
    unless $foo eq 'abcde' and $bar eq 2 and not defined pos;
print "ok $test\n";
$test++;

undef $foo; undef $bar;
$_ = 'abcde|abcde';
print "#'$str','$foo','$bar','$_'\nnot "
    unless s/b(?{$foo = $_; $bar = pos})c/x/g and $foo eq 'abcde|abcde' 
	and $bar eq 8 and $_ eq 'axde|axde';
print "ok $test\n";
$test++;

@res = ();
# List context:
$_ = 'abcde|abcde';
@dummy = /([ace]).(?{push @res, $1,$2})([ce])(?{push @res, $1,$2})/g;
@res = map {defined $_ ? "'$_'" : 'undef'} @res;
$res = "@res";
print "#'@res' '$_'\nnot "
    unless "@res" eq "'a' undef 'a' 'c' 'e' undef 'a' undef 'a' 'c'";
print "ok $test\n";
$test++;

@res = ();
@dummy = /([ace]).(?{push @res, $`,$&,$'})([ce])(?{push @res, $`,$&,$'})/g;
@res = map {defined $_ ? "'$_'" : 'undef'} @res;
$res = "@res";
print "#'@res' '$_'\nnot "
    unless "@res" eq
  "'' 'ab' 'cde|abcde' " .
  "'' 'abc' 'de|abcde' " .
  "'abcd' 'e|' 'abcde' " .
  "'abcde|' 'ab' 'cde' " .
  "'abcde|' 'abc' 'de'" ;
print "ok $test\n";
$test++;

#Some more \G anchor checks
$foo='aabbccddeeffgg';

pos($foo)=1;

$foo=~/.\G(..)/g;
print "not " unless($1 eq 'ab');
print "ok $test\n";
$test++;

pos($foo) += 1;
$foo=~/.\G(..)/g;
print "not " unless($1 eq 'cc');
print "ok $test\n";
$test++;

pos($foo) += 1;
$foo=~/.\G(..)/g;
print "not " unless($1 eq 'de');
print "ok $test\n";
$test++;

print "not " unless $foo =~ /\Gef/g;
print "ok $test\n";
$test++;

undef pos $foo;

$foo=~/\G(..)/g;
print "not " unless($1  eq 'aa');
print "ok $test\n";
$test++;

$foo=~/\G(..)/g;
print "not " unless($1  eq 'bb');
print "ok $test\n";
$test++;

pos($foo)=5;
$foo=~/\G(..)/g;
print "not " unless($1  eq 'cd');
print "ok $test\n";
$test++;

$_='123x123'; 
@res = /(\d*|x)/g;
print "not " unless('123||x|123|' eq join '|', @res);
print "ok $test\n";
$test++;

# see if matching against temporaries (created via pp_helem()) is safe
{ foo => "ok $test\n".$^X }->{foo} =~ /^(.*)\n/g;
print "$1\n";
$test++;

# See if $i work inside (?{}) in the presense of saved substrings and
# changing $_
@a = qw(foo bar);
@b = ();
s/(\w)(?{push @b, $1})/,$1,/g for @a;

print "# \@b='@b', expect 'f o o b a r'\nnot " unless("@b" eq "f o o b a r");
print "ok $test\n";
$test++;

print "not " unless("@a" eq ",f,,o,,o, ,b,,a,,r,");
print "ok $test\n";
$test++;

$brackets = qr{
	         {  (?> [^{}]+ | (??{ $brackets }) )* }
	      }x;

"{{}" =~ $brackets;
print "ok $test\n";		# Did we survive?
$test++;

"something { long { and } hairy" =~ $brackets;
print "ok $test\n";		# Did we survive?
$test++;

"something { long { and } hairy" =~ m/((??{ $brackets }))/;
print "not " unless $1 eq "{ and }";
print "ok $test\n";
$test++;

$_ = "a-a\nxbb";
pos=1;
m/^-.*bb/mg and print "not ";
print "ok $test\n";
$test++;

$text = "aaXbXcc";
pos($text)=0;
$text =~ /\GXb*X/g and print 'not ';
print "ok $test\n";
$test++;

$text = "xA\n" x 500;
$text =~ /^\s*A/m and print 'not ';
print "ok $test\n";
$test++;

$text = "abc dbf";
@res = ($text =~ /.*?(b).*?\b/g);
"@res" eq 'b b' or print 'not ';
print "ok $test\n";
$test++;

@a = map chr,0..255;

@b = grep(/\S/,@a);
@c = grep(/[^\s]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\S/,@a);
@c = grep(/[\S]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\s/,@a);
@c = grep(/[^\S]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\s/,@a);
@c = grep(/[\s]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\D/,@a);
@c = grep(/[^\d]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\D/,@a);
@c = grep(/[\D]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\d/,@a);
@c = grep(/[^\D]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\d/,@a);
@c = grep(/[\d]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\W/,@a);
@c = grep(/[^\w]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\W/,@a);
@c = grep(/[\W]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\w/,@a);
@c = grep(/[^\W]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\w/,@a);
@c = grep(/[\w]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

# see if backtracking optimization works correctly
"\n\n" =~ /\n  $ \n/x or print "not ";
print "ok $test\n";
$test++;

"\n\n" =~ /\n* $ \n/x or print "not ";
print "ok $test\n";
$test++;

"\n\n" =~ /\n+ $ \n/x or print "not ";
print "ok $test\n";
$test++;

[] =~ /^ARRAY/ or print "# [] \nnot ";
print "ok $test\n";
$test++;

eval << 'EOE';
{
 package S;
 use overload '""' => sub { 'Object S' };
 sub new { bless [] }
}
$a = 'S'->new;
EOE

$a and $a =~ /^Object\sS/ or print "# '$a' \nnot ";
print "ok $test\n";
$test++;

# test result of match used as match (!)
'a1b' =~ ('xyz' =~ /y/) and $` eq 'a' or print "not ";
print "ok $test\n";
$test++;

'a1b' =~ ('xyz' =~ /t/) and $` eq 'a' or print "not ";
print "ok $test\n";
$test++;

$w = 0;
{
    local $SIG{__WARN__} = sub { $w = 1 };
    local $^W = 1;
	$w = 1 if ("1\n" x 102) =~ /^\s*\n/m;
}
print $w ? "not " : "", "ok $test\n";
$test++;

my %space = ( spc   => " ",
	      tab   => "\t",
	      cr    => "\r",
	      lf    => "\n",
	      ff    => "\f",
# There's no \v but the vertical tabulator seems miraculously
# be 11 both in ASCII and EBCDIC.
	      vt    => chr(11),
	      false => "space" );

my @space0 = sort grep { $space{$_} =~ /\s/ }          keys %space;
my @space1 = sort grep { $space{$_} =~ /[[:space:]]/ } keys %space;
my @space2 = sort grep { $space{$_} =~ /[[:blank:]]/ } keys %space;

print "not " unless "@space0" eq "cr ff lf spc tab";
print "ok $test # @space0\n";
$test++;

print "not " unless "@space1" eq "cr ff lf spc tab vt";
print "ok $test # @space1\n";
$test++;

print "not " unless "@space2" eq "spc tab";
print "ok $test # @space2\n";
$test++;
 
# bugid 20001021.005 - this caused a SEGV
print "not " unless undef =~ /^([^\/]*)(.*)$/;
print "ok $test\n";
$test++;

{
  # japhy -- added 03/03/2001
  () = (my $str = "abc") =~ /(...)/;
  $str = "def";
  print "not " if $1 ne "abc";
  print "ok $test\n";
  $test++;
}
