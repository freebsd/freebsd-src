#
#  Awk program to analyze mtrace.c output.
#
$1 == "+"	{ if (allocated[$2] != "")
		    print "+", $2, "Alloc", NR, "duplicate:", allocated[$2];
		  else
		    allocated[$2] = $3;
		}
$1 == "-"	{ if (allocated[$2] != "") {
		    allocated[$2] = "";
		    if (allocated[$2] != "")
			print "DELETE FAILED", $2, allocated[$2];
		  } else
		    print "-", $2, "Free", NR, "was never alloc'd";
		}
$1 == "<"	{ if (allocated[$2] != "")
		    allocated[$2] = "";
		  else
		    print "-", $2, "Realloc", NR, "was never alloc'd";
		}
$1 == ">"	{ if (allocated[$2] != "")
		    print "+", $2, "Realloc", NR, "duplicate:", allocated[$2];
		  else
		    allocated[$2] = $3;
		}

# Ignore "= Start"
$1 == "="	{ }
# Ignore failed realloc attempts for now
$1 == "!"	{ }


END		{ for (x in allocated) 
		    if (allocated[x] != "")
		      print "+", x, allocated[x];
		}
