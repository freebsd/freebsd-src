#include "uwx.h"
#include "uwx_self.h"

struct uwx_env *uenv;
struct uwx_self_info *cbinfo;

extern int uwx_get_frame_info(struct uwx_env *uenv);

extern void dump_context(uint64_t *context);

extern void prime_registers();

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
    prime_registers();
    uwx_free(uenv);
    return 0;
}

int func1(void)
{
    uwx_self_init_context(uenv);
    printf("In func1():\n");
    dump_context((uint64_t *)uenv);
    return func2();
}

int func2(void)
{
    uwx_self_init_context(uenv);
    printf("In func2():\n");
    dump_context((uint64_t *)uenv);
    return func3();
}

int func3(void)
{
    uwx_self_init_context(uenv);
    printf("In func3():\n");
    dump_context((uint64_t *)uenv);
    return func4();
}

int func4(void)
{
    int status;
    int foo[10];
    uint64_t *p;
    uint64_t disp;
    uint64_t val;

    func5(foo);
    uwx_self_init_context(uenv);
    uwx_init_history(uenv);
    printf("In func4():\n");
    dump_context((uint64_t *)uenv);
    for (;;) {
	status = uwx_step(uenv);
	if (status != UWX_OK) {
	    printf("uwx_step returned %d\n", status);
	    break;
	}
	status = uwx_get_reg(uenv, UWX_REG_PFS, &val);
	if (status != UWX_OK) {
	    printf("uwx_get_reg returned %d\n", status);
	    break;
	}
	printf("After step:\n");
	dump_context((uint64_t *)uenv);
	status = uwx_get_spill_loc(uenv, UWX_REG_IP, &disp);
	if (status == UWX_OK) {
	    p = (uint64_t *)(disp & ~0x7LL);
	    if ((disp & 0x7) == UWX_DISP_RSTK(0))
		printf("IP spilled to backing store %08x = %08x\n",
						    (int)p, (int)(*p));
	    else if ((disp & 0x7) == UWX_DISP_MSTK(0))
		printf("IP spilled to mem stack %08x = %08x\n",
						    (int)p, (int)(*p));
	    else if ((disp & 0x7) == UWX_DISP_REG(0))
		printf("IP found in register %08x\n", (int)disp >> 4);
	    else
		printf("IP history not available\n");
	}
    }
    return 0;
}

int func5(int *foo)
{
    foo[0] = 0;
    return 0;
}
