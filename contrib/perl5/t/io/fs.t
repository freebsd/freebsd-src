#!./perl

# $RCSfile: fs.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:28 $

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Config;

$Is_Dosish = ($^O eq 'MSWin32' or $^O eq 'dos' or
	      $^O eq 'os2' or $^O eq 'mint');

print "1..28\n";

$wd = (($^O eq 'MSWin32') ? `cd` : `pwd`);
chop($wd);

if ($^O eq 'MSWin32') { `del tmp 2>nul`; `mkdir tmp`; }
else {  `rm -f tmp 2>/dev/null; mkdir tmp 2>/dev/null`; }
chdir './tmp';
`/bin/rm -rf a b c x` if -x '/bin/rm';

umask(022);

if ($^O eq 'MSWin32') { print "ok 1 # skipped: bogus umask()\n"; }
elsif ((umask(0)&0777) == 022) {print "ok 1\n";} else {print "not ok 1\n";}
open(fh,'>x') || die "Can't create x";
close(fh);
open(fh,'>a') || die "Can't create a";
close(fh);

if ($Is_Dosish) {print "ok 2 # skipped: no link\n";} 
elsif (eval {link('a','b')}) {print "ok 2\n";} 
else {print "not ok 2\n";}

if ($Is_Dosish) {print "ok 3 # skipped: no link\n";} 
elsif (eval {link('b','c')}) {print "ok 3\n";} 
else {print "not ok 3\n";}

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('c');

if ($Config{dont_use_nlink} || $Is_Dosish)
    {print "ok 4 # skipped: no link\n";} 
elsif ($nlink == 3)
    {print "ok 4\n";} 
else {print "not ok 4\n";}

if ($^O eq 'amigaos' || $Is_Dosish)
    {print "ok 5 # skipped: no link\n";} 
elsif (($mode & 0777) == 0666)
    {print "ok 5\n";} 
else {print "not ok 5\n";}

if ((chmod 0777,'a') == 1) {print "ok 6\n";} else {print "not ok 6\n";}

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('c');
if ($Is_Dosish) {print "ok 7 # skipped: no link\n";} 
elsif (($mode & 0777) == 0777) {print "ok 7\n";} 
else {print "not ok 7\n";}

if ($Is_Dosish) {print "ok 8 # skipped: no link\n";} 
elsif ((chmod 0700,'c','x') == 2) {print "ok 8\n";} 
else {print "not ok 8\n";}

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('c');
if ($Is_Dosish) {print "ok 9 # skipped: no link\n";} 
elsif (($mode & 0777) == 0700) {print "ok 9\n";} 
else {print "not ok 9\n";}

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('x');
if ($Is_Dosish) {print "ok 10 # skipped: no link\n";} 
elsif (($mode & 0777) == 0700) {print "ok 10\n";} 
else {print "not ok 10\n";}

if ($Is_Dosish) {print "ok 11 # skipped: no link\n"; unlink 'b','x'; } 
elsif ((unlink 'b','x') == 2) {print "ok 11\n";} 
else {print "not ok 11\n";}
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
$delta = $Is_Dosish ? 2 : 1;	# Granularity of time on the filesystem
$foo = (utime 500000000,500000000 + $delta,'b');
if ($foo == 1) {print "ok 16\n";} else {print "not ok 16 $foo\n";}
($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('b');
if ($^O eq 'MSWin32') { print "ok 17 # skipped: bogus (stat)[1]\n"; }
elsif ($ino) {print "ok 17\n";} else {print "not ok 17\n";}
if ($wd =~ m#/afs/# || $^O eq 'amigaos' || $^O eq 'dos' || $^O eq 'MSWin32')
    {print "ok 18 # skipped: granularity of the filetime\n";}
elsif ($atime == 500000000 && $mtime == 500000000 + $delta)
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
if ($^O ne 'MSWin32' and `ls -l perl 2>/dev/null` =~ /^l.*->/) {
    # we have symbolic links
    if (symlink("TEST","c")) {print "ok 21\n";} else {print "not ok 21\n";}
    $foo = `grep perl c`;
    if ($foo) {print "ok 22\n";} else {print "not ok 22\n";}
    unlink 'c';
}
else {
    print "ok 21\nok 22\n";
}

# truncate (may not be implemented everywhere)
unlink "Iofs.tmp";
`echo helloworld > Iofs.tmp`;
eval { truncate "Iofs.tmp", 5; };
if ($@ =~ /not implemented/) {
  print "# truncate not implemented -- skipping tests 23 through 26\n";
  for (23 .. 26) {
    print "ok $_\n";
  }
}
else {
  if (-s "Iofs.tmp" == 5) {print "ok 23\n"} else {print "not ok 23\n"}
  truncate "Iofs.tmp", 0;
  if (-z "Iofs.tmp") {print "ok 24\n"} else {print "not ok 24\n"}
  open(FH, ">Iofs.tmp") or die "Can't create Iofs.tmp";
  { select FH; $| = 1; select STDOUT }
  print FH "helloworld\n";
  truncate FH, 5;
  if ($^O eq 'dos') {
      close (FH); open (FH, ">>Iofs.tmp") or die "Can't reopen Iofs.tmp";
  }
  if (-s "Iofs.tmp" == 5) {print "ok 25\n"} else {print "not ok 25\n"}
  truncate FH, 0;
  if ($^O eq 'dos') {
      close (FH); open (FH, ">>Iofs.tmp") or die "Can't reopen Iofs.tmp";
  }
  if (-z "Iofs.tmp") {print "ok 26\n"} else {print "not ok 26\n"}
  close FH;
}

# check if rename() works on directories
rename 'tmp', 'tmp1' or print "not ";
print "ok 27\n";
-d 'tmp1' or print "not ";
print "ok 28\n";

END { rmdir 'tmp1'; unlink "Iofs.tmp"; }
