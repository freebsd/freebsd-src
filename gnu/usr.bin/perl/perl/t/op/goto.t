#!./perl

# $RCSfile: goto.t,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:30:03 $

print "1..3\n";

while (0) {
    $foo = 1;
  label1:
    $foo = 2;
    goto label2;
} continue {
    $foo = 0;
    goto label4;
  label3:
    $foo = 4;
    goto label4;
}
goto label1;

$foo = 3;

label2:
print "#1\t:$foo: == 2\n";
if ($foo == 2) {print "ok 1\n";} else {print "not ok 1\n";}
goto label3;

label4:
print "#2\t:$foo: == 4\n";
if ($foo == 4) {print "ok 2\n";} else {print "not ok 2\n";}

$x = `./perl -e 'goto foo;' 2>&1`;
if ($x =~ /label/) {print "ok 3\n";} else {print "not ok 3\n";}
