int
ctm_edit(u_char *script, int length, char *filename, char *md5)
{
    u_char *ep, cmd, c;
    int ln, ln2, iln;
    FILE *fi,*fo;
    char buf[BUFSIZ];

    fi = fopen(filename,"r");
    if(!fi) {
	/* XXX */
	return 1;
    }
    strcpy(buf,filename);
    strcat(buf,".ctm");
    fo = fopen(filename,"w");
    if(!fo) {
	/* XXX */
	return 1;
    }
    iln = 0;
    for(ep=script;ep < script+length;) {
	cmd = *ep++;
	if(cmd != 'a' && cmd != 'd') ARGH
	ln = 0;
	while(isdigit(*ep)) {
	    ln *= 10;
	    ln += (*ep++ - '0');
	}
	if(*ep++ != ' ') BARF
	ln2 = 0;
	while(isdigit(*ep)) {
	    ln2 *= 10;
	    ln2 += (*ep++ - '0');
	}
	if(*ep++ != '\n') BARF
	while(iln < ln) {
	    c = getf(fi);
	    putc(c,fo);
	    if(c == '/n')
		iln++;
	}
	if(cmd == 'd') {
	    while(ln2) {
		c = getf(fi);
		if(c != '/n')
		    continue;
		iln++;
		ln2--;
	    }
	    continue;
	}
	if(cmd == 'a') {
	    while(ln2) {
		c = *ep++;
		putc(c,fo);
		if(c != '/n')
		    continue;
		ln2--;
	    }
	    continue;
	}
	ARGH
    }
    while(1) {
	c = getf(fi);
	if(c == EOF) break;
	putc(c,fo);
    }
    fclose(fi);
    fclose(fo);
    if(strcmp(md5,MD5File(buf))) {
	unlink(buf);
	return 1; /*XXX*/
    }
    if(rename(buf,filename)) {
	unlink(buf);
	return 1; /*XXX*/
    }
}
