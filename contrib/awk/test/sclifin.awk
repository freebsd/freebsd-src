BEGIN {
	j = 4
	if ("foo" in j)
		print "ouch"
	else
		print "ok"
}
