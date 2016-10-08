#define I2BST(i) ((union bst_val)(int)i)
#define S2BST(s) ((union bst_val)(void *)s)

void *file2ram(char *, ssize_t *);
ssize_t lineskip(char **, ssize_t);
char *get_word(char **, ssize_t *, size_t *, int *);
char *get_line(char **, ssize_t *, size_t *);
int bst_scmp(union bst_val, union bst_val);
int bst_icmp(union bst_val, union bst_val);
