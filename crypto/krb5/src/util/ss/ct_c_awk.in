/^command_table / {
	cmdtbl = $2;
	printf "/* %s.c - automatically generated from %s.ct */\n", \
		rootname, rootname > outfile
	print "#include <ss/ss.h>" > outfile
	print "" >outfile
	print "#ifndef __STDC__" > outfile
	print "#define const" > outfile
	print "#endif" > outfile
	print "" > outfile
}
	
/^BOR$/ {
	cmdnum++
	options = 0
	cmdtab = ""
	printf "static char const * const ssu%05d[] = {\n", cmdnum > outfile
}

/^sub/ {
	subr = substr($0, 6, length($0)-5)
}

/^hlp/ {
	help = substr($0, 6, length($0)-5)
}

/^cmd/ {
	cmd = substr($0, 6, length($0)-5)
	printf "%s\"%s\",\n", cmdtab, cmd > outfile
	cmdtab = "    "
}

/^opt/ {
	opt = substr($0, 6, length($0)-5)
	if (opt == "dont_list") {
		options += 1
	}
	if (opt == "dont_summarize") {
		options += 2
	}
}

/^EOR/ {
	print "    (char const *)0" > outfile
	print "};" > outfile 
	printf "extern void %s __SS_PROTO;\n", subr > outfile
	subr_tab[cmdnum] = subr
	options_tab[cmdnum] = options
	help_tab[cmdnum] = help
}

/^[0-9]/ {
	linenum = $1;
}

/^ERROR/ {
	error = substr($0, 8, length($0)-7)
	printf "Error in line %d: %s\n", linenum, error
	print "#__ERROR_IN_FILE__" > outfile
}

END {
	printf "static ss_request_entry ssu%05d[] = {\n", cmdnum+1 > outfile
	for (i=1; i <= cmdnum; i++) {
		printf "    { ssu%05d,\n", i > outfile
		printf "      %s,\n", subr_tab[i] > outfile
		printf "      \"%s\",\n", help_tab[i] > outfile
		printf "      %d },\n", options_tab[i] > outfile
	}
	print "    { 0, 0, 0, 0 }" > outfile
	print "};" > outfile
	print "" > outfile
	printf "ss_request_table %s = { 2, ssu%05d };\n", \
		cmdtbl, cmdnum+1 > outfile
}

