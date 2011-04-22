/*-
 * Copyright (c) 2006 Kip Macy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#ifdef DEBUG
#include <sys/kdb.h>
#endif
#include <vm/vm.h> 
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_extern.h>
#include <vm/uma.h> 

#include <machine/cpufunc.h>
#include <machine/hypervisorvar.h>
#include <machine/smp.h>
#include <machine/mmu.h>
#include <machine/tte.h>
#include <machine/vmparam.h>
#include <machine/tlb.h>
#include <machine/tte_hash.h>

#define HASH_SIZE        (1 << HASH_ENTRY_SHIFT)
#define HASH_MASK(th)    ((1<<(th->th_shift+PAGE_SHIFT-THE_SHIFT))-1)
#define NULL_TAG         0
#define MAGIC_VALUE      0xcafebabe

struct tte_hash_entry;
struct of_field;

#define MAX_FRAGMENT_ENTRIES ((PAGE_SIZE / sizeof(struct tte_hash_entry)) - 1)

typedef struct tte_hash_field_ {
	uint64_t tag;
	uint64_t data;
} tte_hash_field, *tte_hash_field_t;

struct of_field {
	int16_t          count;
	uint8_t          lock;
	uint8_t          pad;
	uint32_t         flags;
	struct tte_hash_entry *next;
};

typedef struct tte_hash_entry {
	tte_hash_field the_fields[HASH_ENTRIES];
	struct of_field of;
} *tte_hash_entry_t;

struct fragment_header {
	struct tte_hash_fragment *fh_next;
	uint8_t fh_count;
	uint8_t fh_free_head;
	uint8_t pad[sizeof(struct tte_hash_entry) - 10];
};

CTASSERT(sizeof(struct fragment_header) == sizeof(struct tte_hash_entry));

SLIST_HEAD(tte_hash_list, tte_hash); 

struct tte_hash_list hash_free_list[PAGE_SHIFT];

struct tte_hash {
	uint16_t th_shift;              /* effective size in pages */
	uint16_t th_context;            /* TLB context   */
	uint32_t th_entries;            /* # pages held  */
	tte_hash_entry_t th_hashtable;  /* hash of TTEs  */
	struct tte_hash_fragment *th_fhhead;
	struct tte_hash_fragment *th_fhtail;
	SLIST_ENTRY(tte_hash) th_next;
};

struct tte_hash_fragment {
	struct fragment_header thf_head;
	struct tte_hash_entry  thf_entries[MAX_FRAGMENT_ENTRIES];
};

CTASSERT(sizeof(struct tte_hash_fragment) == PAGE_SIZE);


static struct tte_hash kernel_tte_hash;
/*
 * Data for the tte_hash allocation mechanism
 */
static uma_zone_t thzone;
static struct vm_object thzone_obj;
static int tte_hash_count = 0, tte_hash_max = 0;

extern uint64_t hash_bucket_lock(tte_hash_field_t fields);
extern void hash_bucket_unlock(tte_hash_field_t fields, uint64_t s);

static tte_hash_t
get_tte_hash(void)
{
	tte_hash_t th;

	th = uma_zalloc(thzone, M_NOWAIT);

	KASSERT(th != NULL, ("tte_hash allocation failed"));
	tte_hash_count++;
	return th;

}

static __inline void
free_tte_hash(tte_hash_t th)
{
	tte_hash_count--;
	uma_zfree(thzone, th);
}

static tte_hash_t
tte_hash_cached_get(int shift)
{
	tte_hash_t th;
	struct tte_hash_list *head;

	th = NULL;
	head = &hash_free_list[shift];
	if (!SLIST_EMPTY(head)) {
		th = SLIST_FIRST(head);
		SLIST_REMOVE_HEAD(head, th_next);
	}
	return (th);
}

static void
tte_hash_cached_free(tte_hash_t th)
{
	th->th_context = 0xffff;
	SLIST_INSERT_HEAD(&hash_free_list[th->th_shift - HASH_ENTRY_SHIFT], th, th_next);
}

void 
tte_hash_init(void)
{
	int i;

	thzone = uma_zcreate("TTE_HASH", sizeof(struct tte_hash), NULL, NULL, 
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM | UMA_ZONE_NOFREE);
	tte_hash_max = maxproc;
	uma_zone_set_obj(thzone, &thzone_obj, tte_hash_max);
	for (i = 0; i < PAGE_SHIFT; i++)
		SLIST_INIT(&hash_free_list[i]); 
}

tte_hash_t
tte_hash_kernel_create(vm_offset_t va, uint16_t shift, vm_paddr_t fragment_page)
{
	tte_hash_t th;
		
	th = &kernel_tte_hash;
	th->th_shift = shift;
	th->th_entries = 0;
	th->th_context = 0;
	th->th_hashtable = (tte_hash_entry_t)va;
	th->th_fhtail = th->th_fhhead = (void *)TLB_PHYS_TO_DIRECT(fragment_page);

	return (th);
}

static inline void *
alloc_zeroed_page(void)
{
	vm_page_t m;
	static int color;
	void *ptr;

	m = NULL;

	while (m == NULL) {
		m = vm_page_alloc(NULL, color++,
		    VM_ALLOC_NORMAL | VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
		    VM_ALLOC_ZERO);

		if (m == NULL) 
			VM_WAIT;
	}

	if ((m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);

	ptr = (void *)TLB_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(m));
	return (ptr);
}

static inline void
free_fragment_pages(void *ptr)
{
	struct tte_hash_fragment *fh;
	vm_page_t m;
	
        for (fh = ptr; fh != NULL; fh = fh->thf_head.fh_next) {
                m = PHYS_TO_VM_PAGE(TLB_DIRECT_TO_PHYS((vm_offset_t)fh));
                m->wire_count--;
		atomic_subtract_int(&cnt.v_wire_count, 1);
                vm_page_free(m);
        }
}

static inline tte_hash_t
_tte_hash_create(uint64_t context, uint64_t *scratchval, uint16_t shift)
{
	tte_hash_t th;
	
	th = get_tte_hash();
	th->th_shift = shift;
	th->th_entries = 0;
	th->th_context = (uint16_t)context;

	th->th_hashtable = pmap_alloc_zeroed_contig_pages((1 << shift), PAGE_SIZE);

	th->th_fhtail = th->th_fhhead = alloc_zeroed_page();
	KASSERT(th->th_fhtail != NULL, ("th->th_fhtail == NULL"));
	
	if (scratchval)
		*scratchval = (uint64_t)((vm_offset_t)th->th_hashtable) | ((vm_offset_t)(1 << shift));

	return (th);
}


tte_hash_t
tte_hash_create(uint64_t context, uint64_t *scratchval)
{
	return (_tte_hash_create(context, scratchval, HASH_ENTRY_SHIFT));
}

void
tte_hash_destroy(tte_hash_t th)
{
	tte_hash_cached_free(th);
}

static void
_tte_hash_reset(tte_hash_t th)
{
	
	free_fragment_pages(th->th_fhhead->thf_head.fh_next);

	th->th_fhtail = th->th_fhhead;
	hwblkclr(th->th_fhhead, PAGE_SIZE); 
#if 0
	if (th->th_entries != 0) 
#endif
		hwblkclr(th->th_hashtable, (1 << (th->th_shift + PAGE_SHIFT)));
	th->th_entries = 0;
}

tte_hash_t
tte_hash_reset(tte_hash_t th, uint64_t *scratchval)
{
	tte_hash_t newth;

	if (th->th_shift != HASH_ENTRY_SHIFT && (newth = tte_hash_cached_get(0)) != NULL) {
		newth->th_context = th->th_context;
		tte_hash_cached_free(th);
		*scratchval = (uint64_t)((vm_offset_t)newth->th_hashtable) | ((vm_offset_t)HASH_SIZE);
	} else {
		newth = th;
	}
	_tte_hash_reset(newth);

	return (newth);
}

static __inline void
tte_hash_set_field(tte_hash_field_t field, uint64_t tag, tte_t tte)
{
	field->tag = tag;
	field->data = tte | (field->data & VTD_LOCK);
}

static __inline tte_hash_entry_t 
find_entry(tte_hash_t th, vm_offset_t va, int page_shift)
{
	uint64_t hash_index;

	hash_index = (va >> page_shift) & HASH_MASK(th);
	return (&th->th_hashtable[hash_index]);
}

static __inline tte_hash_entry_t 
tte_hash_lookup_last_entry(tte_hash_entry_t entry)
{

	while (entry->of.next) 
		entry = entry->of.next;

	return (entry);
}

static tte_hash_entry_t 
tte_hash_allocate_fragment_entry(tte_hash_t th)
{
	struct tte_hash_fragment *fh;
	tte_hash_entry_t newentry;
	
	fh = th->th_fhtail;
	if (fh->thf_head.fh_free_head == MAX_FRAGMENT_ENTRIES) {
		fh = th->th_fhtail = fh->thf_head.fh_next = alloc_zeroed_page();
		fh->thf_head.fh_free_head = 1;
#ifdef NOISY_DEBUG
		printf("new fh=%p \n", fh);
#endif
	} 
	newentry = &fh->thf_entries[fh->thf_head.fh_free_head];

	fh->thf_head.fh_free_head++;
	fh->thf_head.fh_count++; 

	return (newentry);
}

/*
 * if a match for va is found the tte value is returned 
 * and if field is non-null field will point to that entry
 * 
 * 
 */
static __inline tte_t 
_tte_hash_lookup(tte_hash_entry_t entry, tte_t tte_tag, tte_hash_field_t *field)
{
	int i;
	tte_t tte_data;
	tte_hash_field_t fields;

	tte_data = 0;
	do { 
		fields = entry->the_fields;
		for (i = 0; i < entry->of.count; i++) {
			if (fields[i].tag == tte_tag) {
				tte_data = (fields[i].data & ~VTD_LOCK);
				*field = &fields[i];
				goto done;
			}
		}
#ifdef DEBUG
	if (entry->of.next && entry->of.flags != MAGIC_VALUE)
		panic("overflow pointer not null without flags set entry= %p next=%p flags=0x%x count=%d", 
		      entry, entry->of.next, entry->of.flags, entry->of.count);
#endif
		entry = entry->of.next;
	} while (entry);

done:
	return (tte_data);
}


static __inline void
_tte_hash_lookup_last(tte_hash_entry_t entry, tte_hash_field_t *field)
{

	tte_hash_field_t fields;

	fields = entry->the_fields;

	while (entry->of.next && (entry->of.next->of.count > 1))
		entry = entry->of.next;

	if (entry->of.next && entry->of.next->of.count == 1) {
		*field = &entry->of.next->the_fields[0];
		entry->of.next = NULL;
		entry->of.flags = 0;
	} else {
#ifdef DEBUG
		if (entry->of.count == 0)
			panic("count zero");
#endif
		*field = &entry->the_fields[--entry->of.count];
	}
}

tte_t
tte_hash_clear_bits(tte_hash_t th, vm_offset_t va, uint64_t flags)
{
	uint64_t s;
	tte_hash_entry_t entry;
	tte_t otte_data, tte_tag;
	tte_hash_field_t field = NULL;

	/* XXX - only handle 8K pages for now */
	entry = find_entry(th, va, PAGE_SHIFT);

	tte_tag = (((uint64_t)th->th_context << TTARGET_CTX_SHIFT)|(va >> TTARGET_VA_SHIFT));
	
	s = hash_bucket_lock(entry->the_fields);
	if((otte_data = _tte_hash_lookup(entry, tte_tag, &field)) != 0)
		tte_hash_set_field(field, field->tag, field->data & ~flags);
	hash_bucket_unlock(entry->the_fields, s);
	return (otte_data);
}

tte_t
tte_hash_delete(tte_hash_t th, vm_offset_t va)
{
	uint64_t s;
	tte_hash_entry_t entry;
	tte_t tte_data, tte_tag;
	tte_hash_field_t lookup_field = NULL; 
	tte_hash_field_t last_field = NULL;

	/* XXX - only handle 8K pages for now */
	entry = find_entry(th, va, PAGE_SHIFT);

	tte_tag = (((uint64_t)th->th_context << TTARGET_CTX_SHIFT)|(va >> TTARGET_VA_SHIFT));

	s  = hash_bucket_lock(entry->the_fields);
	
	if ((tte_data = _tte_hash_lookup(entry, tte_tag, &lookup_field)) == 0) 
		goto done;

	_tte_hash_lookup_last(entry, &last_field);

#ifdef DEBUG
	if (last_field->tag == 0) {
		hash_bucket_unlock(entry->the_fields, s);
		panic("lookup_last failed for va=0x%lx\n", va);
	}
#endif
	/* move last field's values in to the field we are deleting */
	if (lookup_field != last_field) 
		tte_hash_set_field(lookup_field, last_field->tag, last_field->data);
	
	tte_hash_set_field(last_field, 0, 0);
done:	
	hash_bucket_unlock(entry->the_fields, s);
	if (tte_data) 
		th->th_entries--;

	return (tte_data);
}

static __inline int 
tte_hash_insert_locked(tte_hash_t th, tte_hash_entry_t entry, uint64_t tte_tag, tte_t tte_data)
{
	tte_hash_entry_t lentry;

	lentry = tte_hash_lookup_last_entry(entry);

	if (lentry->of.count == HASH_ENTRIES) 
		return -1;
	tte_hash_set_field(&lentry->the_fields[lentry->of.count++], 
			   tte_tag, tte_data);
	th->th_entries++;
	return (0);
}

static __inline void
tte_hash_extend_locked(tte_hash_t th, tte_hash_entry_t entry, tte_hash_entry_t newentry, uint64_t tte_tag, tte_t tte_data)
{
	tte_hash_entry_t lentry;

	lentry = tte_hash_lookup_last_entry(entry);
	lentry->of.flags = MAGIC_VALUE;
	lentry->of.next = newentry;
	tte_hash_set_field(&newentry->the_fields[newentry->of.count++], tte_tag, tte_data);
	th->th_entries++;
}

void
tte_hash_insert(tte_hash_t th, vm_offset_t va, tte_t tte_data)
{

	tte_hash_entry_t entry, newentry;
	tte_t tte_tag;
	uint64_t s;
	int retval;
	
#ifdef DEBUG
	if (tte_hash_lookup(th, va) != 0) 
		panic("mapping for va=0x%lx already exists", va);
#endif
	entry = find_entry(th, va, PAGE_SHIFT); /* should actually be a function of tte_data */
	tte_tag = (((uint64_t)th->th_context << TTARGET_CTX_SHIFT)|(va >> TTARGET_VA_SHIFT));

	s = hash_bucket_lock(entry->the_fields);
	retval = tte_hash_insert_locked(th, entry, tte_tag, tte_data);
	hash_bucket_unlock(entry->the_fields, s);

	if (retval == -1) {
		newentry = tte_hash_allocate_fragment_entry(th); 
		s = hash_bucket_lock(entry->the_fields);
		tte_hash_extend_locked(th, entry, newentry, tte_tag, tte_data);
		hash_bucket_unlock(entry->the_fields, s);
	}

#ifdef DEBUG
	if (tte_hash_lookup(th, va) == 0) 
		panic("insert for va=0x%lx failed", va);
#endif
}

/* 
 * If leave_locked is true the tte's data field will be returned to
 * the caller with the hash bucket left locked
 */
tte_t 
tte_hash_lookup(tte_hash_t th, vm_offset_t va)
{
	uint64_t s;
	tte_hash_entry_t entry;
	tte_t tte_data, tte_tag;
	tte_hash_field_t field = NULL;
	/* XXX - only handle 8K pages for now */
	entry = find_entry(th, va, PAGE_SHIFT);

	tte_tag = (((uint64_t)th->th_context << TTARGET_CTX_SHIFT)|(va >> TTARGET_VA_SHIFT));

	s = hash_bucket_lock(entry->the_fields);
	tte_data = _tte_hash_lookup(entry, tte_tag, &field);
	hash_bucket_unlock(entry->the_fields, s);
	
	return (tte_data);
}

uint64_t
tte_hash_set_scratchpad_kernel(tte_hash_t th)
{
	
	uint64_t hash_scratch;
	/* This breaks if a hash table grows above 32MB
	 */
	hash_scratch = ((vm_offset_t)th->th_hashtable) | ((vm_offset_t)(1<<th->th_shift));
	set_hash_kernel_scratchpad(hash_scratch);
	
	return (hash_scratch);
}

uint64_t
tte_hash_set_scratchpad_user(tte_hash_t th, uint64_t context)
{

	uint64_t hash_scratch;
	/* This breaks if a hash table grows above 32MB
	 */
	th->th_context = (uint16_t)context;
	hash_scratch = ((vm_offset_t)th->th_hashtable) | ((vm_offset_t)(1<<th->th_shift));
	set_hash_user_scratchpad(hash_scratch);
	
	return (hash_scratch);
}

tte_t
tte_hash_update(tte_hash_t th, vm_offset_t va, tte_t tte_data)
{

	uint64_t s;
	tte_hash_entry_t entry;
	tte_t otte_data, tte_tag;
	tte_hash_field_t field = NULL;

	entry = find_entry(th, va, PAGE_SHIFT); /* should actually be a function of tte_data */

	tte_tag = (((uint64_t)th->th_context << TTARGET_CTX_SHIFT)|(va >> TTARGET_VA_SHIFT));
	s = hash_bucket_lock(entry->the_fields);
	otte_data = _tte_hash_lookup(entry, tte_tag, &field);

	if (otte_data == 0) {
		hash_bucket_unlock(entry->the_fields, s);
		tte_hash_insert(th, va, tte_data);
	} else {
		tte_hash_set_field(field, tte_tag, tte_data);
		hash_bucket_unlock(entry->the_fields, s);
	}
	return (otte_data);
}

/*
 * resize when the average entry has a full fragment entry
 */
int
tte_hash_needs_resize(tte_hash_t th)
{
	return ((th->th_entries > (1 << (th->th_shift + PAGE_SHIFT - TTE_SHIFT + 1))) 
		&& (th != &kernel_tte_hash));
}

tte_hash_t
tte_hash_resize(tte_hash_t th)
{
	int i, j, nentries;
	tte_hash_t newth;
	tte_hash_entry_t src_entry, dst_entry, newentry;

	KASSERT(th != &kernel_tte_hash,("tte_hash_resize not supported for this pmap"));
	if ((newth = tte_hash_cached_get((th->th_shift - HASH_ENTRY_SHIFT) + 1)) != NULL) {
		newth->th_context = th->th_context;
		_tte_hash_reset(newth);
	} else {
		newth = _tte_hash_create(th->th_context, NULL, (th->th_shift + 1));
	}

	nentries = (1 << (th->th_shift + PAGE_SHIFT - THE_SHIFT));
	for (i = 0; i < nentries; i++) {
		tte_hash_field_t fields;
		src_entry = (&th->th_hashtable[i]);
		do {
			fields = src_entry->the_fields;
			for (j = 0; j < src_entry->of.count; j++) {
				int shift = TTARGET_VA_SHIFT - PAGE_SHIFT;
				uint64_t index = ((fields[j].tag<<shift) | (uint64_t)(i&((1<<shift)-1))) & HASH_MASK(newth);	
				dst_entry = &(newth->th_hashtable[index]);
				if (tte_hash_insert_locked(newth, dst_entry, fields[j].tag, fields[j].data) == -1) {
					newentry = tte_hash_allocate_fragment_entry(newth); 
					tte_hash_extend_locked(newth, dst_entry, newentry, fields[j].tag, fields[j].data);
				}
			}		 
			src_entry = src_entry->of.next;
		} while (src_entry);
	}

	KASSERT(th->th_entries == newth->th_entries, 
		("not all entries copied old=%d new=%d", th->th_entries, newth->th_entries));
	
	return (newth);
}
