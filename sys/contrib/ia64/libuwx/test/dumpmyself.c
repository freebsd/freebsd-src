/*
 * Copyright (c) 2002,2003 Hewlett-Packard Company
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "uwx.h"
#include "uwx_self.h"

struct uwx_env *uenv;
struct uwx_self_info *cbinfo;

extern int uwx_get_frame_info(struct uwx_env *uenv);

extern void dump_context(uint64_t *context);

int main(int argc, char **argv)
{
    int status;
    unsigned int *wp;
    uenv = uwx_init();
    printf("uwx_init returned %08x\n", uenv);
    cbinfo = uwx_self_init_info(uenv);
    status = uwx_register_callbacks(
		uenv,
		(intptr_t)cbinfo,
		uwx_self_copyin,
		uwx_self_lookupip);
    printf("uwx_register_callbacks returned %d\n", status);
    uwx_self_init_context(uenv);
    printf("In main():\n");
    dump_context((uint64_t *)uenv);
    (void) f1();
    uwx_free(uenv);
    return 0;
}

int f1(void)
{
    uwx_self_init_context(uenv);
    printf("In f1():\n");
    dump_context((uint64_t *)uenv);
    return f2();
}

int f2(void)
{
    uwx_self_init_context(uenv);
    printf("In f2():\n");
    dump_context((uint64_t *)uenv);
    return f3();
}

int f3(void)
{
    uwx_self_init_context(uenv);
    printf("In f3():\n");
    dump_context((uint64_t *)uenv);
    return f4();
}

int f4(void)
{
    int status;
    int foo[10];
    f5(foo);
    uwx_self_init_context(uenv);
    printf("In f4():\n");
    dump_context((uint64_t *)uenv);
    for (;;) {
	status = uwx_step(uenv);
	printf("uwx_step returned %d\n", status);
	if (status != UWX_OK)
	    break;
	printf("After step:\n");
	dump_context((uint64_t *)uenv);
    }
    return 0;
}

int f5(int *foo)
{
    foo[0] = 0;
    return 0;
}
