BEGIN {
	x = "A"
	printf("%-39s\n", substr(x,1,39))
	print substr("abcdef", 0, 2)
	print substr("abcdef", 2.3, 2)
	print substr("abcdef", -1, 2)
	print substr("abcdef", 1, 0)
	print substr("abcdef", 1, -3)
	print substr("abcdef", 1, 2.3)
	print substr("", 1, 2)
	print substr("abcdef", 5, 5)
	print substr("abcdef", 7, 2)
	exit (0)
}
