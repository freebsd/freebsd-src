/* ==> Do not modify this file!!  It is created automatically
   from flat_bl.m using the gen-c-prog.awk script.  <== */

#include <stdio.h>
#include "ansidecl.h"

void  flat_blurb PARAMS ((FILE *));
void
flat_blurb (file)
     FILE *file;
{
  fputs ("\n", file);
  fputs (" %         the percentage of the total running time of the\n", file);
  fputs ("time       program used by this function.\n", file);
  fputs ("\n", file);
  fputs ("cumulative a running sum of the number of seconds accounted\n", file);
  fputs (" seconds   for by this function and those listed above it.\n", file);
  fputs ("\n", file);
  fputs (" self      the number of seconds accounted for by this\n", file);
  fputs ("seconds    function alone.  This is the major sort for this\n", file);
  fputs ("           listing.\n", file);
  fputs ("\n", file);
  fputs ("calls      the number of times this function was invoked, if\n", file);
  fputs ("           this function is profiled, else blank.\n", file);
  fputs (" \n", file);
  fputs (" self      the average number of milliseconds spent in this\n", file);
  fputs ("ms/call    function per call, if this function is profiled,\n", file);
  fputs ("	   else blank.\n", file);
  fputs ("\n", file);
  fputs (" total     the average number of milliseconds spent in this\n", file);
  fputs ("ms/call    function and its descendents per call, if this \n", file);
  fputs ("	   function is profiled, else blank.\n", file);
  fputs ("\n", file);
  fputs ("name       the name of the function.  This is the minor sort\n", file);
  fputs ("           for this listing. The index shows the location of\n", file);
  fputs ("	   the function in the gprof listing. If the index is\n", file);
  fputs ("	   in parenthesis it shows where it would appear in\n", file);
  fputs ("	   the gprof listing if it were to be printed.\n", file);
}
