#!/usr/bin/awk -f

BEGIN {
}

/[{ \t]\.ct_/ {
	member = 1;
	while ($member !~ /^\.ct/)
		member++
	if ($(member + 1) == "=")
		value = member + 2
	else {
		# Require ".ct_XXX = <value>"
		printf("malformed line[%d]: %s", NR,  $0) > "/dev/stderr"
		exit 1
	}
	sub(/[,}]*$/, "", $value)
	if ($member ~ /\.ct_func/)
		funcs[$value]
	else if ($member ~ /\.ct_check_xfail/)
		xfail_funcs[$value]
	else if ($member ~ /\.ct_cp2_exccode/)
		cp2_exccodes[$value]
	else if ($member ~ /\.ct_mips_exccode/)
		mips_exccodes[$value]

}

END {
	printf("/* test functions */\n")
	for (f in funcs)
		printf("#define\t%s\tNULL\n", f)
	printf("\n/* eXpected failure check functions */\n")
	for (f in xfail_funcs)
		printf("#define\t%s\tNULL\n", f)
	printf("\n/* CP2 exception codes */\n")
	for (code in cp2_exccodes)
		printf("#define\t%s\tNULL\n", code)
	printf("\n/* MIPS exception codes */\n")
	for (code in mips_exccodes)
		printf("#define\t%s\tNULL\n", code)
	printf("\n")
	print "#ifdef CHERI_C_TESTS"
	print "#define\tDECLARE_TEST(name, desc) \\"
	print "    inline void cheri_c_test_ ## name( \\"
	print "    const struct cheri_test *ctp __unused) {}"
	print "    #include <cheri_c_testdecls.h>"
	print "    #undef\tDECLARE_TEST"
	print "#endif"
}
