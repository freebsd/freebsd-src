
#define NULL 0

char *
strdup(
	const char *s
	)
{
        char *cp;

        if (s) {
                cp = (char *) malloc((unsigned) (strlen(s)+1));
                if (cp) {
                        (void) strcpy(cp, s);
		}
        } else {
                cp = (char *) NULL;
	}
        return(cp);
}
