struct iob_data {
	struct iob_data *next;
	char *type;
	char *name;
	char *fields[1];
	};
struct io_setup {
	char **fields;
	int nelt, type;
	};

struct defines {
	struct defines *next;
	char defname[1];
	};

typedef struct iob_data iob_data;
typedef struct io_setup io_setup;
typedef struct defines defines;

extern iob_data *iob_list;
extern struct Addrblock *io_structs[9];
void	def_start Argdcl((FILEP, char*, char*, char*));
void	new_iob_data Argdcl((io_setup*, char*));
void	other_undefs Argdcl((FILEP));
char*	tostring Argdcl((char*, int));
