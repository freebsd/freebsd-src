# lint.awk --- test lint variable

BEGIN {
	a[1] = 1
	LINT = 1
	delete a[2]
	LINT = ""
	delete a[3]
	LINT = "true"
	delete a[4]
	LINT = 0
	delete a[5]
	print "done"
}
