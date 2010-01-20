/*
Copyright (c) 2003-2006 Hewlett-Packard Development Company, L.P.
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include "uwx_env.h"
#include "uwx_swap.h"

void uwx_swap4(uint32_t *w)
{
    unsigned char *p;
    unsigned char t[4];

    p = (unsigned char *) w;

    t[0] = p[0];
    t[1] = p[1];
    t[2] = p[2];
    t[3] = p[3];

    p[0] = t[3];
    p[1] = t[2];
    p[2] = t[1];
    p[3] = t[0];
}

void uwx_swap8(uint64_t *dw)
{
    unsigned char *p;
    unsigned char t[8];

    p = (unsigned char *) dw;

    t[0] = p[0];
    t[1] = p[1];
    t[2] = p[2];
    t[3] = p[3];
    t[4] = p[4];
    t[5] = p[5];
    t[6] = p[6];
    t[7] = p[7];

    p[0] = t[7];
    p[1] = t[6];
    p[2] = t[5];
    p[3] = t[4];
    p[4] = t[3];
    p[5] = t[2];
    p[6] = t[1];
    p[7] = t[0];
}
