BEGIN {
	data = "abc:easy:as:one:two:three"
	FS = ":"
	FIELDWIDTHS = "3 1 4 1 2 1 3 1 3 1 5"
	n = split(data, a)
	printf "n = %d, a[3] = %s\n", n, a[3]
}
