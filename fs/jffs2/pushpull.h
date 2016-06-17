/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * The original JFFS, from which the design for JFFS2 was derived,
 * was designed and implemented by Axis Communications AB.
 *
 * The contents of this file are subject to the Red Hat eCos Public
 * License Version 1.1 (the "Licence"); you may not use this file
 * except in compliance with the Licence.  You may obtain a copy of
 * the Licence at http://www.redhat.com/
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.
 *
 * The Original Code is JFFS2 - Journalling Flash File System, version 2
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the RHEPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the RHEPL or the GPL.
 *
 * $Id: pushpull.h,v 1.5 2001/09/23 10:04:15 rmk Exp $
 *
 */

#ifndef __PUSHPULL_H__
#define __PUSHPULL_H__
struct pushpull {
	unsigned char *buf;
	unsigned int buflen;
	unsigned int ofs;
	unsigned int reserve;
};

void init_pushpull(struct pushpull *, char *, unsigned, unsigned, unsigned);
int pushbit(struct pushpull *pp, int bit, int use_reserved);
int pushedbits(struct pushpull *pp);

static inline int pullbit(struct pushpull *pp)
{
	int bit;

	bit = (pp->buf[pp->ofs >> 3] >> (7-(pp->ofs & 7))) & 1;

	pp->ofs++;
	return bit;
}

static inline int pulledbits(struct pushpull *pp)
{
	return pp->ofs;
}

#endif /* __PUSHPULL_H__ */
