#!./perl

# $RCSfile: tell.t,v $$Revision$$Date$

print "1..23\n";

$TST = 'tst';

$Is_Dosish = ($^O eq 'MSWin32' or $^O eq 'dos' or
	      $^O eq 'os2' or $^O eq 'mint' or $^O eq 'cygwin');

open($TST, 'harness') || (die "Can't open harness");
binmode $TST if $Is_Dosish;
if (eof(tst)) { print "not ok 1\n"; } else { print "ok 1\n"; }

$firstline = <$TST>;
$secondpos = tell;

$x = 0;
while (<tst>) {
    if (eof) {$x++;}
}
if ($x == 1) { print "ok 2\n"; } else { print "not ok 2\n"; }

$lastpos = tell;

unless (eof) { print "not ok 3\n"; } else { print "ok 3\n"; }

if (seek($TST,0,0)) { print "ok 4\n"; } else { print "not ok 4\n"; }

if (eof) { print "not ok 5\n"; } else { print "ok 5\n"; }

if ($firstline eq <tst>) { print "ok 6\n"; } else { print "not ok 6\n"; }

if ($secondpos == tell) { print "ok 7\n"; } else { print "not ok 7\n"; }

if (seek(tst,0,1)) { print "ok 8\n"; } else { print "not ok 8\n"; }

if (eof($TST)) { print "not ok 9\n"; } else { print "ok 9\n"; }

if ($secondpos == tell) { print "ok 10\n"; } else { print "not ok 10\n"; }

if (seek(tst,0,2)) { print "ok 11\n"; } else { print "not ok 11\n"; }

if ($lastpos == tell) { print "ok 12\n"; } else { print "not ok 12\n"; }

unless (eof) { print "not ok 13\n"; } else { print "ok 13\n"; }

if ($. == 0) { print "not ok 14\n"; } else { print "ok 14\n"; }

$curline = $.;
open(other, 'harness') || (die "Can't open harness: $!");
binmode other if $^O eq 'MSWin32';

{
    local($.);

    if ($. == 0) { print "not ok 15\n"; } else { print "ok 15\n"; }

    tell other;
    if ($. == 0) { print "ok 16\n"; } else { print "not ok 16\n"; }

    $. = 5;
    scalar <other>;
    if ($. == 6) { print "ok 17\n"; } else { print "not ok 17\n"; }
}

if ($. == $curline) { print "ok 18\n"; } else { print "not ok 18\n"; }

{
    local($.);

    scalar <other>;
    if ($. == 7) { print "ok 19\n"; } else { print "not ok 19\n"; }
}

if ($. == $curline) { print "ok 20\n"; } else { print "not ok 20\n"; }

{
    local($.);

    tell other;
    if ($. == 7) { print "ok 21\n"; } else { print "not ok 21\n"; }
}

close(other);
if (tell(other) == -1)  { print "ok 22\n"; } else { print "not ok 22\n"; }

if (tell(ether) == -1)  { print "ok 23\n"; } else { print "not ok 23\n"; }

# ftell(STDIN) (or any std streams) is undefined, it can return -1 or
# something else.  ftell() on pipes, fifos, and sockets is defined to
# return -1.

