BEGIN {
	$0 = "a b c d e f"
	print NF
	OFS = ":"
	NF--
	print $0
	print NF
	NF++
	print $0
	print NF
}
