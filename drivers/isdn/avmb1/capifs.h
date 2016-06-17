/* $Id: capifs.h,v 1.1.4.1 2001/11/20 14:19:34 kai Exp $
 * 
 * Copyright 2000 by Carsten Paeth <calle@calle.de>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

void capifs_new_ncci(char type, unsigned int num, kdev_t device);
void capifs_free_ncci(char type, unsigned int num);
