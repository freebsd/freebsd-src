{ 
	if ($1 !~ /^+[2-9]/)
		print "gawk is broken"
	else
		print "gawk is ok"
}
