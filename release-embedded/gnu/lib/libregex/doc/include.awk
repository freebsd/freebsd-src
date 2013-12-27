# Assume `source' is set with -vsource=filename on the command line.
# 
/^\[\[\[/	{ inclusion = $2; # name of the thing to include.
                  printing = 0;
                  while ((getline line < source) > 0)
                    {
                      if (match (line, "\\[\\[\\[end " inclusion "\\]\\]\\]"))
                        printing = 0;

                      if (printing)
                        print line;

                      if (match (line,"\\[\\[\\[begin " inclusion "\\]\\]\\]"))
                        printing = 1;
                    }
                  close (source);
		  next;
                }
		{ print }
