BEGIN { RS = "" }

{
	if (/^@/)
		print "not ok"
	else
		print "ok"
}
