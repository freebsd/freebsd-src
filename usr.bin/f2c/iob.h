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
extern void def_start(), new_iob_data(), other_undefs();
extern char *tostring();
