/* ==> Do not modify this file!!  It is created automatically
   from fsf_callg_bl.m using the gen-c-prog.awk script.  <== */

#include <stdio.h>
#include "ansidecl.h"

void  fsf_callg_blurb PARAMS ((FILE *));
void
fsf_callg_blurb (file)
     FILE *file;
{
  fputs ("\n", file);
  fputs (" This table describes the call tree of the program, and was sorted by\n", file);
  fputs (" the total amount of time spent in each function and its children.\n", file);
  fputs ("\n", file);
  fputs (" Each entry in this table consists of several lines.  The line with the\n", file);
  fputs (" index number at the left hand margin lists the current function.\n", file);
  fputs (" The lines above it list the functions that called this function,\n", file);
  fputs (" and the lines below it list the functions this one called.\n", file);
  fputs (" This line lists:\n", file);
  fputs ("     index	A unique number given to each element of the table.\n", file);
  fputs ("		Index numbers are sorted numerically.\n", file);
  fputs ("		The index number is printed next to every function name so\n", file);
  fputs ("		it is easier to look up where the function in the table.\n", file);
  fputs ("\n", file);
  fputs ("     % time	This is the percentage of the `total' time that was spent\n", file);
  fputs ("		in this function and its children.  Note that due to\n", file);
  fputs ("		different viewpoints, functions excluded by options, etc,\n", file);
  fputs ("		these numbers will NOT add up to 100%.\n", file);
  fputs ("\n", file);
  fputs ("     self	This is the total amount of time spent in this function.\n", file);
  fputs ("\n", file);
  fputs ("     children	This is the total amount of time propagated into this\n", file);
  fputs ("		function by its children.\n", file);
  fputs ("\n", file);
  fputs ("     called	This is the number of times the function was called.\n", file);
  fputs ("		If the function called itself recursively, the number\n", file);
  fputs ("		only includes non-recursive calls, and is followed by\n", file);
  fputs ("		a `+' and the number of recursive calls.\n", file);
  fputs ("\n", file);
  fputs ("     name	The name of the current function.  The index number is\n", file);
  fputs ("		printed after it.  If the function is a member of a\n", file);
  fputs ("		cycle, the cycle number is printed between the\n", file);
  fputs ("		function's name and the index number.\n", file);
  fputs ("\n", file);
  fputs ("\n", file);
  fputs (" For the function's parents, the fields have the following meanings:\n", file);
  fputs ("\n", file);
  fputs ("     self	This is the amount of time that was propagated directly\n", file);
  fputs ("		from the function into this parent.\n", file);
  fputs ("\n", file);
  fputs ("     children	This is the amount of time that was propagated from\n", file);
  fputs ("		the function's children into this parent.\n", file);
  fputs ("\n", file);
  fputs ("     called	This is the number of times this parent called the\n", file);
  fputs ("		function `/' the total number of times the function\n", file);
  fputs ("		was called.  Recursive calls to the function are not\n", file);
  fputs ("		included in the number after the `/'.\n", file);
  fputs ("\n", file);
  fputs ("     name	This is the name of the parent.  The parent's index\n", file);
  fputs ("		number is printed after it.  If the parent is a\n", file);
  fputs ("		member of a cycle, the cycle number is printed between\n", file);
  fputs ("		the name and the index number.\n", file);
  fputs ("\n", file);
  fputs (" If the parents of the function cannot be determined, the word\n", file);
  fputs (" `<spontaneous>' is printed in the `name' field, and all the other\n", file);
  fputs (" fields are blank.\n", file);
  fputs ("\n", file);
  fputs (" For the function's children, the fields have the following meanings:\n", file);
  fputs ("\n", file);
  fputs ("     self	This is the amount of time that was propagated directly\n", file);
  fputs ("		from the child into the function.\n", file);
  fputs ("\n", file);
  fputs ("     children	This is the amount of time that was propagated from the\n", file);
  fputs ("		child's children to the function.\n", file);
  fputs ("\n", file);
  fputs ("     called	This is the number of times the function called\n", file);
  fputs ("		this child `/' the total number of times the child\n", file);
  fputs ("		was called.  Recursive calls by the child are not\n", file);
  fputs ("		listed in the number after the `/'.\n", file);
  fputs ("\n", file);
  fputs ("     name	This is the name of the child.  The child's index\n", file);
  fputs ("		number is printed after it.  If the child is a\n", file);
  fputs ("		member of a cycle, the cycle number is printed\n", file);
  fputs ("		between the name and the index number.\n", file);
  fputs ("\n", file);
  fputs (" If there are any cycles (circles) in the call graph, there is an\n", file);
  fputs (" entry for the cycle-as-a-whole.  This entry shows who called the\n", file);
  fputs (" cycle (as parents) and the members of the cycle (as children.)\n", file);
  fputs (" The `+' recursive calls entry shows the number of function calls that\n", file);
  fputs (" were internal to the cycle, and the calls entry for each member shows,\n", file);
  fputs (" for that member, how many times it was called from other members of\n", file);
  fputs (" the cycle.\n", file);
  fputs ("\n", file);
}
