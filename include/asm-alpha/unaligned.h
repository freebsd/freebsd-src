#ifndef __ALPHA_UNALIGNED_H
#define __ALPHA_UNALIGNED_H

/* 
 * The main single-value unaligned transfer routines.
 */
#define get_unaligned(ptr) \
	((__typeof__(*(ptr)))__get_unaligned((ptr), sizeof(*(ptr))))
#define put_unaligned(x,ptr) \
	__put_unaligned((unsigned long)(x), (ptr), sizeof(*(ptr)))

/*
 * This is a silly but good way to make sure that
 * the get/put functions are indeed always optimized,
 * and that we use the correct sizes.
 */
extern void bad_unaligned_access_length(void);

/*
 * EGCS 1.1 knows about arbitrary unaligned loads.  Define some
 * packed structures to talk about such things with.
 */

struct __una_u64 { __u64 x __attribute__((packed)); };
struct __una_u32 { __u32 x __attribute__((packed)); };
struct __una_u16 { __u16 x __attribute__((packed)); };

/*
 * Elemental unaligned loads 
 */

extern inline unsigned long __uldq(const unsigned long * r11)
{
	const struct __una_u64 *ptr = (const struct __una_u64 *) r11;
	return ptr->x;
}

extern inline unsigned long __uldl(const unsigned int * r11)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *) r11;
	return ptr->x;
}

extern inline unsigned long __uldw(const unsigned short * r11)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *) r11;
	return ptr->x;
}

/*
 * Elemental unaligned stores 
 */

extern inline void __ustq(unsigned long r5, unsigned long * r11)
{
	struct __una_u64 *ptr = (struct __una_u64 *) r11;
	ptr->x = r5;
}

extern inline void __ustl(unsigned long r5, unsigned int * r11)
{
	struct __una_u32 *ptr = (struct __una_u32 *) r11;
	ptr->x = r5;
}

extern inline void __ustw(unsigned long r5, unsigned short * r11)
{
	struct __una_u16 *ptr = (struct __una_u16 *) r11;
	ptr->x = r5;
}

extern inline unsigned long __get_unaligned(const void *ptr, size_t size)
{
	unsigned long val;
	switch (size) {
	      case 1:
		val = *(const unsigned char *)ptr;
		break;
	      case 2:
		val = __uldw((const unsigned short *)ptr);
		break;
	      case 4:
		val = __uldl((const unsigned int *)ptr);
		break;
	      case 8:
		val = __uldq((const unsigned long *)ptr);
		break;
	      default:
		bad_unaligned_access_length();
	}
	return val;
}

extern inline void __put_unaligned(unsigned long val, void *ptr, size_t size)
{
	switch (size) {
	      case 1:
		*(unsigned char *)ptr = (val);
	        break;
	      case 2:
		__ustw(val, (unsigned short *)ptr);
		break;
	      case 4:
		__ustl(val, (unsigned int *)ptr);
		break;
	      case 8:
		__ustq(val, (unsigned long *)ptr);
		break;
	      default:
	    	bad_unaligned_access_length();
	}
}

#endif
