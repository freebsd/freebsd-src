struct vary {
  char flag;
  char *arg;
  struct vary *next;
};

extern struct vary *vary_append(struct vary *v, char flag, char *arg);
extern const struct vary *vary_apply(const struct vary *v, struct tm *t);
extern void vary_destroy(struct vary *v);
