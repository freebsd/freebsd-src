#ifndef KEY_H
#define KEY_H

typedef struct Key Key;
enum types {
	KEY_RSA,
	KEY_DSA,
	KEY_EMPTY
};
struct Key {
	int	type;
	RSA	*rsa;
	DSA	*dsa;
};

Key	*key_new(int type);
void	key_free(Key *k);
int	key_equal(Key *a, Key *b);
char	*key_fingerprint(Key *k);
char	*key_type(Key *k);
int	key_write(Key *key, FILE *f);
unsigned int
key_read(Key *key, char **cpp);

#endif
