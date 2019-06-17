#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/cprng.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/filedesc.h>

#include <sys/kasan.h>

/* Shadow layout customization. */
#define SHADOW_BYTES_PER_BLOCK 1
#define SHADOW_BLOCKS_PER_ROW 16
#define SHADOW_BYTES_PER_ROW (SHADOW_BLOCKS_PER_ROW * SHADOW_BYTES_PER_BLOCK)
#define SHADOW_ROWS_AROUND_ADDR 2
#define _RET_IP_      (unsigned long)__builtin_return_address(0)

#define TASK_SIZE 30 //Temp

#define panic printf
//Typedefs for current version
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef int8_t __s8;
typedef int16_t __s16;
typedef int32_t __s32;
typedef int64_t __s64;

typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;

typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;

//End of typedefs

static const void *find_first_bad_addr(const void *addr, size_t size)
{
	//u8 shadow_val = *(u8 *)kasan_mem_to_shadow(addr);
	const void *first_bad_addr = addr;

/*	while (!shadow_val && first_bad_addr < (const char *)addr + size) {
		(const char *)first_bad_addr += KASAN_SHADOW_SCALE_SIZE;
		shadow_val = *(u8 *)kasan_mem_to_shadow(first_bad_addr);
	}
*/	return first_bad_addr;
}

static bool addr_has_shadow(struct kasan_bug_info *info)
{
	return (info->access_addr >=
		kasan_shadow_to_mem((void *)KASAN_SHADOW_START));
}

static const char *get_shadow_bug_type(struct kasan_bug_info *info)
{
	const char *bug_type = "unknown-crash";
	u8 *shadow_addr;

	info->first_bad_addr = find_first_bad_addr(info->access_addr,
						info->access_size);

	shadow_addr = (u8 *)kasan_mem_to_shadow(info->first_bad_addr);

	/*
	 * If shadow byte value is in [0, KASAN_SHADOW_SCALE_SIZE) we can look
	 * at the next shadow byte to determine the type of the bad access.
	 */
	if (*shadow_addr > 0 && *shadow_addr <= KASAN_SHADOW_SCALE_SIZE - 1)
	shadow_addr++;

	switch (*shadow_addr) {
	case 0 ... KASAN_SHADOW_SCALE_SIZE - 1:
		/*
		 * In theory it's still possible to see these shadow values
		 * due to a data race in the kernel code.
		 */
		bug_type = "out-of-bounds";
		break;
	case KASAN_PAGE_REDZONE:
	case KASAN_KMALLOC_REDZONE:
		bug_type = "slab-out-of-bounds";
		break;
	case KASAN_GLOBAL_REDZONE:
		bug_type = "global-out-of-bounds";
		break;
	case KASAN_STACK_LEFT:
	case KASAN_STACK_MID:
	case KASAN_STACK_RIGHT:
	case KASAN_STACK_PARTIAL:
		bug_type = "stack-out-of-bounds";
		break;
	case KASAN_FREE_PAGE:
	case KASAN_KMALLOC_FREE:
		bug_type = "use-after-free";
		break;
	case KASAN_USE_AFTER_SCOPE:
		bug_type = "use-after-scope";
		break;
	case KASAN_ALLOCA_LEFT:
	case KASAN_ALLOCA_RIGHT:
		bug_type = "alloca-out-of-bounds";
		break;
	}

	return bug_type;
}

static const char *get_wild_bug_type(struct kasan_bug_info *info)
{
	const char *bug_type = "unknown-crash";

	if ((unsigned long)info->access_addr < PAGE_SIZE)
		bug_type = "null-ptr-deref";
	else if ((unsigned long)info->access_addr < TASK_SIZE)
		bug_type = "user-memory-access";
	else
		bug_type = "wild-memory-access";

	return bug_type;
}

static const char *get_bug_type(struct kasan_bug_info *info)
{
	if (addr_has_shadow(info))
		return get_shadow_bug_type(info);
	return get_wild_bug_type(info);
}

static void print_error_description(struct kasan_bug_info *info)
{
	const char *bug_type = get_bug_type(info);

	snprintf(info->bug_type, 50, "BUG: KASAN: %s in %pS\n",
		bug_type, (void *)info->ip);
	snprintf(info->bug_info, 60, "%s of size %zu at addr %px\n",
		info->is_write ? "Write" : "Read", info->access_size,
		info->access_addr);
}

extern unsigned long text_start ;
extern unsigned long text_end ;

static inline bool kernel_or_module_addr(const void *addr)
{
	if (addr >= (void *)text_start && addr < (void *)text_end)
		return true;
/*	if (is_module_address((unsigned long)addr))
		return true;
*/	return false;
}
/*
static inline bool init_task_stack_addr(const void *addr)
{
	return addr >= (void *)&init_thread_union.stack &&
		(addr <= (void *)&init_thread_union.stack +
			sizeof(init_thread_union.stack));
}

static DEFINE_SPINLOCK(report_lock);
*/

/*
 * REMOVED start_report and end report
 */

/*
static void print_track(struct kasan_track *track, const char *prefix)
{
	pr_err("%s by task %u:\n", prefix, track->pid);
	if (track->stack) {
		struct stack_trace trace;

		depot_fetch_stack(track->stack, &trace);
		print_stack_trace(&trace, 0);
	} else {
		pr_err("(stack is not available)\n");
	}
}

static struct page *addr_to_page(const void *addr)
{
	if ((addr >= (void *)PAGE_OFFSET) &&
			(addr < high_memory))
		return virt_to_head_page(addr);
	return NULL;
}

static void describe_object_addr(struct kmem_cache *cache, void *object,
				const void *addr)
{
	unsigned long access_addr = (unsigned long)addr;
	unsigned long object_addr = (unsigned long)object;
	const char *rel_type;
	int rel_bytes;

	pr_err("The buggy address belongs to the object at %px\n"
	       " which belongs to the cache %s of size %d\n",
		object, cache->name, cache->object_size);

	if (!addr)
		return;

	if (access_addr < object_addr) {
		rel_type = "to the left";
		rel_bytes = object_addr - access_addr;
	} else if (access_addr >= object_addr + cache->object_size) {
		rel_type = "to the right";
		rel_bytes = access_addr - (object_addr + cache->object_size);
	} else {
		rel_type = "inside";
		rel_bytes = access_addr - object_addr;
	}

	pr_err("The buggy address is located %d bytes %s of\n"
	       " %d-byte region [%px, %px)\n",
		rel_bytes, rel_type, cache->object_size, (void *)object_addr,
		(void *)(object_addr + cache->object_size));
}

static void describe_object(struct kmem_cache *cache, void *object,
				const void *addr)
{
	struct kasan_alloc_meta *alloc_info = get_alloc_info(cache, object);

	if (cache->flags & SLAB_KASAN) {
		print_track(&alloc_info->alloc_track, "Allocated");
		pr_err("\n");
		print_track(&alloc_info->free_track, "Freed");
		pr_err("\n");
	}

	describe_object_addr(cache, object, addr);
}

static void print_address_description(void *addr)
{
	struct page *page = addr_to_page(addr);

	dump_stack();
	pr_err("\n");

	if (page && PageSlab(page)) {
		struct kmem_cache *cache = page->slab_cache;
		void *object = nearest_obj(cache, page,	addr);

		describe_object(cache, object, addr);
	}

	if (kernel_or_module_addr(addr) && !init_task_stack_addr(addr)) {
		pr_err("The buggy address belongs to the variable:\n");
		pr_err(" %pS\n", addr);
	}

	if (page) {
		pr_err("The buggy address belongs to the page:\n");
		dump_page(page, "kasan: bad access detected");
	}
}

static bool row_is_guilty(const void *row, const void *guilty)
{
	return (row <= guilty) && (guilty < row + SHADOW_BYTES_PER_ROW);
}

static int shadow_pointer_offset(const void *row, const void *shadow)
{
*/	/* The length of ">ff00ff00ff00ff00: " is
	 *    3 + (BITS_PER_LONG/8)*2 chars.
	 */
/*	return 3 + (BITS_PER_LONG/8)*2 + (shadow - row)*2 +
		(shadow - row) / SHADOW_BYTES_PER_BLOCK + 1;
}

static void print_shadow_for_address(const void *addr)
{
	int i;
	const void *shadow = kasan_mem_to_shadow(addr);
	const void *shadow_row;

	shadow_row = (void *)round_down((unsigned long)shadow,
					SHADOW_BYTES_PER_ROW)
		- SHADOW_ROWS_AROUND_ADDR * SHADOW_BYTES_PER_ROW;

	pr_err("Memory state around the buggy address:\n");

	for (i = -SHADOW_ROWS_AROUND_ADDR; i <= SHADOW_ROWS_AROUND_ADDR; i++) {
		const void *kaddr = kasan_shadow_to_mem(shadow_row);
		char buffer[4 + (BITS_PER_LONG/8)*2];
		char shadow_buf[SHADOW_BYTES_PER_ROW];

		snprintf(buffer, sizeof(buffer),
			(i == 0) ? ">%px: " : " %px: ", kaddr);
*/		/*
		 * We should not pass a shadow pointer to generic
		 * function, because generic functions may try to
		 * access kasan mapping for the passed address.
		 */
/*		memcpy(shadow_buf, shadow_row, SHADOW_BYTES_PER_ROW);
		print_hex_dump(KERN_ERR, buffer,
			DUMP_PREFIX_NONE, SHADOW_BYTES_PER_ROW, 1,
			shadow_buf, SHADOW_BYTES_PER_ROW, 0);

		if (row_is_guilty(shadow_row, shadow))
			pr_err("%*c\n",
				shadow_pointer_offset(shadow_row, shadow),
				'^');

		shadow_row += SHADOW_BYTES_PER_ROW;
	}
}

void kasan_report_invalid_free(void *object, unsigned long ip)
{
	unsigned long flags;

	kasan_start_report(&flags);
	pr_err("BUG: KASAN: double-free or invalid-free in %pS\n", (void *)ip);
	pr_err("\n");
	print_address_description(object);
	pr_err("\n");
	print_shadow_for_address(object);
	kasan_end_report(&flags);
}
*/
static void kasan_report_error(struct kasan_bug_info *info)
{
	const char *delimit = "==================================================================\n";

        info->start = delimit;
        info->end = delimit;

	print_error_description(info);

	if (!addr_has_shadow(info)) {
//		dump_stack();
	} else {
//		print_address_description((void *)info->access_addr);
//		pr_err("\n");
//		print_shadow_for_address(info->first_bad_addr);
	}

}

static void
kasan_print_report(struct kasan_bug_info *info)
{
        panic("%.67s %s\n %s\n %.67s", info->start, info->bug_type, info->bug_info, info->end);
}

//static unsigned long kasan_flags;

#define KASAN_BIT_REPORTED	0
#define KASAN_BIT_MULTI_SHOT	1

bool kasan_save_enable_multi_shot(void);
bool kasan_save_enable_multi_shot(void)
{
//	return test_and_set_bit(KASAN_BIT_MULTI_SHOT, &kasan_flags);
        return true;
}

void kasan_restore_multi_shot(bool);
void kasan_restore_multi_shot(bool enabled)
{
//	if (!enabled)
//		clear_bit(KASAN_BIT_MULTI_SHOT, &kasan_flags);
}
/*
static int __init kasan_set_multi_shot(char *str)
{
	set_bit(KASAN_BIT_MULTI_SHOT, &kasan_flags);
	return 1;
}
__setup("kasan_multi_shot", kasan_set_multi_shot);

static inline bool kasan_report_enabled(void)
{
	if (kasan_depth)
		return false;
	if (test_bit(KASAN_BIT_MULTI_SHOT, &kasan_flags))
		return true;
	return !test_and_set_bit(KASAN_BIT_REPORTED, &kasan_flags);
}
*/
void kasan_report(unsigned long addr, size_t size,
		bool is_write, unsigned long ip)
{
	struct kasan_bug_info info;

/*	if (likely(!kasan_report_enabled()))
                return;
Not necessary maybe
*/
//	disable_trace_on_warning();

	info.access_addr = (void *)addr;
	info.first_bad_addr = (void *)addr;
	info.access_size = size;
	info.is_write = is_write;
	info.ip = ip;

	kasan_report_error(&info);
        kasan_print_report(&info);
}


#define DEFINE_ASAN_REPORT_LOAD(size)                     \
void __asan_report_load##size##_noabort(unsigned long); \
void __asan_report_load##size##_noabort(unsigned long addr) \
{                                                         \
	kasan_report(addr, size, false, _RET_IP_);	  \
}                                                         \

#define DEFINE_ASAN_REPORT_STORE(size)                     \
void __asan_report_store##size##_noabort(unsigned long ); \
void __asan_report_store##size##_noabort(unsigned long addr) \
{                                                          \
	kasan_report(addr, size, true, _RET_IP_);	   \
}                                                          \

DEFINE_ASAN_REPORT_LOAD(1);
DEFINE_ASAN_REPORT_LOAD(2);
DEFINE_ASAN_REPORT_LOAD(4);
DEFINE_ASAN_REPORT_LOAD(8);
DEFINE_ASAN_REPORT_LOAD(16);
DEFINE_ASAN_REPORT_STORE(1);
DEFINE_ASAN_REPORT_STORE(2);
DEFINE_ASAN_REPORT_STORE(4);
DEFINE_ASAN_REPORT_STORE(8);
DEFINE_ASAN_REPORT_STORE(16);

void __asan_report_load_n_noabort(unsigned long , size_t );
void __asan_report_load_n_noabort(unsigned long addr, size_t size)
{
	kasan_report(addr, size, false, _RET_IP_);
}

void __asan_report_store_n_noabort(unsigned long, size_t );
void __asan_report_store_n_noabort(unsigned long addr, size_t size)
{
	kasan_report(addr, size, true, _RET_IP_);
}
