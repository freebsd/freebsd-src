function tst () {
	sum += 1
	return sum
}

BEGIN { OFMT = "%.0f" ; print tst() }
