BEGIN {
	str = "a   b\t\tc d"
	n = split(str, a, " ")
	print n
	m = split(str, b, / /)
	print m
}
