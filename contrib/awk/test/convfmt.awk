BEGIN {
	CONVFMT = "%2.2f"
	a = 123.456
	b = a ""                # give `a' string value also
	printf "a = %s\n", a
	CONVFMT = "%.6g"
	printf "a = %s\n", a
	a += 0                  # make `a' numeric only again
	printf "a = %s\n", a    # use `a' as string
}
