/* $XConsortium: AuWrite.c,v 1.6 94/04/17 20:15:45 gildea Exp $ */

/*

Copyright (c) 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id$");
#endif

#include <X11/Xauth.h>

static int
write_short (unsigned short s, FILE *file)
{
    unsigned char   file_short[2];

    file_short[0] = (s & (unsigned)0xff00) >> 8;
    file_short[1] = s & 0xff;
    if (fwrite (file_short, sizeof (file_short), 1, file) != 1)
	return 0;
    return 1;
}

static int
write_counted_string (unsigned short count, char *string, FILE *file)
{
    if (write_short (count, file) == 0)
	return 0;
    if (fwrite (string, (int) sizeof (char), (int) count, file) != count)
	return 0;
    return 1;
}

int
XauWriteAuth (FILE *auth_file, Xauth *auth)
{
    if (write_short (auth->family, auth_file) == 0)
	return 0;
    if (write_counted_string (auth->address_length, auth->address, auth_file) == 0)
	return 0;
    if (write_counted_string (auth->number_length, auth->number, auth_file) == 0)
	return 0;
    if (write_counted_string (auth->name_length, auth->name, auth_file) == 0)
	return 0;
    if (write_counted_string (auth->data_length, auth->data, auth_file) == 0)
	return 0;
    return 1;
}
