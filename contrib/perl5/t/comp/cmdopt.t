#!./perl

# $RCSfile: cmdopt.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:17 $

print "1..44\n";

# test the optimization of constants

if (1) { print "ok 1\n";} else { print "not ok 1\n";}
unless (0) { print "ok 2\n";} else { print "not ok 2\n";}

if (0) { print "not ok 3\n";} else { print "ok 3\n";}
unless (1) { print "not ok 4\n";} else { print "ok 4\n";}

unless (!1) { print "ok 5\n";} else { print "not ok 5\n";}
if (!0) { print "ok 6\n";} else { print "not ok 6\n";}

unless (!0) { print "not ok 7\n";} else { print "ok 7\n";}
if (!1) { print "not ok 8\n";} else { print "ok 8\n";}

$x = 1;
if (1 && $x) { print "ok 9\n";} else { print "not ok 9\n";}
if (0 && $x) { print "not ok 10\n";} else { print "ok 10\n";}
$x = '';
if (1 && $x) { print "not ok 11\n";} else { print "ok 11\n";}
if (0 && $x) { print "not ok 12\n";} else { print "ok 12\n";}

$x = 1;
if (1 || $x) { print "ok 13\n";} else { print "not ok 13\n";}
if (0 || $x) { print "ok 14\n";} else { print "not ok 14\n";}
$x = '';
if (1 || $x) { print "ok 15\n";} else { print "not ok 15\n";}
if (0 || $x) { print "not ok 16\n";} else { print "ok 16\n";}


# test the optimization of variables

$x = 1;
if ($x) { print "ok 17\n";} else { print "not ok 17\n";}
unless ($x) { print "not ok 18\n";} else { print "ok 18\n";}

$x = '';
if ($x) { print "not ok 19\n";} else { print "ok 19\n";}
unless ($x) { print "ok 20\n";} else { print "not ok 20\n";}

# test optimization of string operations

$a = 'a';
if ($a eq 'a') { print "ok 21\n";} else { print "not ok 21\n";}
if ($a ne 'a') { print "not ok 22\n";} else { print "ok 22\n";}

if ($a =~ /a/) { print "ok 23\n";} else { print "not ok 23\n";}
if ($a !~ /a/) { print "not ok 24\n";} else { print "ok 24\n";}
# test interaction of logicals and other operations

$a = 'a';
$x = 1;
if ($a eq 'a' and $x) { print "ok 25\n";} else { print "not ok 25\n";}
if ($a ne 'a' and $x) { print "not ok 26\n";} else { print "ok 26\n";}
$x = '';
if ($a eq 'a' and $x) { print "not ok 27\n";} else { print "ok 27\n";}
if ($a ne 'a' and $x) { print "not ok 28\n";} else { print "ok 28\n";}

$x = 1;
if ($a eq 'a' or $x) { print "ok 29\n";} else { print "not ok 29\n";}
if ($a ne 'a' or $x) { print "ok 30\n";} else { print "not ok 30\n";}
$x = '';
if ($a eq 'a' or $x) { print "ok 31\n";} else { print "not ok 31\n";}
if ($a ne 'a' or $x) { print "not ok 32\n";} else { print "ok 32\n";}

$x = 1;
if ($a =~ /a/ && $x) { print "ok 33\n";} else { print "not ok 33\n";}
if ($a !~ /a/ && $x) { print "not ok 34\n";} else { print "ok 34\n";}
$x = '';
if ($a =~ /a/ && $x) { print "not ok 35\n";} else { print "ok 35\n";}
if ($a !~ /a/ && $x) { print "not ok 36\n";} else { print "ok 36\n";}

$x = 1;
if ($a =~ /a/ || $x) { print "ok 37\n";} else { print "not ok 37\n";}
if ($a !~ /a/ || $x) { print "ok 38\n";} else { print "not ok 38\n";}
$x = '';
if ($a =~ /a/ || $x) { print "ok 39\n";} else { print "not ok 39\n";}
if ($a !~ /a/ || $x) { print "not ok 40\n";} else { print "ok 40\n";}

$x = 1;
if ($a eq 'a' xor $x) { print "not ok 41\n";} else { print "ok 41\n";}
if ($a ne 'a' xor $x) { print "ok 42\n";} else { print "not ok 42\n";}
$x = '';
if ($a eq 'a' xor $x) { print "ok 43\n";} else { print "not ok 43\n";}
if ($a ne 'a' xor $x) { print "not ok 44\n";} else { print "ok 44\n";}
