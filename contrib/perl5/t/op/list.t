#!./perl

# $RCSfile: list.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:02 $

print "1..27\n";

@foo = (1, 2, 3, 4);
if ($foo[0] == 1 && $foo[3] == 4) {print "ok 1\n";} else {print "not ok 1\n";}

$_ = join(':',@foo);
if ($_ eq '1:2:3:4') {print "ok 2\n";} else {print "not ok 2\n";}

($a,$b,$c,$d) = (1,2,3,4);
if ("$a;$b;$c;$d" eq '1;2;3;4') {print "ok 3\n";} else {print "not ok 3\n";}

($c,$b,$a) = split(/ /,"111 222 333");
if ("$a;$b;$c" eq '333;222;111') {print "ok 4\n";} else {print "not ok 4\n";}

($a,$b,$c) = ($c,$b,$a);
if ("$a;$b;$c" eq '111;222;333') {print "ok 5\n";} else {print "not ok 5 $a;$b;$c\n";}

($a, $b) = ($b, $a);
if ("$a;$b;$c" eq '222;111;333') {print "ok 6\n";} else {print "not ok 6\n";}

($a, $b[1], $c{2}, $d) = (1, 2, 3, 4);
if ($a eq 1) {print "ok 7\n";} else {print "not ok 7\n";}
if ($b[1] eq 2) {print "ok 8\n";} else {print "not ok 8\n";}
if ($c{2} eq 3) {print "ok 9\n";} else {print "not ok 9\n";}
if ($d eq 4) {print "ok 10\n";} else {print "not ok 10\n";}

@foo = (1,2,3,4,5,6,7,8);
($a, $b, $c, $d) = @foo;
print "#11	$a;$b;$c;$d eq 1;2;3;4\n";
if ("$a;$b;$c;$d" eq '1;2;3;4') {print "ok 11\n";} else {print "not ok 11\n";}

@foo = @bar = (1);
if (join(':',@foo,@bar) eq '1:1') {print "ok 12\n";} else {print "not ok 12\n";}

@foo = ();
@foo = 1+2+3;
if (join(':',@foo) eq '6') {print "ok 13\n";} else {print "not ok 13\n";}

for ($x = 0; $x < 3; $x++) {
    ($a, $b, $c) = 
	    $x == 0?
		    ('ok ', 14, "\n"):
	    $x == 1?
		    ('ok ', 15, "\n"):
	    # default
		    ('ok ', 16, "\n");

    print $a,$b,$c;
}

@a = ($x == 12345 || (1,2,3));
if (join('',@a) eq '123') {print "ok 17\n";} else {print "not ok 17\n";}

@a = ($x == $x || (4,5,6));
if (join('',@a) eq '1') {print "ok 18\n";} else {print "not ok 18\n";}

if (join('',1,2,(3,4,5)) eq '12345'){print "ok 19\n";}else{print "not ok 19\n";}
if (join('',(1,2,3,4,5)) eq '12345'){print "ok 20\n";}else{print "not ok 20\n";}
if (join('',(1,2,3,4),5) eq '12345'){print "ok 21\n";}else{print "not ok 21\n";}
if (join('',1,(2,3,4),5) eq '12345'){print "ok 22\n";}else{print "not ok 22\n";}
if (join('',1,2,(3,4),5) eq '12345'){print "ok 23\n";}else{print "not ok 23\n";}
if (join('',1,2,3,(4),5) eq '12345'){print "ok 24\n";}else{print "not ok 24\n";}

for ($x = 0; $x < 3; $x++) {
    ($a, $b, $c) = do {
	    if ($x == 0) {
		('ok ', 25, "\n");
	    }
	    elsif ($x == 1) {
		('ok ', 26, "\n");
	    }
	    else {
		('ok ', 27, "\n");
	    }
	};

    print $a,$b,$c;
}

