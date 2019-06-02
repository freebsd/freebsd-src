BEGIN {
	OFMT = ">>%.6g<<"
        a = 12.1234
	print "a =", a
        b = a ""
        print "1 ->", b
        CONVFMT = "%2.2f"
        b = a ""
        print "2 ->", b
        CONVFMT = "%.12g"
        b = a ""
        print "3 ->", b
}
