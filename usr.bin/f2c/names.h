#define CONST_IDENT_MAX 30
#define IO_IDENT_MAX 30
#define ARGUMENT_MAX 30
#define USER_LABEL_MAX 30

#define EQUIV_INIT_NAME "equiv"

#define write_nv_ident(fp,a) wr_nv_ident_help ((fp), (struct Addrblock *) (a))
#define nv_type(x) nv_type_help ((struct Addrblock *) x)

extern char *c_keywords[];

char *new_io_ident (/* char * */);
char *new_func_length (/* char * */);
char *new_arg_length (/* Namep */);
void declare_new_addr (/* struct Addrblock * */);
char *nv_ident_help (/* struct Addrblock * */);
int nv_type_help (/* struct Addrblock */);
char *user_label (/* int */);
char *temp_name (/* int, char */);
char *c_type_decl (/* int, int */);
char *equiv_name (/* int, char * */);
