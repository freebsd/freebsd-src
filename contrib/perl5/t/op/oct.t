#!./perl

# $RCSfile: oct.t,v $$Revision: 1.1.1.1 $$Date: 1998/09/09 07:00:02 $

print "1..8\n";

print +(oct('01234') == 01234) ? "ok" : "not ok", " 1\n";
print +(oct('0x1234') == 0x1234) ? "ok" : "not ok", " 2\n";
print +(hex('01234') == 0x1234) ? "ok" : "not ok", " 3\n";
print +(oct('20000000000') == 020000000000) ? "ok" : "not ok", " 4\n";
print +(oct('x80000000') == 0x80000000) ? "ok" : "not ok", " 5\n";
print +(hex('80000000') == 0x80000000) ? "ok" : "not ok", " 6\n";
print +(oct('1234') == 668) ? "ok" : "not ok", " 7\n";
print +(hex('1234') == 4660) ? "ok" : "not ok", " 8\n";
