BEGIN {
	str = "abc"; print gsub("b+", "FOO", str), str
	str = "abc"; print gsub("x*", "X", str), str
	str = "abc"; print gsub("b*", "X", str), str
	str = "abc"; print gsub("c", "X", str), str
	str = "abc"; print gsub("c+", "X", str), str
	str = "abc"; print gsub("x*$", "X", str), str
}
