#!./perl

print "1..9\n";

@a = (1..10);

sub j { join(":",@_) }

print "not " unless j(splice(@a,@a,0,11,12)) eq "" && j(@a) eq j(1..12);
print "ok 1\n";

print "not " unless j(splice(@a,-1)) eq "12" && j(@a) eq j(1..11);
print "ok 2\n";

print "not " unless j(splice(@a,0,1)) eq "1" && j(@a) eq j(2..11);
print "ok 3\n";

print "not " unless j(splice(@a,0,0,0,1)) eq "" && j(@a) eq j(0..11);
print "ok 4\n";

print "not " unless j(splice(@a,5,1,5)) eq "5" && j(@a) eq j(0..11);
print "ok 5\n";

print "not " unless j(splice(@a, 20, 0, 12, 13)) eq "" && j(@a) eq j(0..13);
print "ok 6\n";

print "not " unless j(splice(@a, -@a, @a, 1, 2, 3)) eq j(0..13) && j(@a) eq j(1..3);
print "ok 7\n";

print "not " unless j(splice(@a, 1, -1, 7, 7)) eq "2" && j(@a) eq j(1,7,7,3);
print "ok 8\n";

print "not " unless j(splice(@a,-3,-2,2)) eq j(7) && j(@a) eq j(1,2,7,3);
print "ok 9\n";
