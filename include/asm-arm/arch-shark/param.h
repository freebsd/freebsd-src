/*
 * linux/include/asm-arm/arch-shark/param.h
 *
 * by Alexander Schulz
 */

/* This must be a power of 2 because the RTC
 * can't use anything else.
 */
#define HZ 64

#define hz_to_std(a) ((a * HZ)/100)
