/* findterm.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:29:56
 *
 */

#include "defs.h"

#include <ctype.h>
#include <fcntl.h>
#ifdef USE_STDDEF
#include <sys/types.h>
#endif
#include <sys/stat.h>
#ifdef __FreeBSD__
#include <unistd.h>
#endif

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo findterm.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif
static int linecnt;

static int
getln(f, buf, len)
FILE *f;
register char *buf;
int len; {
	register int c, i = 0;

	while((c = getc(f)) == '#') {
		linecnt++;
		while((c = getc(f)) != '\n')
			if (c == EOF)
				return -1;
	}

	while(c != '\n') {
		if (c == EOF)
			return -1;
		if (i < len) {
			i++;
			*buf++ = c;
		}
		c = getc(f);
	}

	while(i > 0 && isspace(*(buf-1))) {
		buf--;
		i--;
	}

	*buf = '\0';
	return i;
}

static int
_findterm2(name, file, buf)
char *name, *buf;
char *file; {
	char line[MAX_LINE];
	FILE *f;
	register char *sp, *dp;
	int c;
	int l;
	int cont;
	int fd;
	struct stat st;

	linecnt = 0;

#ifdef DEBUG
	printf("open: %s\n", file);
#endif
	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -1;
	if (fstat(fd, &st) == -1) {
		close(fd);
		return -1;
	}
	if ((st.st_mode & 0170000) == 0040000) {
		sprintf(buf, "%s/%c/%s", file, name[0], name);
		close(fd);
		fd = open(buf, O_RDONLY);
		if (fd == -1)
			return -1;
		if (read(fd, buf, MAX_BUF) < 12
		    || buf[0] != 032 || buf[1] != 1) {
			close(fd);
			return -1;
		}
		close(fd);
		return 3;
	}
	f = fdopen(fd, "r");
	if (f == NULL) {
		close(fd);
		return -1;
	}

	while ((l = getln(f, buf, MAX_LINE)) != -1) {
		linecnt++;
		if (!isspace(buf[0]) && l != 0) {
			sp = buf + l - 1;
			cont = 0;
			switch(*sp) {
			case '\\':
				cont = 1;
				*sp = '\0';
				/* FALLTHROUGH */
			case ':':
				sp = buf;
				dp = line;
				while (*sp != ':') {
					if (*sp == '\0' && cont &&
					    (l = getln(f, buf, MAX_LINE))
					     != -1) {
						linecnt++;
						sp = buf;
						if (l > 0 && buf[l-1] == '\\')
							cont = 1;
						else
							cont = 0;
						continue;
					}
					if (*sp == '\0') {
#ifdef DEBUG
						printf("bad line (%d)\n",
						       linecnt);
						fclose(f);
						return -2;
#else
						goto err;
#endif
					}
					*dp++ = *sp++;
				}
				*dp = '\0';
				if (!_tmatch(line, name))
					break;
				if (!cont) {
					fclose(f);
					return 1;
				}
				l = strlen(buf);
				dp = buf + l;
				while((c = getc(f)) != EOF && l < MAX_BUF) {
					if (c == '\n')
						break;
					if (c == '\\') {
						c = getc(f);
						if (c == EOF)
							break;
						if (c == '\n') {
							c = getc(f);
							if (c == EOF)
								break;
							if (c == '#') {
								while((c = getc(f)) != EOF && c != '\n');
								if (c == EOF)
									break;
								continue;
							}
							*dp++ = c;
							continue;
						}
						*dp++ = '\\';
						*dp++ = c;
						continue;
					}
					*dp++ = c;
				}
				*dp = '\0';
				fclose(f);
				return 1;
			case ',':
				sp = buf;
				dp = line;
				while(*sp != ',')
					*dp++ = *sp++;
				*dp = '\0';
				if (!_tmatch(line, name))
					break;
				dp = buf + l;
				while ((c = getc(f)) != EOF && l < MAX_BUF) {
					if (c == '\n') {
						c = getc(f);
						if (isspace(c))
							continue;
						if (c == '\n') {
							ungetc(c, f);
							continue;
						}
						if (c == '#') {
							while((c = getc(f)) != EOF)
								if (c == '\n')
									break;
							if (c == EOF)
								break;
							ungetc(c, f);
							continue;
						}
						break;
					}
					*dp++ = c;
					l++;
				}
				*dp = '\0';
				fclose(f);
				return 2;
			default:
			err:
#ifdef DEBUG
				printf("strange line (%d)\n", linecnt);
#endif
				break;
			}
		}
	}
	fclose(f);
	return 0;
}

int
_findterm(name, path, buf)
char *name;
struct term_path *path;
char *buf; {
	register char *s, *d;
	int r = 0;
	while(path->file != NULL) {
		switch(path->type) {
		case 0:
			r = _findterm2(name, path->file, buf);
			break;
		case 1:
			if (path->file[0] == '/') {
				r = _findterm2(name, path->file, buf);
			} else {
				s = path->file;
				d = buf;
				while(*s != '\0' && *s != ':')
					*d++ = *s++;
				*d = '\0';
				if (_tmatch(buf, name)) {
					while(*s != '\0')
						*d++ = *s++;
					return 1;
				}
				r = 0;
			}
			break;
		case 2:
			if (path->file[0] == '/') {
				r = _findterm2(name, path->file, buf);
			} else {
				s = path->file;
				d = buf;
				while(*s != '\0' && *s != ',')
					*d++ = *s++;
				*d = '\0';
				if (_tmatch(buf, name)) {
					while(*s != '\0')
						*d++ = *s++;
					return 2;
				}
				r = 0;
			}
			break;
		default:
			r = 0;
			break;
		}
		if (r == 1 || r == 2 || r == 3) {
#ifdef DEBUG
			printf("found in %s\n", path->file);
#endif
			break;
		}
		path++;
	}
	return r;
}
