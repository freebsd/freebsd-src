#!./perl

print "1..4\n";

sub try ($$) {
   print +($_[1] ? "ok" : "not ok"), " $_[0]\n";
}

try 1,  13 %  4 ==  1;
try 2, -13 %  4 ==  3;
try 3,  13 % -4 == -3;
try 4, -13 % -4 == -1;
