BEGIN {
	# str = "\342\200\257"
	str = "あ"
	n = gsub(//, "X", str)
	print n, str
}
