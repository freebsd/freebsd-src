#!./perl

# $RCSfile: append.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:36 $

print "1..13\n";

$a = 'ab' . 'c';	# compile time
$b = 'def';

$c = $a . $b;
print "#1\t:$c: eq :abcdef:\n";
if ($c eq 'abcdef') {print "ok 1\n";} else {print "not ok 1\n";}

$c .= 'xyz';
print "#2\t:$c: eq :abcdefxyz:\n";
if ($c eq 'abcdefxyz') {print "ok 2\n";} else {print "not ok 2\n";}

$_ = $a;
$_ .= $b;
print "#3\t:$_: eq :abcdef:\n";
if ($_ eq 'abcdef') {print "ok 3\n";} else {print "not ok 3\n";}

# test that when right argument of concat is UTF8, and is the same
# variable as the target, and the left argument is not UTF8, it no
# longer frees the wrong string.
{
    sub r2 {
	my $string = '';
	$string .= pack("U0a*", 'mnopqrstuvwx');
	$string = "abcdefghijkl$string";
    }

    r2() and print "ok $_\n" for qw/ 4 5 /;
}

# test that nul bytes get copied
{
# Character 'b' occurs at codepoint 130 decimal or \202 octal
# under an EBCDIC coded character set.
#    my($a, $ab) = ("a", "a\000b");
    my($a, $ab) = ("\141", "\141\000\142");
    my($u, $ub) = map pack("U0a*", $_), $a, $ab;
    my $t1 = $a; $t1 .= $ab;
    print $t1 =~ /\142/ ? "ok 6\n" : "not ok 6\t# $t1\n";
    my $t2 = $a; $t2 .= $ub;
    print $t2 =~ /\142/ ? "ok 7\n" : "not ok 7\t# $t2\n";
    my $t3 = $u; $t3 .= $ab;
    print $t3 =~ /\142/ ? "ok 8\n" : "not ok 8\t# $t3\n";
    my $t4 = $u; $t4 .= $ub;
    print $t4 =~ /\142/ ? "ok 9\n" : "not ok 9\t# $t4\n";
    my $t5 = $a; $t5 = $ab . $t5;
    print $t5 =~ /\142/ ? "ok 10\n" : "not ok 10\t# $t5\n";
    my $t6 = $a; $t6 = $ub . $t6;
    print $t6 =~ /\142/ ? "ok 11\n" : "not ok 11\t# $t6\n";
    my $t7 = $u; $t7 = $ab . $t7;
    print $t7 =~ /\142/ ? "ok 12\n" : "not ok 12\t# $t7\n";
    my $t8 = $u; $t8 = $ub . $t8;
    print $t8 =~ /\142/ ? "ok 13\n" : "not ok 13\t# $t8\n";
}
