#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_pager.h>
#include <vm/vm_page.h>
#include <vm/default_pager.h>
#include <vm/swap_pager.h>

/*
 * pagerops for OBJT_DEFAULT - "default pager".
 */
struct pagerops defaultpagerops = {
	NULL,
	default_pager_alloc,
	default_pager_dealloc,
	default_pager_getpages,
	default_pager_putpages,
	default_pager_haspage,
	NULL
};

/*
 * no_pager_alloc just returns an initialized object.
 */
vm_object_t
default_pager_alloc(handle, size, prot, offset)
	void *handle;
	register vm_size_t size;
	vm_prot_t prot;
	vm_offset_t offset;
{
	if (handle != NULL)
		panic("default_pager_alloc: handle specified");

	return vm_object_allocate(OBJT_DEFAULT, offset + size);
}

void
default_pager_dealloc(object)
	vm_object_t object;
{
	/*
	 * OBJT_DEFAULT objects have no special resources allocated to them.
	 */
}

/*
 * The default pager has no backing store, so we always return
 * failure.
 */
int
default_pager_getpages(object, m, count, reqpage)
	vm_object_t object;
	vm_page_t *m;
	int count;
	int reqpage;
{
	return VM_PAGER_FAIL;
}

int
default_pager_putpages(object, m, c, sync, rtvals)
	vm_object_t object;
	vm_page_t *m;
	int c;
	boolean_t sync;
	int *rtvals;
{
	int i;

	/*
	 * Try to convert the object type into a OBJT_SWAP.
	 * If the swp structure allocation fails, convert it
	 * back to OBJT_DEFAULT and return failure. Otherwise
	 * pass this putpages to the swap pager.
	 */
	object->type = OBJT_SWAP;

	if (swap_pager_swp_alloc(object, M_KERNEL) != 0) {
		object->type = OBJT_DEFAULT;
		for (i = 0; i < c; i++)
			rtvals[i] = VM_PAGER_FAIL;
		return VM_PAGER_FAIL;
	}

	return swap_pager_putpages(object, m, c, sync, rtvals);
}

boolean_t
default_pager_haspage(object, offset, before, after)
	vm_object_t object;
	vm_offset_t offset;
	int *before;
	int *after;
{
	return FALSE;
}
