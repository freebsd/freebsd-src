#!./perl

print "1..29\n";

$FS = ':';

$_ = 'a:b:c';

($a,$b,$c) = split($FS,$_);

if (join(';',$a,$b,$c) eq 'a;b;c') {print "ok 1\n";} else {print "not ok 1\n";}

@ary = split(/:b:/);
if (join("$_",@ary) eq 'aa:b:cc') {print "ok 2\n";} else {print "not ok 2\n";}

$_ = "abc\n";
@xyz = (@ary = split(//));
if (join(".",@ary) eq "a.b.c.\n") {print "ok 3\n";} else {print "not ok 3\n";}

$_ = "a:b:c::::";
@ary = split(/:/);
if (join(".",@ary) eq "a.b.c") {print "ok 4\n";} else {print "not ok 4\n";}

$_ = join(':',split(' ',"    a b\tc \t d "));
if ($_ eq 'a:b:c:d') {print "ok 5\n";} else {print "not ok 5 #$_#\n";}

$_ = join(':',split(/ */,"foo  bar bie\tdoll"));
if ($_ eq "f:o:o:b:a:r:b:i:e:\t:d:o:l:l")
	{print "ok 6\n";} else {print "not ok 6\n";}

$_ = join(':', 'foo', split(/ /,'a b  c'), 'bar');
if ($_ eq "foo:a:b::c:bar") {print "ok 7\n";} else {print "not ok 7 $_\n";}

# Can we say how many fields to split to?
$_ = join(':', split(' ','1 2 3 4 5 6', 3));
print $_ eq '1:2:3 4 5 6' ? "ok 8\n" : "not ok 8 $_\n";

# Can we do it as a variable?
$x = 4;
$_ = join(':', split(' ','1 2 3 4 5 6', $x));
print $_ eq '1:2:3:4 5 6' ? "ok 9\n" : "not ok 9 $_\n";

# Does the 999 suppress null field chopping?
$_ = join(':', split(/:/,'1:2:3:4:5:6:::', 999));
print $_ eq '1:2:3:4:5:6:::' ? "ok 10\n" : "not ok 10 $_\n";

# Does assignment to a list imply split to one more field than that?
if ($^O eq 'MSWin32') { $foo = `.\\perl -D1024 -e "(\$a,\$b) = split;" 2>&1` }
elsif ($^O eq 'VMS')  { $foo = `./perl "-D1024" -e "(\$a,\$b) = split;" 2>&1` }
else                  { $foo = `./perl -D1024 -e '(\$a,\$b) = split;' 2>&1` }
print $foo =~ /DEBUGGING/ || $foo =~ /SV = (VOID|IV\(3\))/ ? "ok 11\n" : "not ok 11\n";

# Can we say how many fields to split to when assigning to a list?
($a,$b) = split(' ','1 2 3 4 5 6', 2);
$_ = join(':',$a,$b);
print $_ eq '1:2 3 4 5 6' ? "ok 12\n" : "not ok 12 $_\n";

# do subpatterns generate additional fields (without trailing nulls)?
$_ = join '|', split(/,|(-)/, "1-10,20,,,");
print $_ eq "1|-|10||20" ? "ok 13\n" : "not ok 13\n";

# do subpatterns generate additional fields (with a limit)?
$_ = join '|', split(/,|(-)/, "1-10,20,,,", 10);
print $_ eq "1|-|10||20||||||" ? "ok 14\n" : "not ok 14\n";

# is the 'two undefs' bug fixed?
(undef, $a, undef, $b) = qw(1 2 3 4);
print "$a|$b" eq "2|4" ? "ok 15\n" : "not ok 15\n";

# .. even for locals?
{
  local(undef, $a, undef, $b) = qw(1 2 3 4);
  print "$a|$b" eq "2|4" ? "ok 16\n" : "not ok 16\n";
}

# check splitting of null string
$_ = join('|', split(/x/,   '',-1), 'Z');
print $_ eq "Z" ? "ok 17\n" : "#$_\nnot ok 17\n";

$_ = join('|', split(/x/,   '', 1), 'Z');
print $_ eq "Z" ? "ok 18\n" : "#$_\nnot ok 18\n";

$_ = join('|', split(/(p+)/,'',-1), 'Z');
print $_ eq "Z" ? "ok 19\n" : "#$_\nnot ok 19\n";

$_ = join('|', split(/.?/,  '',-1), 'Z');
print $_ eq "Z" ? "ok 20\n" : "#$_\nnot ok 20\n";


# Are /^/m patterns scanned?
$_ = join '|', split(/^a/m, "a b a\na d a", 20);
print $_ eq "| b a\n| d a" ? "ok 21\n" : "not ok 21\n# `$_'\n";

# Are /$/m patterns scanned?
$_ = join '|', split(/a$/m, "a b a\na d a", 20);
print $_ eq "a b |\na d |" ? "ok 22\n" : "not ok 22\n# `$_'\n";

# Are /^/m patterns scanned?
$_ = join '|', split(/^aa/m, "aa b aa\naa d aa", 20);
print $_ eq "| b aa\n| d aa" ? "ok 23\n" : "not ok 23\n# `$_'\n";

# Are /$/m patterns scanned?
$_ = join '|', split(/aa$/m, "aa b aa\naa d aa", 20);
print $_ eq "aa b |\naa d |" ? "ok 24\n" : "not ok 24\n# `$_'\n";

# Greedyness:
$_ = "a : b :c: d";
@ary = split(/\s*:\s*/);
if (($res = join(".",@ary)) eq "a.b.c.d") {print "ok 25\n";} else {print "not ok 25\n# res=`$res' != `a.b.c.d'\n";}

# use of match result as pattern (!)
'p:q:r:s' eq join ':', split('abc' =~ /b/, 'p1q1r1s') or print "not ";
print "ok 26\n";

# /^/ treated as /^/m
$_ = join ':', split /^/, "ab\ncd\nef\n";
print "not " if $_ ne "ab\n:cd\n:ef\n";
print "ok 27\n";

# see if @a = @b = split(...) optimization works
@list1 = @list2 = split ('p',"a p b c p");
print "not " if @list1 != @list2 or "@list1" ne "@list2"
             or @list1 != 2 or "@list1" ne "a   b c ";
print "ok 28\n";

# zero-width assertion
$_ = join ':', split /(?=\w)/, "rm b";
print "not" if $_ ne "r:m :b";
print "ok 29\n";
