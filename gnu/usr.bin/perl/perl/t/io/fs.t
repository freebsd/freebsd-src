#!./perl

# $RCSfile: fs.t,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:30:05 $

print "1..22\n";

$wd = `pwd`;
chop($wd);

`rm -f tmp 2>/dev/null; mkdir tmp 2>/dev/null`;
chdir './tmp';
`/bin/rm -rf a b c x`;

umask(022);

if ((umask(0)&0777) == 022) {print "ok 1\n";} else {print "not ok 1\n";}
open(fh,'>x') || die "Can't create x";
close(fh);
open(fh,'>a') || die "Can't create a";
close(fh);

if (link('a','b')) {print "ok 2\n";} else {print "not ok 2\n";}

if (link('b','c')) {print "ok 3\n";} else {print "not ok 3\n";}

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('c');

if ($nlink == 3) {print "ok 4\n";} else {print "not ok 4\n";}
if (($mode & 0777) == 0666) {print "ok 5\n";} else {print "not ok 5\n";}

if ((chmod 0777,'a') == 1) {print "ok 6\n";} else {print "not ok 6\n";}

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('c');
if (($mode & 0777) == 0777) {print "ok 7\n";} else {print "not ok 7\n";}

if ((chmod 0700,'c','x') == 2) {print "ok 8\n";} else {print "not ok 8\n";}

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('c');
if (($mode & 0777) == 0700) {print "ok 9\n";} else {print "not ok 9\n";}
($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('x');
if (($mode & 0777) == 0700) {print "ok 10\n";} else {print "not ok 10\n";}

if ((unlink 'b','x') == 2) {print "ok 11\n";} else {print "not ok 11\n";}
($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('b');
if ($ino == 0) {print "ok 12\n";} else {print "not ok 12\n";}
($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('x');
if ($ino == 0) {print "ok 13\n";} else {print "not ok 13\n";}

if (rename('a','b')) {print "ok 14\n";} else {print "not ok 14\n";}
($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('a');
if ($ino == 0) {print "ok 15\n";} else {print "not ok 15\n";}
$foo = (utime 500000000,500000001,'b');
if ($foo == 1) {print "ok 16\n";} else {print "not ok 16 $foo\n";}
($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('b');
if ($ino) {print "ok 17\n";} else {print "not ok 17\n";}
if (($atime == 500000000 && $mtime == 500000001) || $wd =~ m#/afs/#)
    {print "ok 18\n";}
else
    {print "not ok 18 $atime $mtime\n";}

if ((unlink 'b') == 1) {print "ok 19\n";} else {print "not ok 19\n";}
($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('b');
if ($ino == 0) {print "ok 20\n";} else {print "not ok 20\n";}
unlink 'c';

chdir $wd || die "Can't cd back to $wd";

unlink 'c';
if (`ls -l perl 2>/dev/null` =~ /^l.*->/) {  # we have symbolic links
    if (symlink("TEST","c")) {print "ok 21\n";} else {print "not ok 21\n";}
    $foo = `grep perl c`;
    if ($foo) {print "ok 22\n";} else {print "not ok 22\n";}
}
else {
    print "ok 21\nok 22\n";
}
