# check that values with leading digits get converted the
# right way, based on a note in comp.lang.awk.
#
# run with gawk -v x=2E -f leaddig.awk
BEGIN {
	print "x =", x, (x == 2), (x == 2E0), (x == 2E), (x == 2D)
}
