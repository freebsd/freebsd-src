# Tue May 25 16:36:16 IDT 1999
#
# Test cases based on email from Andreas Schwab, schwab@gnu.org

BEGIN {
	fmt[1] = "%8.5d";	data[1] = 100
	fmt[2] = "%#o";		data[2] = 0
	fmt[3] = "%#.1o";	data[3] = 0
	fmt[4] = "%#.0o";	data[4] = 0
	fmt[5] = "%#x";		data[5] = 0
	fmt[6] = "%.0d";	data[6] = 0
	fmt[7] = "%5.0d";	data[7] = 0

	for (i = 1; i <= 7; i++) {
		format = "%s, %d --- |" fmt[i] "|\n"
		printf(format, fmt[i], data[i], data[i])
	}

}
