#!./perl

# $RCSfile: time.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:32 $

if ($does_gmtime = gmtime(time)) { print "1..6\n" }
else { print "1..3\n" }

($beguser,$begsys) = times;

$beg = time;

while (($now = time) == $beg) { sleep 1 }

if ($now > $beg && $now - $beg < 10){print "ok 1\n";} else {print "not ok 1\n";}

for ($i = 0; $i < 100000; $i++) {
    ($nowuser, $nowsys) = times;
    $i = 200000 if $nowuser > $beguser && ( $nowsys > $begsys || 
                                            (!$nowsys && !$begsys));
    last if time - $beg > 20;
}

if ($i >= 200000) {print "ok 2\n";} else {print "not ok 2\n";}

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($beg);
($xsec,$foo) = localtime($now);
$localyday = $yday;

if ($sec != $xsec && $mday && $year)
    {print "ok 3\n";}
else
    {print "not ok 3\n";}

exit 0 unless $does_gmtime;

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime($beg);
($xsec,$foo) = localtime($now);

if ($sec != $xsec && $mday && $year)
    {print "ok 4\n";}
else
    {print "not ok 4\n";}

if (index(" :0:1:-1:364:365:-364:-365:",':' . ($localyday - $yday) . ':') > 0)
    {print "ok 5\n";}
else
    {print "not ok 5\n";}

# This could be stricter.
if (gmtime() =~ /^(Sun|Mon|Tue|Wed|Thu|Fri|Sat) (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) ([ \d]\d) (\d\d):(\d\d):(\d\d) (\d\d\d\d)$/)
    {print "ok 6\n";}
else
    {print "not ok 6\n";}
