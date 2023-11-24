BEGIN {
	$0 = "a b c";
	OFS = 1;
	ORS = 2;
	NF = 2;
	print;
	print "d", "e";
}
