#!./perl

# $RCSfile: vec.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:36 $

print "1..15\n";

print vec($foo,0,1) == 0 ? "ok 1\n" : "not ok 1\n";
print length($foo) == 0 ? "ok 2\n" : "not ok 2\n";
vec($foo,0,1) = 1;
print length($foo) == 1 ? "ok 3\n" : "not ok 3\n";
print ord($foo) == 1 ? "ok 4\n" : "not ok 4\n";
print vec($foo,0,1) == 1 ? "ok 5\n" : "not ok 5\n";

print vec($foo,20,1) == 0 ? "ok 6\n" : "not ok 6\n";
vec($foo,20,1) = 1;
print vec($foo,20,1) == 1 ? "ok 7\n" : "not ok 7\n";
print length($foo) == 3 ? "ok 8\n" : "not ok 8\n";
print vec($foo,1,8) == 0 ? "ok 9\n" : "not ok 9\n";
vec($foo,1,8) = 0xf1;
print vec($foo,1,8) == 0xf1 ? "ok 10\n" : "not ok 10\n";
print ((ord(substr($foo,1,1)) & 255) == 0xf1 ? "ok 11\n" : "not ok 11\n");
print vec($foo,2,4) == 1 ? "ok 12\n" : "not ok 12\n";
print vec($foo,3,4) == 15 ? "ok 13\n" : "not ok 13\n";
vec($Vec, 0, 32) = 0xbaddacab;
print $Vec eq "\xba\xdd\xac\xab" ? "ok 14\n" : "not ok 14\n";
print vec($Vec, 0, 32) == 3135089835 ? "ok 15\n" : "not ok 15\n";

