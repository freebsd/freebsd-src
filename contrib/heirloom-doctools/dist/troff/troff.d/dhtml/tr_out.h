#define hdecr() hdec += 10 * fontsize

void out_anchor(char *);
void out_begin_link(char *);
void out_begin_ulink(char *);
void out_end_link(void);
void out_f(int);
void out_h(int);
void out_n(int);
void out_s(int);
void out_V(int);
void out_w(void);
void out_x_f(int, char *);
void out_x_T(char *);

extern int fontsize;
extern int hdec;
