#!./perl

# $RCSfile: stat.t,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:30:01 $

print "1..56\n";

chop($cwd = `pwd`);

$DEV = `ls -l /dev`;

unlink "Op.stat.tmp";
open(FOO, ">Op.stat.tmp");

$junk = `ls Op.stat.tmp`;	# hack to make Apollo update link count

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat(FOO);
if ($nlink == 1) {print "ok 1\n";} else {print "not ok 1\n";}
if ($mtime && $mtime == $ctime) {print "ok 2\n";} else {print "not ok 2\n";}

print FOO "Now is the time for all good men to come to.\n";
close(FOO);

sleep 2;

`rm -f Op.stat.tmp2; ln Op.stat.tmp Op.stat.tmp2; chmod 644 Op.stat.tmp`;

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('Op.stat.tmp');

if ($nlink == 2) {print "ok 3\n";} else {print "not ok 3\n";}
if (($mtime && $mtime != $ctime) || $cwd =~ m#/afs/#) {
    print "ok 4\n";
}
else {
    print "not ok 4\n";
}
print "#4	:$mtime: != :$ctime:\n";

`rm -f Op.stat.tmp`;
`touch Op.stat.tmp`;

if (-z 'Op.stat.tmp') {print "ok 5\n";} else {print "not ok 5\n";}
if (! -s 'Op.stat.tmp') {print "ok 6\n";} else {print "not ok 6\n";}

`echo hi >Op.stat.tmp`;
if (! -z 'Op.stat.tmp') {print "ok 7\n";} else {print "not ok 7\n";}
if (-s 'Op.stat.tmp') {print "ok 8\n";} else {print "not ok 8\n";}

unlink 'Op.stat.tmp';
$olduid = $>;		# can't test -r if uid == 0
`echo hi >Op.stat.tmp`;
chmod 0,'Op.stat.tmp';
eval '$> = 1;';		# so switch uid (may not be implemented)
if (!$> || ! -r 'Op.stat.tmp') {print "ok 9\n";} else {print "not ok 9\n";}
if (!$> || ! -w 'Op.stat.tmp') {print "ok 10\n";} else {print "not ok 10\n";}
eval '$> = $olduid;';		# switch uid back (may not be implemented)
print "# olduid=$olduid, newuid=$>\n" unless ($> == $olduid);
if (! -x 'Op.stat.tmp') {print "ok 11\n";} else {print "not ok 11\n";}

foreach ((12,13,14,15,16,17)) {
    print "ok $_\n";		#deleted tests
}

chmod 0700,'Op.stat.tmp';
if (-r 'Op.stat.tmp') {print "ok 18\n";} else {print "not ok 18\n";}
if (-w 'Op.stat.tmp') {print "ok 19\n";} else {print "not ok 19\n";}
if (-x 'Op.stat.tmp') {print "ok 20\n";} else {print "not ok 20\n";}

if (-f 'Op.stat.tmp') {print "ok 21\n";} else {print "not ok 21\n";}
if (! -d 'Op.stat.tmp') {print "ok 22\n";} else {print "not ok 22\n";}

if (-d '.') {print "ok 23\n";} else {print "not ok 23\n";}
if (! -f '.') {print "ok 24\n";} else {print "not ok 24\n";}

if (`ls -l perl` =~ /^l.*->/) {
    if (-l 'perl') {print "ok 25\n";} else {print "not ok 25\n";}
}
else {
    print "ok 25\n";
}

if (-o 'Op.stat.tmp') {print "ok 26\n";} else {print "not ok 26\n";}

if (-e 'Op.stat.tmp') {print "ok 27\n";} else {print "not ok 27\n";}
`rm -f Op.stat.tmp Op.stat.tmp2`;
if (! -e 'Op.stat.tmp') {print "ok 28\n";} else {print "not ok 28\n";}

if ($DEV !~ /\nc.* (\S+)\n/)
    {print "ok 29\n";}
elsif (-c "/dev/$1")
    {print "ok 29\n";}
else
    {print "not ok 29\n";}
if (! -c '.') {print "ok 30\n";} else {print "not ok 30\n";}

if ($DEV !~ /\ns.* (\S+)\n/)
    {print "ok 31\n";}
elsif (-S "/dev/$1")
    {print "ok 31\n";}
else
    {print "not ok 31\n";}
if (! -S '.') {print "ok 32\n";} else {print "not ok 32\n";}

if ($DEV !~ /\nb.* (\S+)\n/)
    {print "ok 33\n";}
elsif (-b "/dev/$1")
    {print "ok 33\n";}
else
    {print "not ok 33\n";}
if (! -b '.') {print "ok 34\n";} else {print "not ok 34\n";}

$cnt = $uid = 0;

die "Can't run op/stat.t test 35 without pwd working" unless $cwd;
chdir '/usr/bin' || die "Can't cd to /usr/bin";
while (defined($_ = <*>)) {
    $cnt++;
    $uid++ if -u;
    last if $uid && $uid < $cnt;
}
chdir $cwd || die "Can't cd back to $cwd";

# I suppose this is going to fail somewhere...
if ($uid > 0 && $uid < $cnt) {print "ok 35\n";} else {print "not ok 35\n";}

unless (open(tty,"/dev/tty")) {
    print STDERR "Can't open /dev/tty--run t/TEST outside of make.\n";
}
if (-t tty) {print "ok 36\n";} else {print "not ok 36\n";}
if (-c tty) {print "ok 37\n";} else {print "not ok 37\n";}
close(tty);
if (! -t tty) {print "ok 38\n";} else {print "not ok 38\n";}
open(null,"/dev/null");
if (! -t null || -e '/xenix') {print "ok 39\n";} else {print "not ok 39\n";}
close(null);
if (-t) {print "ok 40\n";} else {print "not ok 40\n";}

# These aren't strictly "stat" calls, but so what?

if (-T 'op/stat.t') {print "ok 41\n";} else {print "not ok 41\n";}
if (! -B 'op/stat.t') {print "ok 42\n";} else {print "not ok 42\n";}

if (-B './perl') {print "ok 43\n";} else {print "not ok 43\n";}
if (! -T './perl') {print "ok 44\n";} else {print "not ok 44\n";}

open(FOO,'op/stat.t');
eval { -T FOO; };
if ($@ =~ /not implemented/) {
    print "# $@";
    for (45 .. 54) {
	print "ok $_\n";
    }
}
else {
    if (-T FOO) {print "ok 45\n";} else {print "not ok 45\n";}
    if (! -B FOO) {print "ok 46\n";} else {print "not ok 46\n";}
    $_ = <FOO>;
    if (/perl/) {print "ok 47\n";} else {print "not ok 47\n";}
    if (-T FOO) {print "ok 48\n";} else {print "not ok 48\n";}
    if (! -B FOO) {print "ok 49\n";} else {print "not ok 49\n";}
    close(FOO);

    open(FOO,'op/stat.t');
    $_ = <FOO>;
    if (/perl/) {print "ok 50\n";} else {print "not ok 50\n";}
    if (-T FOO) {print "ok 51\n";} else {print "not ok 51\n";}
    if (! -B FOO) {print "ok 52\n";} else {print "not ok 52\n";}
    seek(FOO,0,0);
    if (-T FOO) {print "ok 53\n";} else {print "not ok 53\n";}
    if (! -B FOO) {print "ok 54\n";} else {print "not ok 54\n";}
}
close(FOO);

if (-T '/dev/null') {print "ok 55\n";} else {print "not ok 55\n";}
if (-B '/dev/null') {print "ok 56\n";} else {print "not ok 56\n";}
