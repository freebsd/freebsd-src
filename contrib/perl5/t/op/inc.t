#!./perl


# $RCSfile$

print "1..6\n";

# Verify that addition/subtraction properly upgrade to doubles.
# These tests are only significant on machines with 32 bit longs,
# and two's complement negation, but shouldn't fail anywhere.

$a = 2147483647;
$c=$a++;
if ($a == 2147483648) 
	{print "ok 1\n"}
else
	{print "not ok 1\n";}

$a = 2147483647;
$c=++$a;
if ($a == 2147483648) 
	{print "ok 2\n"}
else
	{print "not ok 2\n";}

$a = 2147483647;
$a=$a+1;
if ($a == 2147483648) 
	{print "ok 3\n"}
else
	{print "not ok 3\n";}

$a = -2147483648;
$c=$a--;
if ($a == -2147483649) 
	{print "ok 4\n"}
else
	{print "not ok 4\n";}

$a = -2147483648;
$c=--$a;
if ($a == -2147483649) 
	{print "ok 5\n"}
else
	{print "not ok 5\n";}

$a = -2147483648;
$a=$a-1;
if ($a == -2147483649) 
	{print "ok 6\n"}
else
	{print "not ok 6\n";}
