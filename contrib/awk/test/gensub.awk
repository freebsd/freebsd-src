BEGIN { a = "this is a test of gawk"
	b = gensub(/(this).*(test).*(gawk)/, "3 = <\\3>, 2 = <\\2>, 1 = <\\1>", 1, a)
	print b
}
NR == 1 { print gensub(/b/, "BB", 2) }
NR == 2 { print gensub(/c/, "CC", "global") }
END { print gensub(/foo/, "bar", 1, "DON'T PANIC") }
