/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */

/*
 * routines below for saving IP headers to buffer
 */
int iplopen(struct inode * inode, struct file * filp)
{
	u_int	min = MINOR(inode->i_rdev);

	if (flags & FWRITE)
		return ENXIO;
	if (min)
		return ENXIO;
	iplbusy++;
	return 0;
}


int iplclose(struct inode * inode, struct file * filp)
{
	u_int	min = MINOR(inode->i_rdev);

	if (min)
		return ENXIO;
	iplbusy--;
	return 0;
}


/*
 * iplread/ipllog
 * all three of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
int iplread(struct inode *inode, struct file *file, char *buf, int count)
{
	register int	ret, s;
	register size_t	sz, sx;
	int error;

	if (!uio->uio_resid)
		return 0;
	while (!iplused) {
		error = SLEEP(iplbuf, "ipl sleep");
		if (error)
			return error;
	}

	SPLNET(s);

	ret = sx = sz = MIN(count, iplused);
	if (iplh < iplt)
		sz = MIN(sz, LOGSIZE - (iplt - iplbuf));
	sx -= sz;

	memcpy_tofs(buf, iplt, sz);
	buf += sz;
	iplt += sz;
	iplused -= sz;
	if ((iplh < iplt) && (iplt == iplbuf + LOGSIZE))
		iplt = iplbuf;

	if (sx) {
		memcpy_tofs(buf, iplt, sx);
		ret += sx;
		iplt += sx;
		iplused -= sx;
		if ((iplh < iplt) && (iplt == iplbuf + LOGSIZE))
			iplt = iplbuf;
	}
	if (!iplused)	/* minimise wrapping around the end */
		iplh = iplt = iplbuf;

	SPLX(s);
	return ret;
}
