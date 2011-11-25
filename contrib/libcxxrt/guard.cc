/**
 * guard.cc: Functions for thread-safe static initialisation.
 *
 * Static values in C++ can be initialised lazily their first use.  This file
 * contains functions that are used to ensure that two threads attempting to
 * initialize the same static do not call the constructor twice.  This is
 * important because constructors can have side effects, so calling the
 * constructor twice may be very bad.
 *
 * Statics that require initialisation are protected by a 64-bit value.  Any
 * platform that can do 32-bit atomic test and set operations can use this
 * value as a low-overhead lock.  Because statics (in most sane code) are
 * accessed far more times than they are initialised, this lock implementation
 * is heavily optimised towards the case where the static has already been
 * initialised.  
 */
#include <stdint.h>
#include <pthread.h>
#include <assert.h>

#ifdef __arm__
// ARM ABI - 32-bit guards.

/**
 * Acquires a lock on a guard, returning 0 if the object has already been
 * initialised, and 1 if it has not.  If the object is already constructed then
 * this function just needs to read a byte from memory and return.
 */
extern "C" int __cxa_guard_acquire(volatile int32_t *guard_object)
{
	if ((1<<31) == *guard_object) { return 0; }
	// If we can atomically move the value from 0 -> 1, then this is
	// uninitialised.
	if (__sync_bool_compare_and_swap(guard_object, 0, 1))
	{
		return 1;
	}
	// If the value is not 0, some other thread was initialising this.  Spin
	// until it's finished.
	while (__sync_bool_compare_and_swap(guard_object, (1<<31), (1<<31)))
	{
		// If the other thread aborted, then we grab the lock
		if (__sync_bool_compare_and_swap(guard_object, 0, 1))
		{
			return 1;
		}
		sched_yield();
	}
	return 0;
}

/**
 * Releases the lock without marking the object as initialised.  This function
 * is called if initialising a static causes an exception to be thrown.
 */
extern "C" void __cxa_guard_abort(int32_t *guard_object)
{
	assert(__sync_bool_compare_and_swap(guard_object, 1, 0));
}
/**
 * Releases the guard and marks the object as initialised.  This function is
 * called after successful initialisation of a static.
 */
extern "C" void __cxa_guard_release(int32_t *guard_object)
{
	assert(__sync_bool_compare_and_swap(guard_object, 1, (1<<31)));
}


#else
// Itanium ABI: 64-bit guards

/**
 * Returns a pointer to the low 32 bits in a 64-bit value, respecting the
 * platform's byte order.
 */
static int32_t *low_32_bits(volatile int64_t *ptr)
{
	int32_t *low= (int32_t*)ptr;
	// Test if the machine is big endian - constant propagation at compile time
	// should eliminate this completely.
	int one = 1;
	if (*(char*)&one != 1)
	{
		low++;
	}
	return low;
}

/**
 * Acquires a lock on a guard, returning 0 if the object has already been
 * initialised, and 1 if it has not.  If the object is already constructed then
 * this function just needs to read a byte from memory and return.
 */
extern "C" int __cxa_guard_acquire(volatile int64_t *guard_object)
{
	char first_byte = (*guard_object) >> 56;
	if (1 == first_byte) { return 0; }
	int32_t *lock = low_32_bits(guard_object);
	// Simple spin lock using the low 32 bits.  We assume that concurrent
	// attempts to initialize statics are very rare, so we don't need to
	// optimise for the case where we have lots of threads trying to acquire
	// the lock at the same time.
	while (!__sync_bool_compare_and_swap_4(lock, 0, 1))
	{
		sched_yield();
	}
	// We have to test the guard again, in case another thread has performed
	// the initialisation while we were trying to acquire the lock.
	first_byte = (*guard_object) >> 56;
	return (1 != first_byte);
}

/**
 * Releases the lock without marking the object as initialised.  This function
 * is called if initialising a static causes an exception to be thrown.
 */
extern "C" void __cxa_guard_abort(int64_t *guard_object)
{
	int32_t *lock = low_32_bits(guard_object);
	*lock = 0;
}
/**
 * Releases the guard and marks the object as initialised.  This function is
 * called after successful initialisation of a static.
 */
extern "C" void __cxa_guard_release(int64_t *guard_object)
{
	// Set the first byte to 1
	*guard_object |= ((int64_t)1) << 56;
	__cxa_guard_abort(guard_object);
}

#endif
