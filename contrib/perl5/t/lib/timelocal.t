#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Time::Local;

# Set up time values to test
@time =
  (
   #year,mon,day,hour,min,sec 
   [1970,  1,  2, 00, 00, 00],
   [1980,  2, 28, 12, 00, 00],
   [1980,  2, 29, 12, 00, 00],
   [1999, 12, 31, 23, 59, 59],
   [2000,  1,  1, 00, 00, 00],
   [2010, 10, 12, 14, 13, 12],
  );

# use vmsish 'time' makes for oddness around the Unix epoch
if ($^O eq 'VMS') { $time[0][2]++ }

print "1..", @time * 2 + 5, "\n";

$count = 1;
for (@time) {
    my($year, $mon, $mday, $hour, $min, $sec) = @$_;
    $year -= 1900;
    $mon --;
    my $time = timelocal($sec,$min,$hour,$mday,$mon,$year);
    # print scalar(localtime($time)), "\n";
    my($s,$m,$h,$D,$M,$Y) = localtime($time);

    if ($s == $sec &&
	$m == $min &&
	$h == $hour &&
	$D == $mday &&
	$M == $mon &&
	$Y == $year
       ) {
	print "ok $count\n";
    } else {
	print "not ok $count\n";
    }
    $count++;

    # Test gmtime function
    $time = timegm($sec,$min,$hour,$mday,$mon,$year);
    ($s,$m,$h,$D,$M,$Y) = gmtime($time);

    if ($s == $sec &&
	$m == $min &&
	$h == $hour &&
	$D == $mday &&
	$M == $mon &&
	$Y == $year
       ) {
	print "ok $count\n";
    } else {
	print "not ok $count\n";
    }
    $count++;
}

#print "Testing that the differences between a few dates makes sence...\n";

timelocal(0,0,1,1,0,90) - timelocal(0,0,0,1,0,90) == 3600
  or print "not ";
print "ok ", $count++, "\n";

timelocal(1,2,3,1,0,100) - timelocal(1,2,3,31,11,99) == 24 * 3600 
  or print "not ";
print "ok ", $count++, "\n";

# Diff beween Jan 1, 1970 and Mar 1, 1970 = (31 + 28 = 59 days)
timegm(0,0,0, 1, 2, 70) - timegm(0,0,0, 1, 0, 70) == 59 * 24 * 3600
  or print "not ";
print "ok ", $count++, "\n";


#print "Testing timelocal.pl module too...\n";
package test;
require 'timelocal.pl';
timegm(0,0,0,1,0,70) == main::timegm(0,0,0,1,0,70) or print "not ";
print "ok ", $main::count++, "\n";

timelocal(1,2,3,4,5,78) == main::timelocal(1,2,3,4,5,78) or print "not ";
print "ok ", $main::count++, "\n";
