#!./perl

print "1..9\n";

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use File::Path;
rmtree('blurfl');

# tests 3 and 7 rather naughtily expect English error messages
$ENV{'LC_ALL'} = 'C';
$ENV{LANGUAGE} = 'C'; # GNU locale extension

print (mkdir('blurfl',0777) ? "ok 1\n" : "not ok 1\n");
print (mkdir('blurfl',0777) ? "not ok 2\n" : "ok 2\n");
print ($! =~ /cannot move|exist|denied/ ? "ok 3\n" : "# $!\nnot ok 3\n");
print (-d 'blurfl' ? "ok 4\n" : "not ok 4\n");
print (rmdir('blurfl') ? "ok 5\n" : "not ok 5\n");
print (rmdir('blurfl') ? "not ok 6\n" : "ok 6\n");
print ($! =~ /cannot find|such|exist|not found/i ? "ok 7\n" : "# $!\nnot ok 7\n");
print (mkdir('blurfl') ? "ok 8\n" : "not ok 8\n");
print (rmdir('blurfl') ? "ok 9\n" : "not ok 9\n");
