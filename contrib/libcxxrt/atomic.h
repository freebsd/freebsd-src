
#ifndef __has_builtin
#	define __has_builtin(x) 0
#endif
#ifndef __has_feature
#	define __has_feature(x) 0
#endif
#ifndef __has_extension
#	define __has_extension(x) 0
#endif

#if !__has_extension(c_atomic)
#	define _Atomic(T) T
#endif
#if __has_builtin(__c11_atomic_exchange)
#	define ATOMIC_BUILTIN(name) __c11_atomic_##name
#else
#	define ATOMIC_BUILTIN(name) __atomic_##name##_n
#endif

namespace
{
	/**
	 * C++11 memory orders.  We only need a subset of them.
	 */
	enum memory_order
	{
		/**
		 * Acquire order.
		 */
		acquire = __ATOMIC_ACQUIRE,

		/**
		 * Release order.
		 */
		release = __ATOMIC_RELEASE,

		/**
		 * Sequentially consistent memory ordering.
		 */
		seqcst = __ATOMIC_SEQ_CST
	};

	/**
	 * Atomic, implements a subset of `std::atomic`.
	 */
	template<typename T>
	class atomic
	{
		/**
		 * The underlying value.  Use C11 atomic qualification if available.
		 */
		_Atomic(T) val;

		public:
		/**
		 * Constructor, takes a value.
		 */
		atomic(T init) : val(init) {}

		/**
		 * Atomically load with the specified memory order.
		 */
		T load(memory_order order = memory_order::seqcst)
		{
			return ATOMIC_BUILTIN(load)(&val, order);
		}

		/**
		 * Atomically store with the specified memory order.
		 */
		void store(T v, memory_order order = memory_order::seqcst)
		{
			return ATOMIC_BUILTIN(store)(&val, v, order);
		}

		/**
		 * Atomically exchange with the specified memory order.
		 */
		T exchange(T v, memory_order order = memory_order::seqcst)
		{
			return ATOMIC_BUILTIN(exchange)(&val, v, order);
		}

		/**
		 * Atomically exchange with the specified memory order.
		 */
		bool compare_exchange(T &          expected,
		                      T            desired,
		                      memory_order order = memory_order::seqcst)
		{
#if __has_builtin(__c11_atomic_compare_exchange_strong)
			return __c11_atomic_compare_exchange_strong(
			  &val, &expected, desired, order, order);
#else
			return __atomic_compare_exchange_n(
			  &val, &expected, desired, true, order, order);
#endif
		}
	};
} // namespace
#undef ATOMIC_BUILTIN
