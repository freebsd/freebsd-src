# Test program for checking sprintf operation with various floating
# point formats
#
# Watch out - full output of this program will have 3000 * tot lines,
# which will take a chunk of space if you will write it to your disk.
# --mj

BEGIN {
    just = "-"
    plus = "+ "
    alt  = "#"
    zero = "0"
    spec = "feEgG"
    fw[1] = ""
    fw[2] = "1"
    fw[3] = "5"
    fw[4] = "10"
    fw[5] = "15"
    prec[1] = ".-1"
    prec[2] = ""
    prec[3] = ".2"
    prec[4] = ".5"
    prec[5] = ".10"

    num = 123.6
    factor = 1.0e-12
    tot = 8
    data[1] = 0
    data[2] = 1
    for (i = 3; i <= tot; i++) {
	data[i] = num * factor
	factor *= 1000
    }

    for (j = 1; j <= 2; j++) {
	for (p = 1; p <= 3; p++) {
	    for (a = 1; a <= 2; a++) {
		for (z = 1; z <= 2; z++) {
		    for (s = 1; s <= 5; s++) {
			for (w = 1; w <= 5; w++) {
			    for (r = 1; r <= 5; r++) {
				frmt = "|%" substr(just, j, 1)
				frmt = frmt substr(plus, p, 1)
				frmt = frmt substr(alt,  a, 1)
				frmt = frmt substr(zero, z, 1)
				frmt = frmt fw[w] prec[r]
				frmt = frmt substr(spec, s, 1) "|"
				for (i = 1; i <= tot; i++) {
				    result = sprintf(frmt, data[i])
#				    "normalize" if you must
#				    sub(/\|\./, "|0.", result)
				    printf("%-16s %-25s\t%g\n", frmt,
						 result,data[i])
				}
			    }
			}
		    }
		}
	    }
	}
    }
}
