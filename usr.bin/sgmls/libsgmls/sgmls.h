/* sgmls.h
   Interface to a library for reading output of sgmls. */

struct sgmls_data {
  char *s;
  unsigned len;
  char is_sdata;
};

struct sgmls_notation {
  char *name;
  char *sysid;
  char *pubid;
};

struct sgmls_internal_entity {
  char *name;
  struct sgmls_data data;
};

enum sgmls_external_entity_type {
  SGMLS_ENTITY_CDATA,
  SGMLS_ENTITY_SDATA,
  SGMLS_ENTITY_NDATA,
  SGMLS_ENTITY_SUBDOC
  };

struct sgmls_external_entity {
  char *name;
  enum sgmls_external_entity_type type;
  char **filenames;
  int nfilenames;
  char *pubid;
  char *sysid;
  struct sgmls_attribute *attributes;
  struct sgmls_notation *notation;
};

struct sgmls_entity {
  union {
    struct sgmls_internal_entity internal;
    struct sgmls_external_entity external;
  } u;
  char is_internal;
};

enum sgmls_attribute_type {
  SGMLS_ATTR_IMPLIED,
  SGMLS_ATTR_CDATA,
  SGMLS_ATTR_TOKEN,
  SGMLS_ATTR_ENTITY,
  SGMLS_ATTR_NOTATION
};

struct sgmls_attribute {
  struct sgmls_attribute *next;
  char *name;
  enum sgmls_attribute_type type;
  union {
    struct {
      struct sgmls_data *v;
      int n;
    } data;
    struct {
      struct sgmls_entity **v;
      int n;
    } entity;
    struct {
      char **v;
      int n;
    } token;
    struct sgmls_notation *notation;
  } value;
};

enum sgmls_event_type {
  SGMLS_EVENT_DATA,		/* data */
  SGMLS_EVENT_ENTITY,		/* external entity reference */
  SGMLS_EVENT_PI,		/* processing instruction */
  SGMLS_EVENT_START,		/* element start */
  SGMLS_EVENT_END,		/* element end */
  SGMLS_EVENT_SUBSTART,		/* subdocument start */
  SGMLS_EVENT_SUBEND,		/* subdocument end */
  SGMLS_EVENT_APPINFO,		/* appinfo */
  SGMLS_EVENT_CONFORMING        /* the document was conforming */
  };

struct sgmls_event {
  enum sgmls_event_type type;
  union {
    struct {
      struct sgmls_data *v;
      int n;
    } data;
    struct sgmls_external_entity *entity;
    struct {
      char *s;
      unsigned len;
    } pi;
    struct {
      char *gi;
      struct sgmls_attribute *attributes;
    } start;
    struct {
      char *gi;
    } end;
    char *appinfo;
  } u;
  char *filename;		/* SGML filename */
  unsigned long lineno;		/* SGML lineno */
};

#ifdef __STDC__
void sgmls_free_attributes(struct sgmls_attribute *);
struct sgmls *sgmls_create(FILE *);
int sgmls_next(struct sgmls *, struct sgmls_event *);
void sgmls_free(struct sgmls *);
typedef void sgmls_errhandler(int, char *, unsigned long);
sgmls_errhandler *sgmls_set_errhandler(sgmls_errhandler *);
#else /* not __STDC__ */
void sgmls_free_attributes();
struct sgmls *sgmls_create();
int sgmls_next();
void sgmls_free();
typedef void sgmls_errhandler();
sgmls_errhandler *sgmls_set_errhandler();
#endif /* not __STDC__ */
