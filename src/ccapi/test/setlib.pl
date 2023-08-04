#!perl -w

$b = "lib\\win\\srctmp";
$a = $ENV{LIB};
if (! ($a =~ /$b/) ) {
    print "$b Not in LIB!\n";
    system("del a.tmp");
    }
else {print "$b in LIB.\n";}
exit(0);