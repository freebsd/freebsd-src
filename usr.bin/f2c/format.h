#define DEF_C_LINE_LENGTH 77
/* actual max will be 79 */

extern int c_output_line_length;	/* max # chars per line in C source
					   code */

chainp	data_value Argdcl((FILEP, long int, int));
int	do_init_data Argdcl((FILEP, FILEP));
void	list_init_data Argdcl((FILEP*, char*, FILEP));
char*	wr_ardecls Argdcl((FILEP, struct Dimblock*, long int));
void	wr_one_init Argdcl((FILEP, char*, chainp*, int));
void	wr_output_values Argdcl((FILEP, Namep, chainp));
