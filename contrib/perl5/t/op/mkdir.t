#!./perl

# $RCSfile: mkdir.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:06 $

print "1..7\n";

$^O eq 'MSWin32' ? `del /s /q blurfl 2>&1` : `rm -rf blurfl`;

# tests 3 and 7 rather naughtily expect English error messages
$ENV{'LC_ALL'} = 'C';

print (mkdir('blurfl',0777) ? "ok 1\n" : "not ok 1\n");
print (mkdir('blurfl',0777) ? "not ok 2\n" : "ok 2\n");
print ($! =~ /exist|denied/ ? "ok 3\n" : "# $!\nnot ok 3\n");
print (-d 'blurfl' ? "ok 4\n" : "not ok 4\n");
print (rmdir('blurfl') ? "ok 5\n" : "not ok 5\n");
print (rmdir('blurfl') ? "not ok 6\n" : "ok 6\n");
print ($! =~ /such|exist|not found/i ? "ok 7\n" : "not ok 7\n");
