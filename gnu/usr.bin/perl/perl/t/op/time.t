#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/time.t,v 1.1.1.1 1994/09/10 06:27:46 gclarkii Exp $

print "1..5\n";

($beguser,$begsys) = times;

$beg = time;

while (($now = time) == $beg) {}

if ($now > $beg && $now - $beg < 10){print "ok 1\n";} else {print "not ok 1\n";}

for ($i = 0; $i < 100000; $i++) {
    ($nowuser, $nowsys) = times;
    $i = 200000 if $nowuser > $beguser && $nowsys > $begsys;
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

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime($beg);
($xsec,$foo) = localtime($now);

if ($sec != $xsec && $mday && $year)
    {print "ok 4\n";}
else
    {print "not ok 4\n";}

if (index(" :0:1:-1:365:366:-365:-366:",':' . ($localyday - $yday) . ':') > 0)
    {print "ok 5\n";}
else
    {print "not ok 5\n";}
