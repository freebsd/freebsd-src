#!./perl

# $RCSfile$

print "1..1\n";

open(A,"+>a");
print A "_";
seek(A,0,0);

$b = "abcd"; 
$b = "";

read(A,$b,1,4);

close(A);

unlink("a");

if ($b eq "\000\000\000\000_") {
	print "ok 1\n";
} else { # Probably "\000bcd_"
	print "not ok 1\n";
}

unlink 'a';
