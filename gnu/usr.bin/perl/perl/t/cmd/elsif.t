#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/cmd/elsif.t,v 1.1.1.1 1993/08/23 21:30:05 nate Exp $

sub foo {
    if ($_[0] == 1) {
	1;
    }
    elsif ($_[0] == 2) {
	2;
    }
    elsif ($_[0] == 3) {
	3;
    }
    else {
	4;
    }
}

print "1..4\n";

if (($x = do foo(1)) == 1) {print "ok 1\n";} else {print "not ok 1 '$x'\n";}
if (($x = do foo(2)) == 2) {print "ok 2\n";} else {print "not ok 2 '$x'\n";}
if (($x = do foo(3)) == 3) {print "ok 3\n";} else {print "not ok 3 '$x'\n";}
if (($x = do foo(4)) == 4) {print "ok 4\n";} else {print "not ok 4 '$x'\n";}
