# fnparydl.awk --- check that deleting works with arrays
# 		   that are parameters.
#
# Tue Jul 11 14:20:58 EDT 2000

function delit(a,	k)
{
	print "BEFORE LOOP"
	for (k in a) {
		print "DELETING KEY", k
		delete a[k]
	}
	print "AFTER LOOP"
}

BEGIN {
	for (i = 1 ; i <= 7; i++) {
		q[i] = sprintf("element %d", i)
		x[i] = i
		y[i] = q[i]
	}
#	adump(q)
	delit(q)
#	for (i in q)
#		delete q[i]
	j = 0;
	for (i in q)
		j++
	print j, "elements still in q[]"
#	adump(q)
}
