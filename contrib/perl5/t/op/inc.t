#!./perl

print "1..12\n";

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

$a = 2147483648;
$a = -$a;
$c=$a--;
if ($a == -2147483649) 
	{print "ok 7\n"}
else
	{print "not ok 7\n";}

$a = 2147483648;
$a = -$a;
$c=--$a;
if ($a == -2147483649) 
	{print "ok 8\n"}
else
	{print "not ok 8\n";}

$a = 2147483648;
$a = -$a;
$a=$a-1;
if ($a == -2147483649) 
	{print "ok 9\n"}
else
	{print "not ok 9\n";}

$a = 2147483648;
$b = -$a;
$c=$b--;
if ($b == -$a-1) 
	{print "ok 10\n"}
else
	{print "not ok 10\n";}

$a = 2147483648;
$b = -$a;
$c=--$b;
if ($b == -$a-1) 
	{print "ok 11\n"}
else
	{print "not ok 11\n";}

$a = 2147483648;
$b = -$a;
$b=$b-1;
if ($b == -(++$a)) 
	{print "ok 12\n"}
else
	{print "not ok 12\n";}
