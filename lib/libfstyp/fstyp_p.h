//TODO: Need a default copyright notice here
#ifndef FSTYP_P_H
#define	FSTYP_P_H

#define	MIN(a,b) (((a)<(b))?(a):(b))

void *read_buf(FILE *fp, off_t off, size_t len);
void rtrim(char *label, size_t size);

extern bool encodings_enabled;

#endif