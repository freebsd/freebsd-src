#!./perl

print "1..71\n";

$x = 'foo';
$_ = "x";
s/x/\$x/;
print "#1\t:$_: eq :\$x:\n";
if ($_ eq '$x') {print "ok 1\n";} else {print "not ok 1\n";}

$_ = "x";
s/x/$x/;
print "#2\t:$_: eq :foo:\n";
if ($_ eq 'foo') {print "ok 2\n";} else {print "not ok 2\n";}

$_ = "x";
s/x/\$x $x/;
print "#3\t:$_: eq :\$x foo:\n";
if ($_ eq '$x foo') {print "ok 3\n";} else {print "not ok 3\n";}

$b = 'cd';
($a = 'abcdef') =~ s<(b${b}e)>'\n$1';
print "#4\t:$1: eq :bcde:\n";
print "#4\t:$a: eq :a\\n\$1f:\n";
if ($1 eq 'bcde' && $a eq 'a\n$1f') {print "ok 4\n";} else {print "not ok 4\n";}

$a = 'abacada';
if (($a =~ s/a/x/g) == 4 && $a eq 'xbxcxdx')
    {print "ok 5\n";} else {print "not ok 5\n";}

if (($a =~ s/a/y/g) == 0 && $a eq 'xbxcxdx')
    {print "ok 6\n";} else {print "not ok 6 $a\n";}

if (($a =~ s/b/y/g) == 1 && $a eq 'xyxcxdx')
    {print "ok 7\n";} else {print "not ok 7 $a\n";}

$_ = 'ABACADA';
if (/a/i && s///gi && $_ eq 'BCD') {print "ok 8\n";} else {print "not ok 8 $_\n";}

$_ = '\\' x 4;
if (length($_) == 4) {print "ok 9\n";} else {print "not ok 9\n";}
s/\\/\\\\/g;
if ($_ eq '\\' x 8) {print "ok 10\n";} else {print "not ok 10 $_\n";}

$_ = '\/' x 4;
if (length($_) == 8) {print "ok 11\n";} else {print "not ok 11\n";}
s/\//\/\//g;
if ($_ eq '\\//' x 4) {print "ok 12\n";} else {print "not ok 12\n";}
if (length($_) == 12) {print "ok 13\n";} else {print "not ok 13\n";}

$_ = 'aaaXXXXbbb';
s/^a//;
print $_ eq 'aaXXXXbbb' ? "ok 14\n" : "not ok 14\n";

$_ = 'aaaXXXXbbb';
s/a//;
print $_ eq 'aaXXXXbbb' ? "ok 15\n" : "not ok 15\n";

$_ = 'aaaXXXXbbb';
s/^a/b/;
print $_ eq 'baaXXXXbbb' ? "ok 16\n" : "not ok 16\n";

$_ = 'aaaXXXXbbb';
s/a/b/;
print $_ eq 'baaXXXXbbb' ? "ok 17\n" : "not ok 17\n";

$_ = 'aaaXXXXbbb';
s/aa//;
print $_ eq 'aXXXXbbb' ? "ok 18\n" : "not ok 18\n";

$_ = 'aaaXXXXbbb';
s/aa/b/;
print $_ eq 'baXXXXbbb' ? "ok 19\n" : "not ok 19\n";

$_ = 'aaaXXXXbbb';
s/b$//;
print $_ eq 'aaaXXXXbb' ? "ok 20\n" : "not ok 20\n";

$_ = 'aaaXXXXbbb';
s/b//;
print $_ eq 'aaaXXXXbb' ? "ok 21\n" : "not ok 21\n";

$_ = 'aaaXXXXbbb';
s/bb//;
print $_ eq 'aaaXXXXb' ? "ok 22\n" : "not ok 22\n";

$_ = 'aaaXXXXbbb';
s/aX/y/;
print $_ eq 'aayXXXbbb' ? "ok 23\n" : "not ok 23\n";

$_ = 'aaaXXXXbbb';
s/Xb/z/;
print $_ eq 'aaaXXXzbb' ? "ok 24\n" : "not ok 24\n";

$_ = 'aaaXXXXbbb';
s/aaX.*Xbb//;
print $_ eq 'ab' ? "ok 25\n" : "not ok 25\n";

$_ = 'aaaXXXXbbb';
s/bb/x/;
print $_ eq 'aaaXXXXxb' ? "ok 26\n" : "not ok 26\n";

# now for some unoptimized versions of the same.

$_ = 'aaaXXXXbbb';
$x ne $x || s/^a//;
print $_ eq 'aaXXXXbbb' ? "ok 27\n" : "not ok 27\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/a//;
print $_ eq 'aaXXXXbbb' ? "ok 28\n" : "not ok 28\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/^a/b/;
print $_ eq 'baaXXXXbbb' ? "ok 29\n" : "not ok 29\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/a/b/;
print $_ eq 'baaXXXXbbb' ? "ok 30\n" : "not ok 30\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/aa//;
print $_ eq 'aXXXXbbb' ? "ok 31\n" : "not ok 31\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/aa/b/;
print $_ eq 'baXXXXbbb' ? "ok 32\n" : "not ok 32\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/b$//;
print $_ eq 'aaaXXXXbb' ? "ok 33\n" : "not ok 33\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/b//;
print $_ eq 'aaaXXXXbb' ? "ok 34\n" : "not ok 34\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/bb//;
print $_ eq 'aaaXXXXb' ? "ok 35\n" : "not ok 35\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/aX/y/;
print $_ eq 'aayXXXbbb' ? "ok 36\n" : "not ok 36\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/Xb/z/;
print $_ eq 'aaaXXXzbb' ? "ok 37\n" : "not ok 37\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/aaX.*Xbb//;
print $_ eq 'ab' ? "ok 38\n" : "not ok 38\n";

$_ = 'aaaXXXXbbb';
$x ne $x || s/bb/x/;
print $_ eq 'aaaXXXXxb' ? "ok 39\n" : "not ok 39\n";

$_ = 'abc123xyz';
s/(\d+)/$1*2/e;              # yields 'abc246xyz'
print $_ eq 'abc246xyz' ? "ok 40\n" : "not ok 40\n";
s/(\d+)/sprintf("%5d",$1)/e; # yields 'abc  246xyz'
print $_ eq 'abc  246xyz' ? "ok 41\n" : "not ok 41\n";
s/(\w)/$1 x 2/eg;            # yields 'aabbcc  224466xxyyzz'
print $_ eq 'aabbcc  224466xxyyzz' ? "ok 42\n" : "not ok 42\n";

$_ = "aaaaa";
print y/a/b/ == 5 ? "ok 43\n" : "not ok 43\n";
print y/a/b/ == 0 ? "ok 44\n" : "not ok 44\n";
print y/b// == 5 ? "ok 45\n" : "not ok 45\n";
print y/b/c/s == 5 ? "ok 46\n" : "not ok 46\n";
print y/c// == 1 ? "ok 47\n" : "not ok 47\n";
print y/c//d == 1 ? "ok 48\n" : "not ok 48\n";
print $_ eq "" ? "ok 49\n" : "not ok 49\n";

$_ = "Now is the %#*! time for all good men...";
print (($x=(y/a-zA-Z //cd)) == 7 ? "ok 50\n" : "not ok 50\n");
print y/ / /s == 8 ? "ok 51\n" : "not ok 51\n";

$_ = 'abcdefghijklmnopqrstuvwxyz0123456789';
tr/a-z/A-Z/;

print $_ eq 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789' ? "ok 52\n" : "not ok 52\n";

# same as tr/A-Z/a-z/;
if ($^O eq 'os390') {	# An EBCDIC variant.
    y[\301-\351][\201-\251];
} else {		# Ye Olde ASCII.  Or something like it.
    y[\101-\132][\141-\172];
}

print $_ eq 'abcdefghijklmnopqrstuvwxyz0123456789' ? "ok 53\n" : "not ok 53\n";

if (ord("+") == ord(",") - 1 && ord(",") == ord("-") - 1 &&
    ord("a") == ord("b") - 1 && ord("b") == ord("c") - 1) {
  $_ = '+,-';
  tr/+--/a-c/;
  print "not " unless $_ eq 'abc';
}
print "ok 54\n";

$_ = '+,-';
tr/+\--/a\/c/;
print $_ eq 'a,/' ? "ok 55\n" : "not ok 55\n";

$_ = '+,-';
tr/-+,/ab\-/;
print $_ eq 'b-a' ? "ok 56\n" : "not ok 56\n";


# test recursive substitutions
# code based on the recursive expansion of makefile variables

my %MK = (
    AAAAA => '$(B)', B=>'$(C)', C => 'D',			# long->short
    E     => '$(F)', F=>'p $(G) q', G => 'HHHHH',	# short->long
    DIR => '$(UNDEFINEDNAME)/xxx',
);
sub var { 
    my($var,$level) = @_;
    return "\$($var)" unless exists $MK{$var};
    return exp_vars($MK{$var}, $level+1); # can recurse
}
sub exp_vars { 
    my($str,$level) = @_;
    $str =~ s/\$\((\w+)\)/var($1, $level+1)/ge; # can recurse
    #warn "exp_vars $level = '$str'\n";
    $str;
}

print exp_vars('$(AAAAA)',0)           eq 'D'
	? "ok 57\n" : "not ok 57\n";
print exp_vars('$(E)',0)               eq 'p HHHHH q'
	? "ok 58\n" : "not ok 58\n";
print exp_vars('$(DIR)',0)             eq '$(UNDEFINEDNAME)/xxx'
	? "ok 59\n" : "not ok 59\n";
print exp_vars('foo $(DIR)/yyy bar',0) eq 'foo $(UNDEFINEDNAME)/xxx/yyy bar'
	? "ok 60\n" : "not ok 60\n";

# a match nested in the RHS of a substitution:

$_ = "abcd";
s/(..)/$x = $1, m#.#/eg;
print $x eq "cd" ? "ok 61\n" : "not ok 61\n";

# Subst and lookbehind

$_="ccccc";
s/(?<!x)c/x/g;
print $_ eq "xxxxx" ? "ok 62\n" : "not ok 62 # `$_' ne `xxxxx'\n";

$_="ccccc";
s/(?<!x)(c)/x/g;
print $_ eq "xxxxx" ? "ok 63\n" : "not ok 63 # `$_' ne `xxxxx'\n";

$_="foobbarfoobbar";
s/(?<!r)foobbar/foobar/g;
print $_ eq "foobarfoobbar" ? "ok 64\n" : "not ok 64 # `$_' ne `foobarfoobbar'\n";

$_="foobbarfoobbar";
s/(?<!ar)(foobbar)/foobar/g;
print $_ eq "foobarfoobbar" ? "ok 65\n" : "not ok 65 # `$_' ne `foobarfoobbar'\n";

$_="foobbarfoobbar";
s/(?<!ar)foobbar/foobar/g;
print $_ eq "foobarfoobbar" ? "ok 66\n" : "not ok 66 # `$_' ne `foobarfoobbar'\n";

# check parsing of split subst with comment
eval 's{foo} # this is a comment, not a delimiter
       {bar};';
print @? ? "not ok 67\n" : "ok 67\n";

# check if squashing works at the end of string
$_="baacbaa";
tr/a/b/s;
print $_ eq "bbcbb" ? "ok 68\n" : "not ok 68 # `$_' ne `bbcbb'\n";

# XXX TODO: Most tests above don't test return values of the ops. They should.
$_ = "ab";
print (s/a/b/ == 1 ? "ok 69\n" : "not ok 69\n");

$_ = <<'EOL';
     $url = new URI::URL "http://www/";   die if $url eq "xXx";
EOL
$^R = 'junk';

$foo = ' $@%#lowercase $@%# lowercase UPPERCASE$@%#UPPERCASE' .
  ' $@%#lowercase$@%#lowercase$@%# lowercase lowercase $@%#lowercase' .
  ' lowercase $@%#MiXeD$@%# ';

s{  \d+          \b [,.;]? (?{ 'digits' })
   |
    [a-z]+       \b [,.;]? (?{ 'lowercase' })
   |
    [A-Z]+       \b [,.;]? (?{ 'UPPERCASE' })
   |
    [A-Z] [a-z]+ \b [,.;]? (?{ 'Capitalized' })
   |
    [A-Za-z]+    \b [,.;]? (?{ 'MiXeD' })
   |
    [A-Za-z0-9]+ \b [,.;]? (?{ 'alphanumeric' })
   |
    \s+                    (?{ ' ' })
   |
    [^A-Za-z0-9\s]+          (?{ '$@%#' })
}{$^R}xg;
print ($_ eq $foo ? "ok 70\n" : "not ok 70\n#'$_'\n#'$foo'\n");

$_ = 'x' x 20; 
s/\d*|x/<$&>/g; 
$foo = '<>' . ('<x><>' x 20) ;
print ($_ eq $foo ? "ok 71\n" : "not ok 71\n#'$_'\n#'$foo'\n");
