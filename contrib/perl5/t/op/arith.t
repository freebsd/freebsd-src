#!./perl

print "1..8\n";

sub try ($$) {
   print +($_[1] ? "ok" : "not ok"), " $_[0]\n";
}

try 1,  13 %  4 ==  1;
try 2, -13 %  4 ==  3;
try 3,  13 % -4 == -3;
try 4, -13 % -4 == -1;

my $limit = 1e6;

# Division (and modulo) of floating point numbers
# seem to be rather sloppy in Cray.
$limit = 1e8 if $^O eq 'unicos';

try 5, abs( 13e21 %  4e21 -  1e21) < $limit;
try 6, abs(-13e21 %  4e21 -  3e21) < $limit;
try 7, abs( 13e21 % -4e21 - -3e21) < $limit;
try 8, abs(-13e21 % -4e21 - -1e21) < $limit;
