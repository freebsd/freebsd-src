#!./perl

# $RCSfile: pack.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:11 $

print "1..60\n";

$format = "c2 x5 C C x s d i l a6";
# Need the expression in here to force ary[5] to be numeric.  This avoids
# test2 failing because ary2 goes str->numeric->str and ary doesn't.
@ary = (1,-100,127,128,32767,987.654321098 / 100.0,12345,123456,"abcdef");
$foo = pack($format,@ary);
@ary2 = unpack($format,$foo);

print ($#ary == $#ary2 ? "ok 1\n" : "not ok 1\n");

$out1=join(':',@ary);
$out2=join(':',@ary2);
print ($out1 eq $out2 ? "ok 2\n" : "not ok 2\n");

print ($foo =~ /def/ ? "ok 3\n" : "not ok 3\n");

# How about counting bits?

print +($x = unpack("%32B*", "\001\002\004\010\020\040\100\200\377")) == 16
	? "ok 4\n" : "not ok 4 $x\n";

print +($x = unpack("%32b69", "\001\002\004\010\020\040\100\200\017")) == 12
	? "ok 5\n" : "not ok 5 $x\n";

print +($x = unpack("%32B69", "\001\002\004\010\020\040\100\200\017")) == 9
	? "ok 6\n" : "not ok 6 $x\n";

my $sum = 129; # ASCII
$sum = 103 if ($^O eq 'os390'); # An EBCDIC variant.

print +($x = unpack("%32B*", "Now is the time for all good blurfl")) == $sum
	? "ok 7\n" : "not ok 7 $x\n";

open(BIN, "./perl") || open(BIN, "./perl.exe") 
    || die "Can't open ../perl or ../perl.exe: $!\n";
sysread BIN, $foo, 8192;
close BIN;

$sum = unpack("%32b*", $foo);
$longway = unpack("b*", $foo);
print $sum == $longway =~ tr/1/1/ ? "ok 8\n" : "not ok 8\n";

print +($x = unpack("I",pack("I", 0xFFFFFFFF))) == 0xFFFFFFFF
	? "ok 9\n" : "not ok 9 $x\n";

# check 'w'
my $test=10;
my @x = (5,130,256,560,32000,3097152,268435455,1073741844,
         '4503599627365785','23728385234614992549757750638446');
my $x = pack('w*', @x);
my $y = pack 'H*', '0581028200843081fa0081bd8440ffffff7f848080801487ffffffffffdb19caefe8e1eeeea0c2e1e3e8ede1ee6e';

print $x eq $y ? "ok $test\n" : "not ok $test\n"; $test++;

@y = unpack('w*', $y);
my $a;
while ($a = pop @x) {
  my $b = pop @y;
  print $a eq $b ? "ok $test\n" : "not ok $test\n$a\n$b\n"; $test++;
}

@y = unpack('w2', $x);

print scalar(@y) == 2 ? "ok $test\n" : "not ok $test\n"; $test++;
print $y[1] == 130 ? "ok $test\n" : "not ok $test\n"; $test++;

# test exeptions
eval { $x = unpack 'w', pack 'C*', 0xff, 0xff};
print $@ ne '' ? "ok $test\n" : "not ok $test\n"; $test++;

eval { $x = unpack 'w', pack 'C*', 0xff, 0xff, 0xff, 0xff};
print $@ ne '' ? "ok $test\n" : "not ok $test\n"; $test++;

eval { $x = unpack 'w', pack 'C*', 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
print $@ ne '' ? "ok $test\n" : "not ok $test\n"; $test++;

#
# test the "p" template

# literals
print((unpack("p",pack("p","foo")) eq "foo" ? "ok " : "not ok "),$test++,"\n");

# scalars
print((unpack("p",pack("p",$test)) == $test ? "ok " : "not ok "),$test++,"\n");

# temps
sub foo { my $a = "a"; return $a . $a++ . $a++ }
{
  local $^W = 1;
  my $last = $test;
  local $SIG{__WARN__} = sub {
	print "ok ",$test++,"\n" if $_[0] =~ /temporary val/
  };
  my $junk = pack("p", &foo);
  print "not ok ", $test++, "\n" if $last == $test;
}

# undef should give null pointer
print((pack("p", undef) =~ /^\0+/ ? "ok " : "not ok "),$test++,"\n");

# Check for optimizer bug (e.g.  Digital Unix GEM cc with -O4 on DU V4.0B gives
#                                4294967295 instead of -1)
#				 see #ifdef __osf__ in pp.c pp_unpack
# Test 30:
print( ((unpack("i",pack("i",-1))) == -1 ? "ok " : "not ok "),$test++,"\n");

# 31..36: test the pack lengths of s S i I l L
print "not " unless length(pack("s", 0)) == 2;
print "ok ", $test++, "\n";
 
print "not " unless length(pack("S", 0)) == 2;
print "ok ", $test++, "\n";
 
print "not " unless length(pack("i", 0)) >= 4;
print "ok ", $test++, "\n";

print "not " unless length(pack("I", 0)) >= 4;
print "ok ", $test++, "\n";

print "not " unless length(pack("l", 0)) == 4;
print "ok ", $test++, "\n";

print "not " unless length(pack("L", 0)) == 4;
print "ok ", $test++, "\n";

# 37..40: test the pack lengths of n N v V

print "not " unless length(pack("n", 0)) == 2;
print "ok ", $test++, "\n";

print "not " unless length(pack("N", 0)) == 4;
print "ok ", $test++, "\n";

print "not " unless length(pack("v", 0)) == 2;
print "ok ", $test++, "\n";

print "not " unless length(pack("V", 0)) == 4;
print "ok ", $test++, "\n";

# 41..56: test unpack-pack lengths

my @templates = qw(c C i I s S l L n N v V f d);

# quads not supported everywhere: if not, retest floats/doubles
# to preserve the test count...
eval { my $q = pack("q",0) };
push @templates, $@ !~ /Invalid type in pack/ ? qw(q Q) : qw(f d);

foreach my $t (@templates) {
    my @t = unpack("$t*", pack("$t*", 12, 34));
    print "not "
      unless @t == 2 and (($t[0] == 12 and $t[1] == 34) or ($t =~ /[nv]/i));
    print "ok ", $test++, "\n";
}

# 57..60: uuencode/decode

$in = join "", map { chr } 0..255;

# just to be anal, we do some random tr/`/ /
$uu = <<'EOUU';
M` $"`P0%!@<("0H+# T.#Q`1$A,4%187&!D:&QP='A\@(2(C)"4F)R@I*BLL
M+2XO,#$R,S0U-C<X.3H[/#T^/T!!0D-$149'2$E*2TQ-3D]045)35%565UA9
M6EM<75Y?8&%B8V1E9F=H:6IK;&UN;W!Q<G-T=79W>'EZ>WQ]?G^`@8*#A(6&
MAXB)BHN,C8Z/D)&2DY25EI>8F9J;G)V>GZ"AHJ.DI::GJ*FJJZRMKJ^PL;*S
MM+6VM[BYNKN\O;Z_P,'"P\3%QL?(R<K+S,W.S]#1TM/4U=;7V-G:V]S=WM_@
?X>+CY.7FY^CIZNOL[>[O\/'R\_3U]O?X^?K[_/W^_P `
EOUU

$_ = $uu;
tr/ /`/;
print "not " unless pack('u', $in) eq $_;
print "ok ", $test++, "\n";

print "not " unless unpack('u', $uu) eq $in;
print "ok ", $test++, "\n";

$in = "\x1f\x8b\x08\x08\x58\xdc\xc4\x35\x02\x03\x4a\x41\x50\x55\x00\xf3\x2a\x2d\x2e\x51\x48\xcc\xcb\x2f\xc9\x48\x2d\x52\x08\x48\x2d\xca\x51\x28\x2d\x4d\xce\x4f\x49\x2d\xe2\x02\x00\x64\x66\x60\x5c\x1a\x00\x00\x00";
$uu = <<'EOUU';
M'XL("%C<Q#4"`TI!4%4`\RHM+E%(S,LOR4@M4@A(+<I1*"U-SD])+>("`&1F
&8%P:````
EOUU

print "not " unless unpack('u', $uu) eq $in;
print "ok ", $test++, "\n";

# 60 identical to 59 except that backquotes have been changed to spaces

$uu = <<'EOUU';
M'XL("%C<Q#4" TI!4%4 \RHM+E%(S,LOR4@M4@A(+<I1*"U-SD])+>(" &1F
&8%P:    
EOUU

print "not " unless unpack('u', $uu) eq $in;
print "ok ", $test++, "\n";

# Note that first uuencoding known 'text' data and then checking the
# binary values of the uuencoded version would not be portable between
# character sets.  Uuencoding is meant for encoding binary data, not
# text data.
