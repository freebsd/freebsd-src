#!./perl

print "1..33\n";

# because of ebcdic.c these should be the same on asciiish 
# and ebcdic machines.
# Peter Prymmer <pvhp@best.com>.

my $c = "\c@";
print +((ord($c) == 0) ? "" : "not "),"ok 1\n";
$c = "\cA";
print +((ord($c) == 1) ? "" : "not "),"ok 2\n";
$c = "\cB";
print +((ord($c) == 2) ? "" : "not "),"ok 3\n";
$c = "\cC";
print +((ord($c) == 3) ? "" : "not "),"ok 4\n";
$c = "\cD";
print +((ord($c) == 4) ? "" : "not "),"ok 5\n";
$c = "\cE";
print +((ord($c) == 5) ? "" : "not "),"ok 6\n";
$c = "\cF";
print +((ord($c) == 6) ? "" : "not "),"ok 7\n";
$c = "\cG";
print +((ord($c) == 7) ? "" : "not "),"ok 8\n";
$c = "\cH";
print +((ord($c) == 8) ? "" : "not "),"ok 9\n";
$c = "\cI";
print +((ord($c) == 9) ? "" : "not "),"ok 10\n";
$c = "\cJ";
print +((ord($c) == 10) ? "" : "not "),"ok 11\n";
$c = "\cK";
print +((ord($c) == 11) ? "" : "not "),"ok 12\n";
$c = "\cL";
print +((ord($c) == 12) ? "" : "not "),"ok 13\n";
$c = "\cM";
print +((ord($c) == 13) ? "" : "not "),"ok 14\n";
$c = "\cN";
print +((ord($c) == 14) ? "" : "not "),"ok 15\n";
$c = "\cO";
print +((ord($c) == 15) ? "" : "not "),"ok 16\n";
$c = "\cP";
print +((ord($c) == 16) ? "" : "not "),"ok 17\n";
$c = "\cQ";
print +((ord($c) == 17) ? "" : "not "),"ok 18\n";
$c = "\cR";
print +((ord($c) == 18) ? "" : "not "),"ok 19\n";
$c = "\cS";
print +((ord($c) == 19) ? "" : "not "),"ok 20\n";
$c = "\cT";
print +((ord($c) == 20) ? "" : "not "),"ok 21\n";
$c = "\cU";
print +((ord($c) == 21) ? "" : "not "),"ok 22\n";
$c = "\cV";
print +((ord($c) == 22) ? "" : "not "),"ok 23\n";
$c = "\cW";
print +((ord($c) == 23) ? "" : "not "),"ok 24\n";
$c = "\cX";
print +((ord($c) == 24) ? "" : "not "),"ok 25\n";
$c = "\cY";
print +((ord($c) == 25) ? "" : "not "),"ok 26\n";
$c = "\cZ";
print +((ord($c) == 26) ? "" : "not "),"ok 27\n";
$c = "\c[";
print +((ord($c) == 27) ? "" : "not "),"ok 28\n";
$c = "\c\\";
print +((ord($c) == 28) ? "" : "not "),"ok 29\n";
$c = "\c]";
print +((ord($c) == 29) ? "" : "not "),"ok 30\n";
$c = "\c^";
print +((ord($c) == 30) ? "" : "not "),"ok 31\n";
$c = "\c_";
print +((ord($c) == 31) ? "" : "not "),"ok 32\n";
$c = "\c?";
print +((ord($c) == 127) ? "" : "not "),"ok 33\n";
