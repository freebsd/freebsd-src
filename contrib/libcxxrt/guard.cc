/*
 * Copyright 2010-2012 PathScale, Inc. All rights reserved.
 * Copyright 2021 David Chisnall. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#include "atomic.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

// Older GCC doesn't define __LITTLE_ENDIAN__
#ifndef __LITTLE_ENDIAN__
// If __BYTE_ORDER__ is defined, use that instead
#	ifdef __BYTE_ORDER__
#		if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#			define __LITTLE_ENDIAN__
#		endif
// x86 and ARM are the most common little-endian CPUs, so let's have a
// special case for them (ARM is already special cased).  Assume everything
// else is big endian.
#	elif defined(__x86_64) || defined(__i386)
#		define __LITTLE_ENDIAN__
#	endif
#endif

/*
 * The Itanium C++ ABI defines guard words that are 64-bit (32-bit on AArch32)
 * values with one bit defined to indicate that the guarded variable is and
 * another bit to indicate that it's currently locked (initialisation in
 * progress).  The bit to use depends on the byte order of the target.
 *
 * On many 32-bit platforms, 64-bit atomics are unavailable (or slow) and so we
 * treat the two halves of the 64-bit word as independent values and establish
 * an ordering on them such that the guard word is never modified unless the
 * lock word is in the locked state.  This means that we can do double-checked
 * locking by loading the guard word and, if it is not initialised, trying to
 * transition the lock word from the unlocked to locked state, and then
 * manipulate the guard word.
 */
namespace
{
	/**
	 * The state of the guard variable when an attempt is made to lock it.
	 */
	enum class GuardState
	{
		/**
		 * The lock is not held but is not needed because initialisation is
		 * one.
		 */
		InitDone,

		/**
		 * Initialisation is not done but the lock is held by the caller.
		 */
		InitLockSucceeded,

		/**
		 * Attempting to acquire the lock failed.
		 */
		InitLockFailed
	};

	/**
	 * Class encapsulating a single atomic word being used to represent the
	 * guard.  The word size is defined by the type of `GuardWord`.  The bit
	 * used to indicate the locked state is `1<<LockedBit`, the bit used to
	 * indicate the initialised state is `1<<InitBit`.
	 */
	template<typename GuardWord, int LockedBit, int InitBit>
	struct SingleWordGuard
	{
		/**
		 * The value indicating that the lock bit is set (and no other bits).
		 */
		static constexpr GuardWord locked = static_cast<GuardWord>(1)
		                                    << LockedBit;

		/**
		 * The value indicating that the initialised bit is set (and all other
		 * bits are zero).
		 */
		static constexpr GuardWord initialised = static_cast<GuardWord>(1)
		                                         << InitBit;

		/**
		 * The guard variable.
		 */
		atomic<GuardWord> val;

		public:
		/**
		 * Release the lock and set the initialised state.  In the single-word
		 * implementation here, these are both done by a single store.
		 */
		void unlock(bool isInitialised)
		{
			val.store(isInitialised ? initialised : 0, memory_order::release);
#ifndef NDEBUG
			GuardWord init_state = initialised;
			assert(*reinterpret_cast<uint8_t*>(&init_state) != 0);
#endif
		}

		/**
		 * Try to acquire the lock.  This has a tri-state return, indicating
		 * either that the lock was acquired, it wasn't acquired because it was
		 * contended, or it wasn't acquired because the guarded variable is
		 * already initialised.
		 */
		GuardState try_lock()
		{
			GuardWord old = 0;
			// Try to acquire the lock, assuming that we are in the state where
			// the lock is not held and the variable is not initialised (so the
			// expected value is 0).
			if (val.compare_exchange(old, locked))
			{
				return GuardState::InitLockSucceeded;
			}
			// If the CAS failed and the old value indicates that this is
			// initialised, return that initialisation is done and skip further
			// retries.
			if (old == initialised)
			{
				return GuardState::InitDone;
			}
			// Otherwise, report failure.
			return GuardState::InitLockFailed;
		}

		/**
		 * Check whether the guard indicates that the variable is initialised.
		 */
		bool is_initialised()
		{
			return (val.load(memory_order::acquire) & initialised) ==
			       initialised;
		}
	};

	/**
	 * Class encapsulating using two 32-bit atomic values to represent a 64-bit
	 * guard variable.
	 */
	template<int LockedBit, int InitBit>
	class DoubleWordGuard
	{
		/**
		 * The value of `lock_word` when the lock is held.
		 */
		static constexpr uint32_t locked = static_cast<uint32_t>(1)
		                                   << LockedBit;

		/**
		 * The value of `init_word` when the guarded variable is initialised.
		 */
		static constexpr uint32_t initialised = static_cast<uint32_t>(1)
		                                        << InitBit;

		/**
		 * The word used for the initialised flag.  This is always the first
		 * word irrespective of endian because the generated code compares the
		 * first byte in memory against 0.
		 */
		atomic<uint32_t> init_word;

		/**
		 * The word used for the lock.
		 */
		atomic<uint32_t> lock_word;

		public:
		/**
		 * Try to acquire the lock.  This has a tri-state return, indicating
		 * either that the lock was acquired, it wasn't acquired because it was
		 * contended, or it wasn't acquired because the guarded variable is
		 * already initialised.
		 */
		GuardState try_lock()
		{
			uint32_t old = 0;
			// Try to acquire the lock
			if (lock_word.compare_exchange(old, locked))
			{
				// If we succeeded, check if initialisation has happened.  In
				// this version, we don't have atomic manipulation of both the
				// lock and initialised bits together.  Instead, we have an
				// ordering rule that the initialised bit is only ever updated
				// with the lock held.
				if (is_initialised())
				{
					// If another thread did manage to initialise this, release
					// the lock and notify the caller that initialisation is
					// done.
					lock_word.store(0, memory_order::release);
					return GuardState::InitDone;
				}
				return GuardState::InitLockSucceeded;
			}
			return GuardState::InitLockFailed;
		}

		/**
		 * Set the initialised state and release the lock.  In this
		 * implementation, this is ordered, not atomic: the initialise bit is
		 * set while the lock is held.
		 */
		void unlock(bool isInitialised)
		{
			init_word.store(isInitialised ? initialised : 0,
			                  memory_order::release);
			lock_word.store(0, memory_order::release);
			assert((*reinterpret_cast<uint8_t*>(this) != 0) == isInitialised);
		}

		/**
		 * Return whether the guarded variable is initialised.
		 */
		bool is_initialised()
		{
			return (init_word.load(memory_order::acquire) & initialised) ==
			       initialised;
		}
	};

	// Check that the two implementations are the correct size.
	static_assert(sizeof(SingleWordGuard<uint32_t, 31, 0>) == sizeof(uint32_t),
	              "Single-word 32-bit guard must be 32 bits");
	static_assert(sizeof(SingleWordGuard<uint64_t, 63, 0>) == sizeof(uint64_t),
	              "Single-word 64-bit guard must be 64 bits");
	static_assert(sizeof(DoubleWordGuard<31, 0>) == sizeof(uint64_t),
	              "Double-word guard must be 64 bits");

#ifdef __arm__
	/**
	 * The Arm PCS defines a variant of the Itanium ABI with 32-bit lock words.
	 */
	using Guard = SingleWordGuard<uint32_t, 31, 0>;
#elif defined(_LP64)
#	if defined(__LITTLE_ENDIAN__)
	/**
	 * On little-endian 64-bit platforms the guard word is a single 64-bit
	 * atomic with the lock in the high bit and the initialised flag in the low
	 * bit.
	 */
	using Guard = SingleWordGuard<uint64_t, 63, 0>;
#	else
	/**
	 * On bit-endian 64-bit platforms, the guard word is a single 64-bit atomic
	 * with the lock in the low bit and the initialised bit in the highest
	 * byte.
	 */
	using Guard = SingleWordGuard<uint64_t, 0, 56>;
#	endif
#else
#	if defined(__LITTLE_ENDIAN__)
	/**
	 * 32-bit platforms use the same layout as 64-bit.
	 */
	using Guard = DoubleWordGuard<31, 0>;
#	else
	/**
	 * 32-bit platforms use the same layout as 64-bit.
	 */
	using Guard = DoubleWordGuard<0, 24>;
#	endif
#endif

} // namespace

/**
 * Acquires a lock on a guard, returning 0 if the object has already been
 * initialised, and 1 if it has not.  If the object is already constructed then
 * this function just needs to read a byte from memory and return.
 */
extern "C" int __cxa_guard_acquire(Guard *guard_object)
{
	// Check if this is already initialised.  If so, we don't have to do
	// anything.
	if (guard_object->is_initialised())
	{
		return 0;
	}
	// Spin trying to acquire the lock.  If we fail to acquire the lock the
	// first time then another thread will *probably* initialise it, but if the
	// constructor throws an exception then we may have to try again in this
	// thread.
	for (;;)
	{
		// Try to acquire the lock.
		switch (guard_object->try_lock())
		{
			// If we failed to acquire the lock but another thread has
			// initialised the lock while we were waiting, return immediately
			// indicating that initialisation is not required.
			case GuardState::InitDone:
				return 0;
			// If we acquired the lock, return immediately to start
			// initialisation.
			case GuardState::InitLockSucceeded:
				return 1;
			// If we didn't acquire the lock, pause and retry.
			case GuardState::InitLockFailed:
				break;
		}
		sched_yield();
	}
}

/**
 * Releases the lock without marking the object as initialised.  This function
 * is called if initialising a static causes an exception to be thrown.
 */
extern "C" void __cxa_guard_abort(Guard *guard_object)
{
	guard_object->unlock(false);
}

/**
 * Releases the guard and marks the object as initialised.  This function is
 * called after successful initialisation of a static.
 */
extern "C" void __cxa_guard_release(Guard *guard_object)
{
	guard_object->unlock(true);
}
