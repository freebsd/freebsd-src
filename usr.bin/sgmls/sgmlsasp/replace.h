/* replace.h
   Interface to replacement file parser. */

enum replacement_type {
  DATA_REPL,
  ATTR_REPL
  };
  
struct replacement_item {
  union {
    char *attr;
    struct {
      char *s;
      unsigned n;
    } data;
  } u;
  enum replacement_type type;
  struct replacement_item *next;
};

#define NEWLINE_BEGIN 01
#define NEWLINE_END 02

struct replacement {
  unsigned flags;
  struct replacement_item *items;
};

enum event_type { START_ELEMENT, END_ELEMENT };

struct replacement_table *make_replacement_table P((void));
void load_replacement_file P((struct replacement_table *, char *));
  
struct replacement *
lookup_replacement P((struct replacement_table *, enum event_type, char *));
