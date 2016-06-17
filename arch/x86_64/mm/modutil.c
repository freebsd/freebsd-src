/*  arch/x86_64/mm/modutil.c
 *
 *  Copyright (C) 1997,1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 *  Based upon code written by Linus Torvalds and others.
 * 
 *  Blatantly copied from sparc64 for x86-64 by Andi Kleen. 
 *  Should use direct mapping with 2MB pages. This would need extension
 *  of the kernel mapping.
 */
 
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/proto.h>

extern char _text[], _end[]; 

/* Kernel mapping to make the kernel alias visible to 
   /proc/kcore and /dev/mem 

   RED-PEN may want vsyscall mappings too 
 */

static struct vm_struct kernel_mapping = { 
	.addr = (void *)KERNEL_TEXT_START, 
}; 

void module_unmap (void * addr)
{
	struct vm_struct **p, *tmp;

	if (!addr)
		return;
	if ((PAGE_SIZE-1) & (unsigned long) addr) {
		printk("Trying to unmap module with bad address (%p)\n", addr);
		return;
	}
	write_lock(&vmlist_lock); 
	for (p = &vmlist ; (tmp = *p) ; p = &tmp->next) {
		if (tmp->addr == addr) {
			*p = tmp->next;
			write_unlock(&vmlist_lock); 
			vmfree_area_pages(VMALLOC_VMADDR(tmp->addr), tmp->size);
			kfree(tmp);
			return;
		}
	}
	printk("Trying to unmap nonexistent module vm area (%p)\n", addr);
}

void * module_map (unsigned long size)
{
	void * addr;
	struct vm_struct **p, *tmp, *area;

	size = PAGE_ALIGN(size);
	if (!size || size > MODULES_LEN) return NULL;
		
	area = (struct vm_struct *) kmalloc(sizeof(*area), GFP_KERNEL);
	if (!area) return NULL; 

	memset(area, 0, sizeof(struct vm_struct));

	size = round_up(size, PAGE_SIZE); 

	addr = (void *) MODULES_VADDR;
	write_lock(&vmlist_lock); 
	for (p = &vmlist; (tmp = *p) ; p = &tmp->next) {
		void *next;
		if (size + (unsigned long) addr < (unsigned long) tmp->addr)
			break;
		next = (void *) (tmp->size + (unsigned long) tmp->addr);
		if (next > addr)
			addr = next;
	}
	if ((unsigned long) addr + size >= MODULES_END) { 
		write_unlock(&vmlist_lock); 
		kfree(area);
		return NULL;
	}
	
	area->size = size;
	area->addr = addr;
	area->next = *p;
	*p = area;
	write_unlock(&vmlist_lock); 

	if (vmalloc_area_pages(VMALLOC_VMADDR(addr), size, GFP_KERNEL, PAGE_KERNEL_EXECUTABLE)) {
		module_unmap(addr);
		return NULL;
	}
	return addr;
}

static int __init mod_vmlist_init(void)
{
	struct vm_struct *vm, **base;
	write_lock(&vmlist_lock); 
	for (base = &vmlist, vm = *base; vm; base = &vm->next, vm = *base) { 
		if (vm->addr > (void *)KERNEL_TEXT_START) 
			break; 
	}  
	kernel_mapping.size = _end - _text;
	kernel_mapping.next = vm; 
	*base = &kernel_mapping; 
	write_unlock(&vmlist_lock); 
	return 0;
} 

__initcall(mod_vmlist_init);
