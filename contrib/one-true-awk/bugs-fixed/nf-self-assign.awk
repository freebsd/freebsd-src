BEGIN {
	$0="a b c";
	OFS=",";
	NF = NF;
	print;
}
