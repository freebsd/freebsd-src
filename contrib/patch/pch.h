/* reading patches */

/* $Id: pch.h,v 1.8 1997/06/13 06:28:37 eggert Exp $ */

LINENUM pch_end PARAMS ((void));
LINENUM pch_first PARAMS ((void));
LINENUM pch_hunk_beg PARAMS ((void));
LINENUM pch_newfirst PARAMS ((void));
LINENUM pch_prefix_context PARAMS ((void));
LINENUM pch_ptrn_lines PARAMS ((void));
LINENUM pch_repl_lines PARAMS ((void));
LINENUM pch_suffix_context PARAMS ((void));
bool pch_swap PARAMS ((void));
bool pch_write_line PARAMS ((LINENUM, FILE *));
bool there_is_another_patch PARAMS ((void));
char *pfetch PARAMS ((LINENUM));
char pch_char PARAMS ((LINENUM));
int another_hunk PARAMS ((enum diff, int));
int pch_says_nonexistent PARAMS ((int));
size_t pch_line_len PARAMS ((LINENUM));
time_t pch_timestamp PARAMS ((int));
void do_ed_script PARAMS ((FILE *));
void open_patch_file PARAMS ((char const *));
void re_patch PARAMS ((void));
void set_hunkmax PARAMS ((void));
