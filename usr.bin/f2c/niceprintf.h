/* niceprintf.h -- contains constants and macros from the output filter
   for the generated C code.  We use macros for increased speed, less
   function overhead.  */

#define MAX_OUTPUT_SIZE 6000	/* Number of chars on one output line PLUS
				   the length of the longest string
				   printed using   nice_printf   */



#define next_tab(fp) (indent += tab_size)

#define prev_tab(fp) (indent -= tab_size)



