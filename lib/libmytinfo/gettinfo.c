/*
 * gettinfo.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:02
 *
 */

#include "defs.h"
#include <term.h>

#include <ctype.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo gettinfo.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

int _gettinfo(sp, cur, path)
register char *sp;
struct term_path *path;
TERMINAL *cur; {
	static char name[MAX_NAME];
	static char entry[MAX_LINE];
	register char *dp;
	register TERMINAL *ct = cur;
	int ind, i, base;
	char c, flag;

	dp = entry;
	while(*sp != ',' && *sp != '\0')
		*dp++ = *sp++;
	if (ct->name_all == NULL) {
		*dp = '\0';
		if ((ct->name_all = _addstr(entry)) == NULL)
			return 1;
		while(dp > entry && *--dp != '|');
		ct->name_long = ct->name_all + (dp - entry) + 1;
	}

 	while(*sp != '\0') {
		while(*sp == ',')
			sp++;
		while(isspace(*sp))
			sp++;
		if (*sp == ',')
			continue;
		if (*sp == '\0')
			break;
		dp = name;
		while (*sp != ',' && *sp != '#' && *sp != '=' && !isspace(*sp) && *sp != '\0')
			*dp++ = *sp++;
		*dp = '\0';
#ifdef DEBUG
		printf(" %s", name);
#endif
		switch(*sp) {
		case '=':
#ifdef DEBUG
			putchar('$');
#endif
			dp = entry;
			sp++;
			ind = _findstrname(name);
			if (ind != -1)
				flag = _strflags[ind];
			else {
				ind = _findstrfname(name);
				if (ind != -1)
					flag = _strflags[ind];
				else
					flag = 'K';
			}
			while(*sp != '\0' && *sp != ',') {
				switch(*sp) {
				case '%':
					*dp++ = *sp++;
					if (*sp == '^' && flag == 'G')
						*dp++ = *sp++;
					break;
				case '\\':
					switch(*++sp) {
					case 'e':
					case 'E': *dp++ = '\033'; break;
					case 'l': *dp++ = '\012'; break;
					case 'n': *dp++ = '\n'; break;
					case 'r': *dp++ = '\r'; break;
					case 't': *dp++ = '\t'; break;
					case 'b': *dp++ = '\b'; break;
					case 'f': *dp++ = '\f'; break;
					case 's': *dp++ = ' '; break;

					case '^':
					case '\\':
					case ',':
					case ':':
						*dp++ = *sp; break;

					case '0':
						if (!isdigit(*(sp + 1))) {
							*dp++ = '\200';
							break;
						}
						/* FALLTHROUGH */
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
						c = *sp - '0';
						if (isdigit(sp[1]))
							c = c * 8 + (*++sp-'0');
						if (isdigit(sp[1]))
							c = c * 8 + (*++sp-'0');
						switch((char)c) {
						case 0:
							if (flag == 'K')
								*dp++ = '\200';
							else {
								*dp++ = '\\';
								*dp++ = '0';
								*dp++ = '0';
								*dp++ = '0';
							}
							break;
						case '$':
						case '\'':
						case '\\':
							if (flag != 'K')
								*dp++ = '\\';
							/* FALLTHROUGH */
						default:
							if (flag == 'G'
							    && c == '%')
								*dp++ = '\\';
							*dp++ = c;
							break;
						}
						break;
					default:
						*dp++ = '\\';
						*dp++ = *sp;
						break;
					}
					sp++;
					break;
				case '^':
					if (*++sp >= 'A' && *sp <= '_') {
						*dp++ = *sp++ - '@';
					} else if (*sp >= 'a' && *sp <= 'z') {
						*dp++ = *sp++ - 'a' + 1;
					} else if (*sp == '@') {
						if (flag == 'K')
							*dp++ = '\200';
						else {
							*dp++ = '\\';
							*dp++ = '0';
							*dp++ = '0';
							*dp++ = '0';
						}
						sp++;
					} else
						*dp++ = '^';
					break;
				default:
					*dp++ = *sp++;
					break;
				}
			}
			*dp++ = '\0';
			if (strcmp(name, "use") == 0) {
				if (_getother(entry, path, ct))
					return 1;
				break;
			}
			if (ind == -1)
				break;
			if (ct->strs[ind] != (char *) -1)
				break;
			if ((ct->strs[ind] = _addstr(entry)) == NULL)
				return 1;
			break;
		case '#':
#ifdef DEBUG
			putchar('#');
#endif
#if 0
			i = atoi(++sp);
#else
			sp++;
			if (*sp == '0') {
				sp++;
				if (*sp == 'x' || *sp == 'X') {
					sp++;
					base = 16;
				} else
					base = 8;
			} else
				base = 10;
			i = 0;
			while(1) {
				if (isdigit(*sp))
					i = i * base + *sp - '0';
				else if (base == 16 && isxdigit(*sp))
					i = i * base + tolower(*sp) - 'a' + 10;
				else
					break;
				sp++;
			}
#endif
			while(*sp != '\0' && *sp != ',')
				sp++;

			ind = _findnumname(name);
			if (ind != -1) {
				if (ct->nums[ind] != -2)
					break;
				ct->nums[ind] = i;
			} else {
				ind = _findnumfname(name);
				if (ind != -1) {
					if (ct->nums[ind] != -2)
						break;
					ct->nums[ind] = i;
				}
			}
			break;
		default:
			while(*sp != '\0' && *sp++ != ',');
			if (*(dp - 1) == '@') {
#ifdef DEBUG
				putchar('@');
#endif
				*(dp - 1) = '\0';
				ind = _findboolname(name);
				if (ind != -1) {
#ifdef DEBUG
					putchar('!');
#endif
					ct->bools[ind] = 0;
					break;
				}
				ind = _findnumname(name);
				if (ind != -1) {
#ifdef DEBUG
					putchar('#');
#endif
					ct->nums[ind] = -1;
					break;
				}
				ind = _findstrname(name);
				if (ind != -1) {
#ifdef DEBUG
					putchar('$');
#endif
					ct->strs[ind] = NULL;
					break;
				}
				ind = _findboolfname(name);
				if (ind != -1) {
#ifdef DEBUG
					putchar('!');
#endif
					ct->bools[ind] = 0;
					break;
				}
				ind = _findnumfname(name);
				if (ind != -1) {
#ifdef DEBUG
					putchar('#');
#endif
					ct->nums[ind] = -1;
					break;
				}
				ind = _findstrfname(name);
				if (ind != -1) {
#ifdef DEBUG
					putchar('$');
#endif
					ct->strs[ind] = NULL;
					break;
				}
				break;
			}
#ifdef DEBUG
			putchar('!');
#endif
			ind = _findboolname(name);
			if (ind == -1) {
				ind = _findboolfname(name);
				if (ind == -1)
					break;
			}
			if (ct->bools[ind] != -1)
				break;
			ct->bools[ind] = 1;
			break;
		}
	}
	return 0;
}
