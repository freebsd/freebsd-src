/*
 * Expand a string, substituting any "%" with the current filename,
 * and any "#" with the previous filename and an initial "!" with 
 * the second string.
 */
char *
fexpand(s, t)
	char *s;
	char *t;
{
	extern char *current_file, *previous_file;
	register char *fr, *to;
	register int n;
	register char *e;

	if (*s == '\0')
		return ((char *) 0);
	/*
	 * Make one pass to see how big a buffer we 
	 * need to allocate for the expanded string.
	 */
	n = 0;
	for (fr = s;  *fr != '\0';  fr++)
	{
		switch (*fr)
		{
		case '!':
			if (s == fr && t == (char *) 0) {
				error("no previous command");
				return ((char *) 0);
			}
			n += (s == fr) ? strlen(t) : 1;
			break;
		case '%':
			n += (current_file != (char *) 0) ?  
			     strlen(current_file) : 1;
			break;
		case '#':
			n += (previous_file != (char *) 0) ?  
			     strlen(previous_file) : 1;
			break;
		default:
			n++;
			if (*fr == '\\') {
				n++;
				if (*++fr == '\0') {
					error("syntax error");
					return ((char *) 0);
				}
			}
			break;
		}
	}

	if ((e = (char *) calloc(n+1, sizeof(char))) == (char *) 0) {
		error("cannot allocate memory");
		quit();
	}

	/*
	 * Now copy the string, expanding any "%" or "#".
	 */
	to = e;
	for (fr = s;  *fr != '\0';  fr++)
	{
		switch (*fr)
		{
		case '!':
			if (s == fr) {
				strcpy(to, t);
				to += strlen(to);
			} else
				*to++ = *fr;
			break;
		case '%':
			if (current_file == (char *) 0)
				*to++ = *fr;
			else {
				strcpy(to, current_file);
				to += strlen(to);
			}
			break;
		case '#':
			if (previous_file == (char *) 0)
				*to++ = *fr;
			else {
				strcpy(to, previous_file);
				to += strlen(to);
			}
			break;
		default:
			*to++ = *fr;
			if (*fr == '\\')
				*to++ = *++fr;
			break;
		}
	}
	*to = '\0';
	return (e);
}
