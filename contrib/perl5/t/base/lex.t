#!./perl

print "1..35\n";

$x = 'x';

print "#1	:$x: eq :x:\n";
if ($x eq 'x') {print "ok 1\n";} else {print "not ok 1\n";}

$x = $#;	# this is the register $#

if ($x eq '') {print "ok 2\n";} else {print "not ok 2\n";}

$x = $#x;

if ($x eq '-1') {print "ok 3\n";} else {print "not ok 3\n";}

$x = '\\'; # ';

if (length($x) == 1) {print "ok 4\n";} else {print "not ok 4\n";}

eval 'while (0) {
    print "foo\n";
}
/^/ && (print "ok 5\n");
';

eval '$foo{1} / 1;';
if (!$@) {print "ok 6\n";} else {print "not ok 6 $@\n";}

eval '$foo = 123+123.4+123e4+123.4E5+123.4e+5+.12;';

$foo = int($foo * 100 + .5);
if ($foo eq 2591024652) {print "ok 7\n";} else {print "not ok 7 :$foo:\n";}

print <<'EOF';
ok 8
EOF

$foo = 'ok 9';
print <<EOF;
$foo
EOF

eval <<\EOE, print $@;
print <<'EOF';
ok 10
EOF

$foo = 'ok 11';
print <<EOF;
$foo
EOF
EOE

print <<`EOS` . <<\EOF;
echo ok 12
EOS
ok 13
EOF

print qq/ok 14\n/;
print qq(ok 15\n);

print qq
[ok 16\n]
;

print q<ok 17
>;

print <<;   # Yow!
ok 18

# previous line intentionally left blank.

print <<E1 eq "foo\n\n" ? "ok 19\n" : "not ok 19\n";
@{[ <<E2 ]}
foo
E2
E1

print <<E1 eq "foo\n\n" ? "ok 20\n" : "not ok 20\n";
@{[
  <<E2
foo
E2
]}
E1

$foo = FOO;
$bar = BAR;
$foo{$bar} = BAZ;
$ary[0] = ABC;

print "$foo{$bar}" eq "BAZ" ? "ok 21\n" : "not ok 21\n";

print "${foo}{$bar}" eq "FOO{BAR}" ? "ok 22\n" : "not ok 22\n";
print "${foo{$bar}}" eq "BAZ" ? "ok 23\n" : "not ok 23\n";

print "FOO:" =~ /$foo[:]/ ? "ok 24\n" : "not ok 24\n";
print "ABC" =~ /^$ary[$A]$/ ? "ok 25\n" : "not ok 25\n";
print "FOOZ" =~ /^$foo[$A-Z]$/ ? "ok 26\n" : "not ok 26\n";

# MJD 19980425
($X, @X) = qw(a b c d); 
print "d" =~ /^$X[-1]$/ ? "ok 27\n" : "not ok 27\n";
print "a1" !~ /^$X[-1]$/ ? "ok 28\n" : "not ok 28\n";

print (((q{{\{\(}} . q{{\)\}}}) eq '{{\(}{\)}}') ? "ok 29\n" : "not ok 29\n");


$foo = "not ok 30\n";
$foo =~ s/^not /substr(<<EOF, 0, 0)/e;
  Ignored
EOF
print $foo;

# see if eval '', s///e, and heredocs mix

sub T {
    my ($where, $num) = @_;
    my ($p,$f,$l) = caller;
    print "# $p:$f:$l vs /$where/\nnot " unless "$p:$f:$l" =~ /$where/;
    print "ok $num\n";
}

my $test = 31;

{
# line 42 "plink"
    local $_ = "not ok ";
    eval q{
	s/^not /<<EOT/e and T '^main:\(eval \d+\):2$', $test++;
# fuggedaboudit
EOT
        print $_, $test++, "\n";
	T('^main:\(eval \d+\):6$', $test++);
# line 1 "plunk"
	T('^main:plunk:1$', $test++);
    };
    print "# $@\nnot ok $test\n" if $@;
    T '^main:plink:53$', $test++;
}
