#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/base/lex.t,v 1.1.1.1 1994/09/10 06:27:39 gclarkii Exp $

print "1..18\n";

$ # this is the register <space>
= 'x';

print "#1	:$ : eq :x:\n";
if ($  eq 'x') {print "ok 1\n";} else {print "not ok 1\n";}

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
if (!$@) {print "ok 6\n";} else {print "not ok 6\n";}

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
ok 16\n
;

print q<ok 17
>;

print <<;   # Yow!
ok 18

# previous line intentionally left blank.
