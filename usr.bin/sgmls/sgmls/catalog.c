/* Normalize public identifiers to handle ISO 8879[-:]1986 problem.
What should happen if there's a duplicate in a single catalog entry file? */

#include "config.h"
#include "std.h"
#include "catalog.h"

#ifdef USE_PROTOTYPES
#define P(parms) parms
#else
#define P(parms) ()
#endif

#include "alloc.h"

#define MINIMUM_DATA_CHARS \
"abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\
0123456789-.'()+,/:=?"

#define N_DECL_TYPE 3
#define PUBLIC_ID_MAP N_DECL_TYPE
#define N_TABLES (N_DECL_TYPE + 1)

enum literal_type {
  NORMAL_LITERAL,
  MINIMUM_LITERAL
};

typedef enum {
  EOF_PARAM,
  NAME_PARAM,
  LITERAL_PARAM
} PARAM_TYPE;

enum catalog_error {
  E_NAME_EXPECTED,
  E_LITERAL_EXPECTED,
  E_ARG_EXPECTED,
  E_MINIMUM_DATA,
  E_EOF_COMMENT,
  E_EOF_LITERAL,
  E_NUL_CHAR,
  E_CANNOT_OPEN,
  E_GETC,
  E_FCLOSE
};

#define FIRST_SYSTEM_ERROR E_CANNOT_OPEN

#define HASH_TABLE_INITIAL_SIZE 8
#define HASH_TABLE_MAX_SIZE (((SIZE_T)-1)/sizeof(struct hash_table_entry *))

struct hash_table_entry {
  int file_index;
  const char *key;
  const char *system_id;
};

/* Number of bytes per string block. */
#define BLOCK_SIZE 1000

/* Bytes follow the struct. */

struct string_block {
  struct string_block *next;
};

struct hash_table {
  struct hash_table_entry **v;
  SIZE_T size;			/* must be power of 2 */
  SIZE_T used;
  SIZE_T used_limit;
};

struct catalog {
  struct hash_table tables[N_TABLES];
  char **files;
  int n_files;
  struct string_block *blocks;
  char *block_ptr;
  SIZE_T block_spare;
  CATALOG_ERROR_HANDLER error_handler;
  int loaded;
};

struct parser {
  FILE *fp;
  struct catalog *cat;
  char *param;
  SIZE_T param_length;
  SIZE_T param_alloc;
  int file_index;
  const char *filename;
  unsigned long newline_count;
  char minimum_data[256];
};

static
VOID add_catalog_file P((struct catalog *cat, const char *filename,
			 SIZE_T length));
static
VOID load P((struct catalog *cat));
static
VOID parse_file P((struct parser *parser));
static
VOID parse_public P((struct parser *parser));
static
VOID parse_name_map P((struct parser *parser,
		       int decl_type));
static
int parse_arg P((struct parser *parser));
static
PARAM_TYPE parse_param P((struct parser *parser, enum literal_type));
static
VOID skip_comment P((struct parser *parser));
static
PARAM_TYPE parse_literal P((struct parser *parser, int lit,
			    enum literal_type));
static
PARAM_TYPE parse_name P((struct parser *parser, int first_char));
static
VOID param_grow P((struct parser *parser));
static
const char *param_save P((struct parser *parser));
static
char *alloc_bytes P((struct catalog *catalog, SIZE_T n));
static
int param_equal P((struct parser *parser, const char *key));
static
int hash_table_add P((struct hash_table *table, const char *s,
		      const char *system_id, int file_index));
static
struct hash_table_entry *hash_table_lookup P((struct hash_table *table,
					      const char *s));
static
struct hash_table_entry *hash_table_lookup_subst P((struct hash_table *table,
						    const char *subst_table,
						    const char *s));
static
VOID hash_table_init P((struct hash_table *p));
static
VOID hash_table_delete P((struct hash_table *p));
static
SIZE_T hash_table_start_index P((struct hash_table *p, const char *s));
static
int subst_equal P((const char *subst_table, const char *s1, const char *s2));
static
VOID error P((struct parser *parser, enum catalog_error err));

#define param_char(parser, c) \
  ((((parser)->param_length < (parser)->param_alloc) \
     || (param_grow(parser), 1)), \
  ((parser)->param[(parser)->param_length] = (c)), \
  ((parser)->param_length += 1))

#define param_init(parser) ((parser)->param_length = 0)
#define param_chop(parser) \
  ((parser)->param_length = (parser)->param_length - 1)

const char *catalog_error_text(error_number)
     int error_number;
{
  static const char *text[] = {
    "Name expected",
    "Literal expected",
    "Missing argument",
    "Only minimum data characters allowed in a public identifier",
    "End of file in comment",
    "End of file in literal",
    "Nul character is not allowed",
    "Cannot open `%s': %s",
    "Error reading `%s': %s",
    "Error closing `%s': %s"
  };
  if (error_number >= 0 && error_number < sizeof(text)/sizeof(text[0]))
    return text[error_number];
  else
    return "(invalid error number)";
}


CATALOG catalog_create(error_handler)
     CATALOG_ERROR_HANDLER error_handler;
{
  int i;
  struct catalog *p = (struct catalog *)xmalloc(sizeof(struct catalog));
  p->loaded = 0;
  p->n_files = 0;
  p->files = 0;
  p->error_handler = error_handler;
  p->blocks = 0;
  p->block_spare = 0;
  p->block_ptr = 0;
  for (i = 0; i < N_TABLES; i++)
    hash_table_init(p->tables + i);
  return (CATALOG)p;
}

VOID catalog_delete(cat)
     CATALOG cat;
{
  int i;
  struct string_block *block;
  struct catalog *catalog = (struct catalog *)cat;
  for (i = 0; i < 4; i++)
    hash_table_delete(catalog->tables + i);
  if (catalog->files)
    free(catalog->files);
  block = catalog->blocks;
  while (block) {
    struct string_block *tem = block;
    block = block->next;
    free((UNIV)tem);
  }
  catalog->blocks = 0;
  free((UNIV)catalog);
}

VOID catalog_load_file(p, filename)
     CATALOG p;
     const char *filename;
{
  add_catalog_file((struct catalog *)p, filename, strlen(filename));
}

int catalog_lookup_entity(cat, public_id, name, decl_type, subst_table,
			  system_id, catalog_file)
     CATALOG cat;
     const char *public_id;
     const char *name;
     enum catalog_decl_type decl_type;
     const char *subst_table;
     const char **system_id;
     const char **catalog_file;
{
  struct catalog *catalog = (struct catalog *)cat;
  const struct hash_table_entry *entry = 0;
  if (!catalog->loaded)
    load(catalog);
  if (public_id)
    entry = hash_table_lookup(catalog->tables + PUBLIC_ID_MAP, public_id);
  if (name
      && decl_type >= 0
      && decl_type < N_DECL_TYPE
      && (!entry || entry->file_index > 0)) {
    const struct hash_table_entry *entity_entry = 0;
    if (!subst_table)
      entity_entry = hash_table_lookup(catalog->tables + decl_type, name);
    else
      entity_entry = hash_table_lookup_subst(catalog->tables + decl_type,
					     subst_table, name);
    if (!entry
	|| (entity_entry
	    && entity_entry->file_index < entry->file_index))
      entry = entity_entry;
  }
  if (!entry)
    return 0;
  *system_id = entry->system_id;
  *catalog_file = catalog->files[entry->file_index];
  return 1;
}

static
VOID add_catalog_file(cat, filename, length)
     struct catalog *cat;
     const char *filename;
     SIZE_T length;
{
  char *s;
  if (!cat->files)
    cat->files = (char **)xmalloc(sizeof(char *));
  else
    cat->files
      = (char **)xrealloc(cat->files, (cat->n_files + 1)*sizeof(char *));
  s = alloc_bytes(cat, length + 1);
  memcpy(s, filename, length);
  s[length] = '\0';
  cat->files[cat->n_files] = s;
  cat->n_files += 1;
}

static
VOID load(cat)
     struct catalog *cat;
{
  int i;
  const char *p;
  struct parser parser;
  const char *env_var;
  int optional_file_index = cat->n_files;

  cat->loaded = 1;
  parser.param = 0;
  parser.param_alloc = 0;
  parser.cat = cat;
  for (i = 0; i < 256; i++)
    parser.minimum_data[i] = 0;
  for (p = MINIMUM_DATA_CHARS; *p; p++)
    parser.minimum_data[(unsigned char)*p] = 1;
  env_var = getenv(CATALOG_FILES_ENV_VAR);
  if (!env_var || *env_var == '\0')
    env_var = DEFAULT_CATALOG_FILES;
  for (;;) {
    for (p = env_var; *p && *p != PATH_FILE_SEP; p++)
      ;
    if (p > env_var)
      add_catalog_file(cat, env_var, p - env_var);
    if (!*p)
      break;
    env_var = p + 1;
  }
  for (i = 0; i < cat->n_files; i++) {
    parser.filename = cat->files[i];
    parser.newline_count = 0;
    parser.fp = fopen(cat->files[i], "r");
    if (!parser.fp) {
      if (i < optional_file_index)
	error(&parser, E_CANNOT_OPEN);
    }
    else {
      parser.file_index = i;
      parse_file(&parser);
      errno = 0;
      if (fclose(parser.fp) < 0)
	error(&parser, E_FCLOSE);
    }
  }
  if (parser.param)
    free(parser.param);
}

static
VOID parse_file(parser)
     struct parser *parser;
{
  int skipping = 0;
  for (;;) {
    PARAM_TYPE type = parse_param(parser, NORMAL_LITERAL);
    if (type == NAME_PARAM) {
      if (param_equal(parser, "PUBLIC"))
	parse_public(parser);
      else if (param_equal(parser, "ENTITY"))
	parse_name_map(parser, CATALOG_ENTITY_DECL);
      else if (param_equal(parser, "DOCTYPE"))
	parse_name_map(parser, CATALOG_DOCTYPE_DECL);
      else if (param_equal(parser, "LINKTYPE"))
	parse_name_map(parser, CATALOG_LINKTYPE_DECL);
      else
	skipping = 1;
    }
    else if (type == EOF_PARAM)
      break;
    else if (!skipping) {
      skipping = 1;
      error(parser, E_NAME_EXPECTED);
    }
  }
}

static
VOID parse_public(parser)
     struct parser *parser;
{
  const char *public_id;

  if (parse_param(parser, MINIMUM_LITERAL) != LITERAL_PARAM)
    error(parser, E_LITERAL_EXPECTED);
  public_id = param_save(parser);
  if (!parse_arg(parser))
    return;
  hash_table_add(parser->cat->tables + PUBLIC_ID_MAP,
		 public_id, param_save(parser), parser->file_index);
}

static
VOID parse_name_map(parser, decl_type)
     struct parser *parser;
     int decl_type;
{
  const char *name;

  if (!parse_arg(parser))
    return;
  name = param_save(parser);
  if (!parse_arg(parser))
    return;
  hash_table_add(parser->cat->tables + decl_type,
		 name, param_save(parser), parser->file_index);
}

static
int parse_arg(parser)
     struct parser *parser;
{
  PARAM_TYPE parm = parse_param(parser, NORMAL_LITERAL);
  if (parm != NAME_PARAM && parm != LITERAL_PARAM) {
    error(parser, E_ARG_EXPECTED);
    return 0;
  }
  return 1;
}

static
PARAM_TYPE parse_param(parser, lit_type)
     struct parser *parser;
     enum literal_type lit_type;
{
  for (;;) {
    int c = getc(parser->fp);
    switch (c) {
    case EOF:
      if (ferror(parser->fp))
	error(parser, E_GETC);
      return EOF_PARAM;
    case '"':
    case '\'':
      return parse_literal(parser, c, lit_type);
    case '\n':
      parser->newline_count += 1;
      break;
    case '\t':
    case ' ':
      break;
    case '\0':
      error(parser, E_NUL_CHAR);
      break;
    case '-':
      c = getc(parser->fp);
      if (c == '-') {
	skip_comment(parser);
	break;
      }
      ungetc(c, parser->fp);
      c = '-';
      /* fall through */
    default:
      return parse_name(parser, c);
    }
  }
}

static
VOID skip_comment(parser)
     struct parser *parser;
{
  FILE *fp = parser->fp;
  for (;;) {
    int c = getc(fp);
    if (c == '-') {
      c = getc(fp);
      if (c == '-')
	return;
    }
    if (c == EOF) {
      if (ferror(fp))
	error(parser, E_GETC);
      error(parser, E_EOF_COMMENT);
      return;
    }
    if (c == '\n')
      parser->newline_count += 1;
  }
}

static
PARAM_TYPE parse_literal(parser, lit, lit_type)
     struct parser *parser;
     int lit;
     enum literal_type lit_type;
{
  enum { no, yes_begin, yes_middle } skipping = yes_begin;
  FILE *fp = parser->fp;
  param_init(parser);
  for (;;) {
    int c = getc(fp);
    if (c == lit)
      break;
    switch (c) {
    case '\0':
      error(parser, E_NUL_CHAR);
      break;
    case EOF:
      if (ferror(fp))
	error(parser, E_GETC);
      error(parser, E_EOF_LITERAL);
      return LITERAL_PARAM;
    case '\n':
      parser->newline_count += 1;
      /* fall through */
    case ' ':
      if (lit_type == MINIMUM_LITERAL) {
	if (skipping == no) {
	  param_char(parser, ' ');
	  skipping = yes_middle;
	}
      }
      else
	param_char(parser, c);
      break;
    default:
      if (lit_type == MINIMUM_LITERAL) {
	if (!parser->minimum_data[c])
	  error(parser, E_MINIMUM_DATA);
	else {
	  skipping = no;
	  param_char(parser, c);
	}
      }
      else
	param_char(parser, c);
      break;
    }
  }
  if (skipping == yes_middle)
    param_chop(parser);
  return LITERAL_PARAM;
}

static
PARAM_TYPE parse_name(parser, first_char)
     struct parser *parser;
     int first_char;
{
  FILE *fp = parser->fp;
  param_init(parser);
  param_char(parser, first_char);
  for (;;) {
    int c = getc(fp);
    switch (c) {
    case '\0':
      error(parser, E_NUL_CHAR);
      break;
    case EOF:
      if (ferror(fp))
	error(parser, E_GETC);
      goto done;
    case '\n':
      parser->newline_count += 1;
      goto done;
    case ' ':
    case '\t':
      goto done;
    case '"':
    case '\'':
      ungetc(c, fp);
      goto done;
    default:
      param_char(parser, c);
    }
  }
 done:
  return NAME_PARAM;
}

static
VOID param_grow(parser)
     struct parser *parser;
{
  if (parser->param_alloc == 0) {
    parser->param_alloc = 256;
    parser->param = xmalloc(parser->param_alloc);
  }
  else {
    parser->param_alloc *= 2;
    parser->param = xrealloc(parser->param, parser->param_alloc);
  }
}

static
const char *param_save(parser)
     struct parser *parser;
{
  char *s = alloc_bytes(parser->cat, parser->param_length + 1);
  memcpy(s, parser->param, parser->param_length);
  s[parser->param_length] = '\0';
  return s;
}

static
char *alloc_bytes(catalog, n)
     struct catalog *catalog;
     SIZE_T n;
{
  char *tem;
  if (n > catalog->block_spare) {
    struct string_block *block;
    SIZE_T block_size = n > BLOCK_SIZE ? n : BLOCK_SIZE;
    block
      = (struct string_block *)xmalloc(sizeof(struct string_block)
				       + block_size);
    block->next = catalog->blocks;
    catalog->blocks = block;
    catalog->block_ptr = (char *)(block + 1);
    catalog->block_spare = block_size;
  }
  tem = catalog->block_ptr;
  catalog->block_ptr += n;
  catalog->block_spare -= n;
  return tem;
}


/* Return 1 if the current parameter is equal to key. */

static
int param_equal(parser, key)
     struct parser *parser;
     const char *key;
{
  const char *param = parser->param;
  SIZE_T param_length = parser->param_length;
  for (; param_length > 0; param++, param_length--, key++) {
    unsigned char c;
    if (*key == '\0')
      return 0;
    c = *param;
    if (islower(c))
      c = toupper(c);
    if (c != (unsigned char)*key)
      return 0;
  }
  return *key == '\0';
}

/* Return 0 if it was a duplicate. */

static
int hash_table_add(table, s, system_id, file_index)
     struct hash_table *table;
     const char *s;
     const char *system_id;
     int file_index;
{
  SIZE_T i;
  struct hash_table_entry *p;

  if (table->size > 0) {
    i = hash_table_start_index(table, s);
    while (table->v[i] != 0)  {
      if (strcmp(table->v[i]->key, s) == 0)
	return 0;
      if (i == 0)
	i = table->size;
      i--;
    }
  }
  if (table->used >= table->used_limit) {
    SIZE_T j;
    struct hash_table_entry **old_table = table->v;
    SIZE_T old_size = table->size;
    if (old_size == 0) {
      table->size = HASH_TABLE_INITIAL_SIZE;
      table->used_limit = table->size/2;
    }
    else {
      if (old_size > HASH_TABLE_MAX_SIZE/2) {
	if (old_size == HASH_TABLE_MAX_SIZE)
	  return 0;		/* FIXME: give an error? */
	table->size = HASH_TABLE_MAX_SIZE;
	table->used_limit = HASH_TABLE_MAX_SIZE - 1;
      }
      else {
	table->size = (old_size << 1);
	table->used_limit = table->size/2;
      }
    }
    table->v
      = (struct hash_table_entry **)xmalloc(sizeof(struct hash_table_entry *)
					    * table->size);
    for (j = 0; j < table->size; j++)
      table->v[j] = 0;
    for (j = 0; j < old_size; j++)
      if (old_table[j]) {
	SIZE_T k = hash_table_start_index(table, old_table[j]->key);
	while (table->v[k] != 0) {
	  if (k == 0)
	    k = table->size;
	  k--;
	}
	table->v[k] = old_table[j];
      }
    if (old_table)
      free((UNIV)old_table);
    i = hash_table_start_index(table, s);
    while (table->v[i] != 0) {
      if (i == 0)
	i = table->size;
      i--;
    }
  }
  p = (struct hash_table_entry *)xmalloc(sizeof(struct hash_table_entry));
  p->key = s;
  p->system_id = system_id;
  p->file_index = file_index;
  table->v[i] = p;
  table->used += 1;
  return 1;
}

static
struct hash_table_entry *hash_table_lookup(table, s)
     struct hash_table *table;
     const char *s;
{
  if (table->size > 0) {
    SIZE_T i;
    i = hash_table_start_index(table, s);
    while (table->v[i] != 0)  {
      if (strcmp(table->v[i]->key, s) == 0)
	return table->v[i];
      if (i == 0)
	i = table->size;
      i--;
    }
  }
  return 0;
}

static
struct hash_table_entry *hash_table_lookup_subst(table, subst_table, s)
     struct hash_table *table;
     const char *subst_table;
     const char *s;
{
  SIZE_T i;
  for (i = 0;  i < table->size; i++) {
    struct hash_table_entry *p = table->v[i];
    if (p && subst_equal(subst_table, s, p->key))
      return p;
  }
  return 0;
}

static
VOID hash_table_init(p)
     struct hash_table *p;
{
  p->v = 0;
  p->size = 0;
  p->used = 0;
  p->used_limit = 0;
}

static
VOID hash_table_delete(p)
     struct hash_table *p;
{
  if (p->v) {
    SIZE_T i;
    for (i = 0; i < p->size; i++)
      if (p->v[i])
	free(p->v[i]);
    free(p->v);
  }
}

static
SIZE_T hash_table_start_index(p, s)
     struct hash_table *p;
     const char *s;
{
  unsigned long h = 0;
  while (*s)
    h = (h << 5) + h + (unsigned char)*s++;
  return (h & (p->size - 1));
}

/* s1 has already been substituted; s2 has not */

static
int subst_equal(subst_table, s1, s2)
     const char *subst_table;
     const char *s1;
     const char *s2;
{
  for (; *s1 == subst_table[(unsigned char)*s2]; s1++, s2++)
    if (*s1 == '\0')
      return 1;
  return 0;
}

static
VOID error(parser, err)
     struct parser *parser;
     enum catalog_error err;
{
  (*parser->cat->error_handler)(parser->filename,
				parser->newline_count + 1,
				err,
				(err >= FIRST_SYSTEM_ERROR
				 ? CATALOG_SYSTEM_ERROR
				 : 0),
				(err >= FIRST_SYSTEM_ERROR
				 ? errno
				 : 0));
}

#ifdef MAIN

static const char *program_name;

#include "getopt.h"

static VOID usage P((void));
static VOID out_of_memory P((void));
static VOID handle_catalog_error P((const char *filename,
				    unsigned long lineno,
				    int error_number,
				    unsigned flags,
				    int sys_errno));

int main(argc, argv)
     int argc;
     char **argv;
{
  int entity_flag = 0;
  enum catalog_decl_type entity_type = CATALOG_NO_DECL;
  char *public_id = 0;
  char *name = 0;
  int exit_status;
  int opt;
  CATALOG catalog;
  int i;
  const char *file;
  const char *system_id;

  program_name = argv[0];

  while ((opt = getopt(argc, argv, "edl")) != EOF)
    switch (opt) {
    case 'e':
      entity_flag = 1;
      entity_type = CATALOG_ENTITY_DECL;
      break;
    case 'd':
      entity_flag = 1;
      entity_type = CATALOG_DOCTYPE_DECL;
      break;
    case 'l':
      entity_flag = 1;
      entity_type = CATALOG_LINKTYPE_DECL;
      break;
    case '?':
      usage();
    }
  if (argc - optind < 2)
    usage();
  if (entity_flag)
    name = argv[optind];
  else
    public_id = argv[optind];

  catalog = catalog_create(handle_catalog_error);
  for (i = optind + 1; i < argc; i++)
    catalog_load_file(catalog, argv[i]);
  if (catalog_lookup_entity(catalog, public_id, name, entity_type, (char *)0,
			    &system_id, &file)) {
    exit_status = 0;
    fprintf(stderr, "%s (%s)\n", system_id, file);
  }
  else {
    fprintf(stderr, "not found\n");
    exit_status = 1;
  }
  catalog_delete(catalog);
  return exit_status;
}

static
VOID usage()
{
  fprintf(stderr, "usage: %s [-e] [-d] [-l] id file ...\n",
	  program_name);
  exit(1);
}

static
VOID handle_catalog_error(filename, lineno, error_number, flags, sys_errno)
     const char *filename;
     unsigned long lineno;
     int error_number;
     unsigned flags;
     int sys_errno;
{
  fprintf(stderr, "%s:", program_name);
  if (flags & CATALOG_SYSTEM_ERROR) {
    putc(' ', stderr);
    fprintf(stderr, catalog_error_text(error_number), filename);
    putc('\n', stderr);
  }
  else
    fprintf(stderr, "%s:%lu: %s\n", filename, lineno,
	    catalog_error_text(error_number));
  fflush(stderr);
}

UNIV xmalloc(n)
     SIZE_T n;
{
  UNIV p = malloc(n);
  if (!p)
    out_of_memory();
  return p;
}

UNIV xrealloc(p, n)
     UNIV p;
     SIZE_T n;
{
  p = realloc(p, n);
  if (!p)
    out_of_memory();
  return p;
}

static
VOID out_of_memory()
{
  fprintf(stderr, "%s: out of memory\n", program_name);
  exit(1);
}

#endif /* MAIN */
