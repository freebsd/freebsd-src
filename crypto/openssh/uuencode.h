#ifndef UUENCODE_H
#define UUENCODE_H
int	uuencode(unsigned char *src, unsigned int srclength, char *target, size_t targsize);
int	uudecode(const char *src, unsigned char *target, size_t targsize);
void	dump_base64(FILE *fp, unsigned char *data, int len);
#endif
