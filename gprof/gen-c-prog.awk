NR == 1	{
    FS="\"";
    print "/* ==> Do not modify this file!!  It is created automatically"
    printf "   from %s using the gen-c-prog.awk script.  <== */\n\n", FILE
    print "#include <stdio.h>"
    print "#include \"ansidecl.h\""
}

	{
	  if (curfun != FUNCTION)
	    {
	      if (curfun)
		print "}"
	      curfun = FUNCTION
	      print ""
	      print "void ", FUNCTION, "(FILE *);"
	      print "void";
	      printf "%s (file)\n", FUNCTION
	      print "     FILE *file;";
	      print "{";
	    }
	  printf "  fputs (\"";
	  for (i = 1; i < NF; i++)
	    printf "%s\\\"", $i;
	  printf "%s\\n\", file);\n", $NF;
}

END	{ print "}" }
