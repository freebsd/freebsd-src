#define clslig() \
	do { \
		if (prevchar) { \
			chrout(prevchar); \
			prevchar = 0; \
		} \
	} while (0)

void char_open(void);
void char_c(int);
void char_C(char *);
void char_N(int);
void chrout(int);

extern int prevchar;
