#include "typeinfo.h"
#include <stdio.h>

using namespace ABI_NAMESPACE;

/**
 * Vtable header.
 */
struct vtable_header
{
	/** Offset of the leaf object. */
	ptrdiff_t leaf_offset;
	/** Type of the object. */
	const __class_type_info *type;
};

/**
 * Simple macro that does pointer arithmetic in bytes but returns a value of
 * the same type as the original.
 */
#define ADD_TO_PTR(x, off) (__typeof__(x))(((char*)x) + off)

bool __class_type_info::can_cast_to(const struct __class_type_info *other) const
{
    return this == other;
}

void *__class_type_info::cast_to(void *obj, const struct __class_type_info *other) const
{
	if (this == other)
	{
		return obj;
	}
	return 0;
}


bool __si_class_type_info::can_cast_to(const struct __class_type_info *other) const
{
    return this == other || __base_type->can_cast_to(other);
}

void *__si_class_type_info::cast_to(void *obj, const struct __class_type_info *other) const
{
	if (this == other)
	{
		return obj;
	}
	return __base_type->cast_to(obj, other);
}


bool __vmi_class_type_info::can_cast_to(const struct __class_type_info *other) const
{
	if (this == other)
	{
		return true;
	}
	for (unsigned int i=0 ; i<__base_count ; i++)
	{
		const __base_class_type_info *info = &__base_info[i];
        if(info->isPublic() && info->__base_type->can_cast_to(other))
        {
            return true;
        }
	}
	return false;
}

void *__vmi_class_type_info::cast_to(void *obj, const struct __class_type_info *other) const
{
	if (this == other)
	{
		return obj;
	}
	for (unsigned int i=0 ; i<__base_count ; i++)
	{
		const __base_class_type_info *info = &__base_info[i];
		ptrdiff_t offset = info->offset();
		// If this is a virtual superclass, the offset is stored in the
		// object's vtable at the offset requested; 2.9.5.6.c:
		//
		// 'For a non-virtual base, this is the offset in the object of the
		// base subobject. For a virtual base, this is the offset in the
		// virtual table of the virtual base offset for the virtual base
		// referenced (negative).'

		if (info->isVirtual())
		{
			// Object's vtable
			ptrdiff_t *off = *(ptrdiff_t**)obj;
			// Offset location in vtable
			off = ADD_TO_PTR(off, offset);
			offset = *off;
		}
		void *cast = ADD_TO_PTR(obj, offset);

		if (info->__base_type == other)
		{
			return cast;
		}
		if ((cast = info->__base_type->cast_to(cast, other)))
		{
			return cast;
		}
	}
	return 0;
}

/**
 * ABI function used to implement the dynamic_cast<> operator.  Some cases of
 * this operator are implemented entirely in the compiler (e.g. to void*).
 * This function implements the dynamic casts of the form dynamic_cast<T>(v).
 * This will be translated to a call to this function with the value v as the
 * first argument.  The type id of the static type of v is the second argument
 * and the type id of the destination type (T) is the third argument.
 *
 * The third argument is a hint about the compiler's guess at the correct
 * pointer offset.  If this value is negative, then -1 indicates no hint, -2
 * that src is not a public base of dst, and -3 that src is a multiple public
 * base type but never a virtual base type
 */
extern "C" void* __dynamic_cast(const void *sub,
                                const __class_type_info *src,
                                const __class_type_info *dst,
                                ptrdiff_t src2dst_offset)
{
	char *vtable_location = *(char**)sub;
	const vtable_header *header =
		(const vtable_header*)(vtable_location - sizeof(vtable_header));
	void *leaf = ADD_TO_PTR((void*)sub, header->leaf_offset);
	return header->type->cast_to(leaf, dst);
}
