#!./perl

# $RCSfile: sort.t,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:30:02 $

print "1..10\n";

sub reverse { $a lt $b ? 1 : $a gt $b ? -1 : 0; }

@harry = ('dog','cat','x','Cain','Abel');
@george = ('gone','chased','yz','Punished','Axed');

$x = join('', sort @harry);
print ($x eq 'AbelCaincatdogx' ? "ok 1\n" : "not ok 1\n");

$x = join('', sort reverse @harry);
print ($x eq 'xdogcatCainAbel' ? "ok 2\n" : "not ok 2\n");

$x = join('', sort @george, 'to', @harry);
print ($x eq 'AbelAxedCainPunishedcatchaseddoggonetoxyz'?"ok 3\n":"not ok 3\n");

@a = ();
@b = reverse @a;
print ("@b" eq "" ? "ok 4\n" : "not ok 4 (@b)\n");

@a = (1);
@b = reverse @a;
print ("@b" eq "1" ? "ok 5\n" : "not ok 5 (@b)\n");

@a = (1,2);
@b = reverse @a;
print ("@b" eq "2 1" ? "ok 6\n" : "not ok 6 (@b)\n");

@a = (1,2,3);
@b = reverse @a;
print ("@b" eq "3 2 1" ? "ok 7\n" : "not ok 7 (@b)\n");

@a = (1,2,3,4);
@b = reverse @a;
print ("@b" eq "4 3 2 1" ? "ok 8\n" : "not ok 8 (@b)\n");

@a = (10,2,3,4);
@b = sort {$a <=> $b;} @a;
print ("@b" eq "2 3 4 10" ? "ok 9\n" : "not ok 9 (@b)\n");

$sub = 'reverse';
$x = join('', sort $sub @harry);
print ($x eq 'xdogcatCainAbel' ? "ok 10\n" : "not ok 10\n");

