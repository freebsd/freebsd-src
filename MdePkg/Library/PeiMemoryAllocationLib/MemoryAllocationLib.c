/** @file
  Support routines for memory allocation routines 
  based on PeiService for PEI phase drivers.

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php.                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/


#include <PiPei.h>


#include <Library/MemoryAllocationLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>


/**
  Allocates one or more 4KB pages of a certain memory type.

  Allocates the number of 4KB pages of a certain memory type and returns a pointer to the allocated
  buffer.  The buffer returned is aligned on a 4KB boundary.  If Pages is 0, then NULL is returned.
  If there is not enough memory remaining to satisfy the request, then NULL is returned.

  @param  MemoryType            The type of memory to allocate.
  @param  Pages                 The number of 4 KB pages to allocate.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
InternalAllocatePages (
  IN EFI_MEMORY_TYPE  MemoryType,  
  IN UINTN            Pages
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  Memory; 

  if (Pages == 0) {
    return NULL;
  }

  Status = PeiServicesAllocatePages (MemoryType, Pages, &Memory);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return (VOID *) (UINTN) Memory;
}

/**
  Allocates one or more 4KB pages of type EfiBootServicesData.

  Allocates the number of 4KB pages of type EfiBootServicesData and returns a pointer to the
  allocated buffer.  The buffer returned is aligned on a 4KB boundary.  If Pages is 0, then NULL
  is returned.  If there is not enough memory remaining to satisfy the request, then NULL is
  returned.

  @param  Pages                 The number of 4 KB pages to allocate.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocatePages (
  IN UINTN  Pages
  )
{
  return InternalAllocatePages (EfiBootServicesData, Pages);
}

/**
  Allocates one or more 4KB pages of type EfiRuntimeServicesData.

  Allocates the number of 4KB pages of type EfiRuntimeServicesData and returns a pointer to the
  allocated buffer.  The buffer returned is aligned on a 4KB boundary.  If Pages is 0, then NULL
  is returned.  If there is not enough memory remaining to satisfy the request, then NULL is
  returned.

  @param  Pages                 The number of 4 KB pages to allocate.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateRuntimePages (
  IN UINTN  Pages
  )
{
  return InternalAllocatePages (EfiRuntimeServicesData, Pages);
}

/**
  Allocates one or more 4KB pages of type EfiReservedMemoryType.

  Allocates the number of 4KB pages of type EfiReservedMemoryType and returns a pointer to the
  allocated buffer.  The buffer returned is aligned on a 4KB boundary.  If Pages is 0, then NULL
  is returned.  If there is not enough memory remaining to satisfy the request, then NULL is
  returned.

  @param  Pages                 The number of 4 KB pages to allocate.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateReservedPages (
  IN UINTN  Pages
  )
{
  return InternalAllocatePages (EfiReservedMemoryType, Pages);
}

/**
  Frees one or more 4KB pages that were previously allocated with one of the page allocation
  functions in the Memory Allocation Library.

  Frees the number of 4KB pages specified by Pages from the buffer specified by Buffer.  Buffer
  must have been allocated on a previous call to the page allocation services of the Memory
  Allocation Library.  If it is not possible to free allocated pages, then this function will
  perform no actions.
  
  If Buffer was not allocated with a page allocation function in the Memory Allocation Library,
  then ASSERT().
  If Pages is zero, then ASSERT().
 
  @param  Buffer                The pointer to the buffer of pages to free.
  @param  Pages                 The number of 4 KB pages to free.

**/
VOID
EFIAPI
FreePages (
  IN VOID   *Buffer,
  IN UINTN  Pages
  )
{
  ASSERT (Pages != 0);
  //
  // PEI phase does not support to free pages, so leave it as NOP.
  //
}

/**
  Allocates one or more 4KB pages of a certain memory type at a specified alignment.

  Allocates the number of 4KB pages specified by Pages of a certain memory type with an alignment
  specified by Alignment.  The allocated buffer is returned.  If Pages is 0, then NULL is returned.
  If there is not enough memory at the specified alignment remaining to satisfy the request, then
  NULL is returned.
  If Alignment is not a power of two and Alignment is not zero, then ASSERT().
  If Pages plus EFI_SIZE_TO_PAGES (Alignment) overflows, then ASSERT().

  @param  MemoryType            The type of memory to allocate.
  @param  Pages                 The number of 4 KB pages to allocate.
  @param  Alignment             The requested alignment of the allocation.  
                                Must be a power of two.
                                If Alignment is zero, then byte alignment is used.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
InternalAllocateAlignedPages (
  IN EFI_MEMORY_TYPE  MemoryType,  
  IN UINTN            Pages,
  IN UINTN            Alignment
  )
{
  EFI_PHYSICAL_ADDRESS   Memory;
  EFI_PHYSICAL_ADDRESS   AlignedMemory;
  EFI_PEI_HOB_POINTERS   Hob;
  BOOLEAN                SkipBeforeMemHob;
  BOOLEAN                SkipAfterMemHob;
  EFI_PHYSICAL_ADDRESS   HobBaseAddress;
  UINT64                 HobLength;
  EFI_MEMORY_TYPE        HobMemoryType;
  UINTN                  TotalPages;

  //
  // Alignment must be a power of two or zero.
  //
  ASSERT ((Alignment & (Alignment - 1)) == 0);

  if (Pages == 0) {
    return NULL;
  }
  //
  // Make sure that Pages plus EFI_SIZE_TO_PAGES (Alignment) does not overflow.
  //
  ASSERT (Pages <= (MAX_ADDRESS - EFI_SIZE_TO_PAGES (Alignment))); 

  //
  // We would rather waste some memory to save PEI code size.
  // meaning in addition to the requested size for the aligned mem,
  // we simply reserve an overhead memory equal to Alignmemt(page-aligned), no matter what.
  // The overhead mem size could be reduced later with more involved malloc mechanisms
  // (e.g., somthing that can detect the alignment boundary before allocating memory or 
  //  can request that memory be allocated at a certain address that is aleady aligned).
  //
  TotalPages = Pages + (Alignment <= EFI_PAGE_SIZE ? 0 : EFI_SIZE_TO_PAGES(Alignment));
  Memory = (EFI_PHYSICAL_ADDRESS) (UINTN) InternalAllocatePages (MemoryType, TotalPages);
  if (Memory == 0) {
    DEBUG((DEBUG_INFO, "Out of memory resource! \n"));
    return NULL;
  }
  DEBUG ((DEBUG_INFO, "Allocated Memory unaligned: Address = 0x%LX, Pages = 0x%X, Type = %d \n", Memory, TotalPages, (UINTN) MemoryType));

  //
  // Alignment calculation
  //
  AlignedMemory = Memory;
  if (Alignment > EFI_PAGE_SIZE) {
    AlignedMemory = ALIGN_VALUE (Memory, Alignment);
  }
  DEBUG ((DEBUG_INFO, "After aligning to 0x%X bytes: Address = 0x%LX, Pages = 0x%X \n", Alignment, AlignedMemory, Pages));

  //
  // In general three HOBs cover the total allocated space.
  // The aligned portion is covered by the aligned mem HOB and
  // the unaligned(to be freed) portions before and after the aligned portion are covered by newly created HOBs.
  //
  // Before mem HOB covers the region between "Memory" and "AlignedMemory"
  // Aligned mem HOB covers the region between "AlignedMemory" and "AlignedMemory + EFI_PAGES_TO_SIZE(Pages)"
  // After mem HOB covers the region between "AlignedMemory + EFI_PAGES_TO_SIZE(Pages)" and "Memory + EFI_PAGES_TO_SIZE(TotalPages)"
  //
  // The before or after mem HOBs need to be skipped under special cases where the aligned portion
  // touches either the top or bottom of the original allocated space.
  //
  SkipBeforeMemHob = FALSE;
  SkipAfterMemHob  = FALSE;
  if (Memory == AlignedMemory) {
    SkipBeforeMemHob = TRUE;
  }
  if ((Memory + EFI_PAGES_TO_SIZE(TotalPages)) == (AlignedMemory + EFI_PAGES_TO_SIZE(Pages))) {
    //
    // This condition is never met in the current implementation.
    // There is always some after-mem since the overhead mem(used in TotalPages)
    // is no less than Alignment.
    //
    SkipAfterMemHob = TRUE;
  }

  //  
  // Search for the mem HOB referring to the original(unaligned) allocation 
  // and update the size and type if needed.
  //
  Hob.Raw = GetFirstHob (EFI_HOB_TYPE_MEMORY_ALLOCATION);
  while (Hob.Raw != NULL) {
    if (Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress == Memory) {
      break;
    }
    Hob.Raw = GET_NEXT_HOB (Hob);
    Hob.Raw = GetNextHob (EFI_HOB_TYPE_MEMORY_ALLOCATION, Hob.Raw);
  }
  ASSERT (Hob.Raw != NULL);
  if (SkipBeforeMemHob) {
    //
    // Use this HOB as aligned mem HOB as there is no portion before it.
    //
    HobLength = EFI_PAGES_TO_SIZE(Pages);
    Hob.MemoryAllocation->AllocDescriptor.MemoryLength = HobLength;
  } else {
    //
    // Use this HOB as before mem HOB and create a new HOB for the aligned portion 
    //
    HobLength = (AlignedMemory - Memory); 
    Hob.MemoryAllocation->AllocDescriptor.MemoryLength = HobLength;
    Hob.MemoryAllocation->AllocDescriptor.MemoryType = EfiConventionalMemory;
  }

  HobBaseAddress = Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress;
  HobMemoryType = Hob.MemoryAllocation->AllocDescriptor.MemoryType;

  //
  // Build the aligned mem HOB if needed
  //
  if (!SkipBeforeMemHob) {
    DEBUG((DEBUG_INFO, "Updated before-mem HOB with BaseAddress = %LX, Length = %LX, MemoryType = %d \n",
      HobBaseAddress, HobLength, (UINTN) HobMemoryType));

    HobBaseAddress = AlignedMemory;
    HobLength = EFI_PAGES_TO_SIZE(Pages);
    HobMemoryType = MemoryType;

    BuildMemoryAllocationHob (
      HobBaseAddress,
      HobLength,
      HobMemoryType
      );

    DEBUG((DEBUG_INFO, "Created aligned-mem HOB with BaseAddress = %LX, Length = %LX, MemoryType = %d \n",
      HobBaseAddress, HobLength, (UINTN) HobMemoryType));
  } else {
    if (HobBaseAddress != 0) {
      DEBUG((DEBUG_INFO, "Updated aligned-mem HOB with BaseAddress = %LX, Length = %LX, MemoryType = %d \n",
        HobBaseAddress, HobLength, (UINTN) HobMemoryType));
    }
  }


  //
  // Build the after mem HOB if needed
  //
  if (!SkipAfterMemHob) {
    HobBaseAddress = AlignedMemory + EFI_PAGES_TO_SIZE(Pages);
    HobLength = (Memory + EFI_PAGES_TO_SIZE(TotalPages)) - (AlignedMemory + EFI_PAGES_TO_SIZE(Pages));
    HobMemoryType = EfiConventionalMemory;

    BuildMemoryAllocationHob (
      HobBaseAddress,
      HobLength,
      HobMemoryType
      );

    DEBUG((DEBUG_INFO, "Created after-mem HOB with BaseAddress = %LX, Length = %LX, MemoryType = %d \n",
      HobBaseAddress, HobLength, (UINTN) HobMemoryType));
  }

  return (VOID *) (UINTN) AlignedMemory;
}

/**
  Allocates one or more 4KB pages of type EfiBootServicesData at a specified alignment.

  Allocates the number of 4KB pages specified by Pages of type EfiBootServicesData with an
  alignment specified by Alignment.  The allocated buffer is returned.  If Pages is 0, then NULL is
  returned.  If there is not enough memory at the specified alignment remaining to satisfy the
  request, then NULL is returned.
  
  If Alignment is not a power of two and Alignment is not zero, then ASSERT().
  If Pages plus EFI_SIZE_TO_PAGES (Alignment) overflows, then ASSERT().

  @param  Pages                 The number of 4 KB pages to allocate.
  @param  Alignment             The requested alignment of the allocation.  
                                Must be a power of two.
                                If Alignment is zero, then byte alignment is used.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateAlignedPages (
  IN UINTN  Pages,
  IN UINTN  Alignment
  )
{
  return InternalAllocateAlignedPages (EfiBootServicesData, Pages, Alignment);
}

/**
  Allocates one or more 4KB pages of type EfiRuntimeServicesData at a specified alignment.

  Allocates the number of 4KB pages specified by Pages of type EfiRuntimeServicesData with an
  alignment specified by Alignment.  The allocated buffer is returned.  If Pages is 0, then NULL is
  returned.  If there is not enough memory at the specified alignment remaining to satisfy the
  request, then NULL is returned.
  
  If Alignment is not a power of two and Alignment is not zero, then ASSERT().
  If Pages plus EFI_SIZE_TO_PAGES (Alignment) overflows, then ASSERT().

  @param  Pages                 The number of 4 KB pages to allocate.
  @param  Alignment             The requested alignment of the allocation.  
                                Must be a power of two.
                                If Alignment is zero, then byte alignment is used.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateAlignedRuntimePages (
  IN UINTN  Pages,
  IN UINTN  Alignment
  )
{
  return InternalAllocateAlignedPages (EfiRuntimeServicesData, Pages, Alignment);
}

/**
  Allocates one or more 4KB pages of type EfiReservedMemoryType at a specified alignment.

  Allocates the number of 4KB pages specified by Pages of type EfiReservedMemoryType with an
  alignment specified by Alignment.  The allocated buffer is returned.  If Pages is 0, then NULL is
  returned.  If there is not enough memory at the specified alignment remaining to satisfy the
  request, then NULL is returned.
  
  If Alignment is not a power of two and Alignment is not zero, then ASSERT().
  If Pages plus EFI_SIZE_TO_PAGES (Alignment) overflows, then ASSERT().

  @param  Pages                 The number of 4 KB pages to allocate.
  @param  Alignment             The requested alignment of the allocation.  
                                Must be a power of two.
                                If Alignment is zero, then byte alignment is used.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateAlignedReservedPages (
  IN UINTN  Pages,
  IN UINTN  Alignment
  )
{
  return InternalAllocateAlignedPages (EfiReservedMemoryType, Pages, Alignment);
}

/**
  Frees one or more 4KB pages that were previously allocated with one of the aligned page
  allocation functions in the Memory Allocation Library.

  Frees the number of 4KB pages specified by Pages from the buffer specified by Buffer.  Buffer
  must have been allocated on a previous call to the aligned page allocation services of the Memory
  Allocation Library.  If it is not possible to free allocated pages, then this function will 
  perform no actions.
  
  If Buffer was not allocated with an aligned page allocation function in the Memory Allocation
  Library, then ASSERT().
  If Pages is zero, then ASSERT().
  
  @param  Buffer                The pointer to the buffer of pages to free.
  @param  Pages                 The number of 4 KB pages to free.

**/
VOID
EFIAPI
FreeAlignedPages (
  IN VOID   *Buffer,
  IN UINTN  Pages
  )
{
  ASSERT (Pages != 0);
  //
  // PEI phase does not support to free pages, so leave it as NOP.
  //
}

/**
  Allocates a buffer of a certain pool type.

  Allocates the number bytes specified by AllocationSize of a certain pool type and returns a
  pointer to the allocated buffer.  If AllocationSize is 0, then a valid buffer of 0 size is
  returned.  If there is not enough memory remaining to satisfy the request, then NULL is returned.

  @param  MemoryType            The type of memory to allocate.
  @param  AllocationSize        The number of bytes to allocate.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
InternalAllocatePool (
  IN EFI_MEMORY_TYPE  MemoryType,  
  IN UINTN            AllocationSize
  )
{
  //
  // If we need lots of small runtime/reserved memory type from PEI in the future, 
  // we can consider providing a more complex algorithm that allocates runtime pages and 
  // provide pool allocations from those pages. 
  //
  return InternalAllocatePages (MemoryType, EFI_SIZE_TO_PAGES (AllocationSize));
}

/**
  Allocates a buffer of type EfiBootServicesData.

  Allocates the number bytes specified by AllocationSize of type EfiBootServicesData and returns a
  pointer to the allocated buffer.  If AllocationSize is 0, then a valid buffer of 0 size is
  returned.  If there is not enough memory remaining to satisfy the request, then NULL is returned.

  @param  AllocationSize        The number of bytes to allocate.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocatePool (
  IN UINTN  AllocationSize
  )
{
  EFI_STATUS        Status;
  VOID              *Buffer;
  
  Status = PeiServicesAllocatePool (AllocationSize, &Buffer);
  if (EFI_ERROR (Status)) {
    Buffer = NULL;
  }
  return Buffer;
}

/**
  Allocates a buffer of type EfiRuntimeServicesData.

  Allocates the number bytes specified by AllocationSize of type EfiRuntimeServicesData and returns
  a pointer to the allocated buffer.  If AllocationSize is 0, then a valid buffer of 0 size is
  returned.  If there is not enough memory remaining to satisfy the request, then NULL is returned.

  @param  AllocationSize        The number of bytes to allocate.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateRuntimePool (
  IN UINTN  AllocationSize
  )
{
  return InternalAllocatePool (EfiRuntimeServicesData, AllocationSize);
}

/**
  Allocates a buffer of type EfiReservedMemoryType.

  Allocates the number bytes specified by AllocationSize of type EfiReservedMemoryType and returns
  a pointer to the allocated buffer.  If AllocationSize is 0, then a valid buffer of 0 size is
  returned.  If there is not enough memory remaining to satisfy the request, then NULL is returned.

  @param  AllocationSize        The number of bytes to allocate.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateReservedPool (
  IN UINTN  AllocationSize
  )
{
  return InternalAllocatePool (EfiReservedMemoryType, AllocationSize);
}

/**
  Allocates and zeros a buffer of a certain pool type.

  Allocates the number bytes specified by AllocationSize of a certain pool type, clears the buffer
  with zeros, and returns a pointer to the allocated buffer.  If AllocationSize is 0, then a valid
  buffer of 0 size is returned.  If there is not enough memory remaining to satisfy the request,
  then NULL is returned.

  @param  PoolType              The type of memory to allocate.
  @param  AllocationSize        The number of bytes to allocate and zero.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
InternalAllocateZeroPool (
  IN EFI_MEMORY_TYPE  PoolType,  
  IN UINTN            AllocationSize
  ) 
{
  VOID  *Memory;

  Memory = InternalAllocatePool (PoolType, AllocationSize);
  if (Memory != NULL) {
    Memory = ZeroMem (Memory, AllocationSize);
  }
  return Memory;
}

/**
  Allocates and zeros a buffer of type EfiBootServicesData.

  Allocates the number bytes specified by AllocationSize of type EfiBootServicesData, clears the
  buffer with zeros, and returns a pointer to the allocated buffer.  If AllocationSize is 0, then a
  valid buffer of 0 size is returned.  If there is not enough memory remaining to satisfy the
  request, then NULL is returned.

  @param  AllocationSize        The number of bytes to allocate and zero.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateZeroPool (
  IN UINTN  AllocationSize
  )
{
  VOID  *Memory;

  Memory = AllocatePool (AllocationSize);
  if (Memory != NULL) {
    Memory = ZeroMem (Memory, AllocationSize);
  }
  return Memory;
}

/**
  Allocates and zeros a buffer of type EfiRuntimeServicesData.

  Allocates the number bytes specified by AllocationSize of type EfiRuntimeServicesData, clears the
  buffer with zeros, and returns a pointer to the allocated buffer.  If AllocationSize is 0, then a
  valid buffer of 0 size is returned.  If there is not enough memory remaining to satisfy the
  request, then NULL is returned.

  @param  AllocationSize        The number of bytes to allocate and zero.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateRuntimeZeroPool (
  IN UINTN  AllocationSize
  )
{
  return InternalAllocateZeroPool (EfiRuntimeServicesData, AllocationSize);
}

/**
  Allocates and zeros a buffer of type EfiReservedMemoryType.

  Allocates the number bytes specified by AllocationSize of type EfiReservedMemoryType, clears the
  buffer with zeros, and returns a pointer to the allocated buffer.  If AllocationSize is 0, then a
  valid buffer of 0 size is returned.  If there is not enough memory remaining to satisfy the
  request, then NULL is returned.

  @param  AllocationSize        The number of bytes to allocate and zero.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateReservedZeroPool (
  IN UINTN  AllocationSize
  )
{
  return InternalAllocateZeroPool (EfiReservedMemoryType, AllocationSize);
}

/**
  Copies a buffer to an allocated buffer of a certain pool type.

  Allocates the number bytes specified by AllocationSize of a certain pool type, copies
  AllocationSize bytes from Buffer to the newly allocated buffer, and returns a pointer to the
  allocated buffer.  If AllocationSize is 0, then a valid buffer of 0 size is returned.  If there
  is not enough memory remaining to satisfy the request, then NULL is returned.
  If Buffer is NULL, then ASSERT().
  If AllocationSize is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT(). 

  @param  PoolType              The type of pool to allocate.
  @param  AllocationSize        The number of bytes to allocate and zero.
  @param  Buffer                The buffer to copy to the allocated buffer.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
InternalAllocateCopyPool (
  IN EFI_MEMORY_TYPE  PoolType,  
  IN UINTN            AllocationSize,
  IN CONST VOID       *Buffer
  ) 
{
  VOID  *Memory;

  ASSERT (Buffer != NULL);
  ASSERT (AllocationSize <= (MAX_ADDRESS - (UINTN) Buffer + 1));

  Memory = InternalAllocatePool (PoolType, AllocationSize);
  if (Memory != NULL) {
     Memory = CopyMem (Memory, Buffer, AllocationSize);
  }
  return Memory;
} 

/**
  Copies a buffer to an allocated buffer of type EfiBootServicesData.

  Allocates the number bytes specified by AllocationSize of type EfiBootServicesData, copies
  AllocationSize bytes from Buffer to the newly allocated buffer, and returns a pointer to the
  allocated buffer.  If AllocationSize is 0, then a valid buffer of 0 size is returned.  If there
  is not enough memory remaining to satisfy the request, then NULL is returned.
  
  If Buffer is NULL, then ASSERT().
  If AllocationSize is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT(). 

  @param  AllocationSize        The number of bytes to allocate and zero.
  @param  Buffer                The buffer to copy to the allocated buffer.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateCopyPool (
  IN UINTN       AllocationSize,
  IN CONST VOID  *Buffer
  )
{
  VOID  *Memory;

  ASSERT (Buffer != NULL);
  ASSERT (AllocationSize <= (MAX_ADDRESS - (UINTN) Buffer + 1));

  Memory = AllocatePool (AllocationSize);
  if (Memory != NULL) {
     Memory = CopyMem (Memory, Buffer, AllocationSize);
  }
  return Memory;
}

/**
  Copies a buffer to an allocated buffer of type EfiRuntimeServicesData.

  Allocates the number bytes specified by AllocationSize of type EfiRuntimeServicesData, copies
  AllocationSize bytes from Buffer to the newly allocated buffer, and returns a pointer to the
  allocated buffer.  If AllocationSize is 0, then a valid buffer of 0 size is returned.  If there
  is not enough memory remaining to satisfy the request, then NULL is returned.
  
  If Buffer is NULL, then ASSERT().
  If AllocationSize is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT(). 

  @param  AllocationSize        The number of bytes to allocate and zero.
  @param  Buffer                The buffer to copy to the allocated buffer.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateRuntimeCopyPool (
  IN UINTN       AllocationSize,
  IN CONST VOID  *Buffer
  )
{
  return InternalAllocateCopyPool (EfiRuntimeServicesData, AllocationSize, Buffer);
}

/**
  Copies a buffer to an allocated buffer of type EfiReservedMemoryType.

  Allocates the number bytes specified by AllocationSize of type EfiReservedMemoryType, copies
  AllocationSize bytes from Buffer to the newly allocated buffer, and returns a pointer to the
  allocated buffer.  If AllocationSize is 0, then a valid buffer of 0 size is returned.  If there
  is not enough memory remaining to satisfy the request, then NULL is returned.
  
  If Buffer is NULL, then ASSERT().
  If AllocationSize is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT(). 

  @param  AllocationSize        The number of bytes to allocate and zero.
  @param  Buffer                The buffer to copy to the allocated buffer.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateReservedCopyPool (
  IN UINTN       AllocationSize,
  IN CONST VOID  *Buffer
  )
{
  return InternalAllocateCopyPool (EfiReservedMemoryType, AllocationSize, Buffer);
}

/**
  Reallocates a buffer of a specified memory type.

  Allocates and zeros the number bytes specified by NewSize from memory of the type
  specified by PoolType.  If OldBuffer is not NULL, then the smaller of OldSize and 
  NewSize bytes are copied from OldBuffer to the newly allocated buffer, and 
  OldBuffer is freed.  A pointer to the newly allocated buffer is returned.  
  If NewSize is 0, then a valid buffer of 0 size is  returned.  If there is not 
  enough memory remaining to satisfy the request, then NULL is returned.
  
  If the allocation of the new buffer is successful and the smaller of NewSize and OldSize
  is greater than (MAX_ADDRESS - OldBuffer + 1), then ASSERT().

  @param  PoolType       The type of pool to allocate.
  @param  OldSize        The size, in bytes, of OldBuffer.
  @param  NewSize        The size, in bytes, of the buffer to reallocate.
  @param  OldBuffer      The buffer to copy to the allocated buffer.  This is an 
                         optional parameter that may be NULL.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
InternalReallocatePool (
  IN EFI_MEMORY_TYPE  PoolType,  
  IN UINTN            OldSize,
  IN UINTN            NewSize,
  IN VOID             *OldBuffer  OPTIONAL
  )
{
  VOID  *NewBuffer;

  NewBuffer = InternalAllocateZeroPool (PoolType, NewSize);
  if (NewBuffer != NULL && OldBuffer != NULL) {
    CopyMem (NewBuffer, OldBuffer, MIN (OldSize, NewSize));
    FreePool (OldBuffer);
  }
  return NewBuffer;
}

/**
  Reallocates a buffer of type EfiBootServicesData.

  Allocates and zeros the number bytes specified by NewSize from memory of type
  EfiBootServicesData.  If OldBuffer is not NULL, then the smaller of OldSize and 
  NewSize bytes are copied from OldBuffer to the newly allocated buffer, and 
  OldBuffer is freed.  A pointer to the newly allocated buffer is returned.  
  If NewSize is 0, then a valid buffer of 0 size is  returned.  If there is not 
  enough memory remaining to satisfy the request, then NULL is returned.
  
  If the allocation of the new buffer is successful and the smaller of NewSize and OldSize
  is greater than (MAX_ADDRESS - OldBuffer + 1), then ASSERT().

  @param  OldSize        The size, in bytes, of OldBuffer.
  @param  NewSize        The size, in bytes, of the buffer to reallocate.
  @param  OldBuffer      The buffer to copy to the allocated buffer.  This is an optional 
                         parameter that may be NULL.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
ReallocatePool (
  IN UINTN  OldSize,
  IN UINTN  NewSize,
  IN VOID   *OldBuffer  OPTIONAL
  )
{
  return InternalReallocatePool (EfiBootServicesData, OldSize, NewSize, OldBuffer);
}

/**
  Reallocates a buffer of type EfiRuntimeServicesData.

  Allocates and zeros the number bytes specified by NewSize from memory of type
  EfiRuntimeServicesData.  If OldBuffer is not NULL, then the smaller of OldSize and 
  NewSize bytes are copied from OldBuffer to the newly allocated buffer, and 
  OldBuffer is freed.  A pointer to the newly allocated buffer is returned.  
  If NewSize is 0, then a valid buffer of 0 size is  returned.  If there is not 
  enough memory remaining to satisfy the request, then NULL is returned.

  If the allocation of the new buffer is successful and the smaller of NewSize and OldSize
  is greater than (MAX_ADDRESS - OldBuffer + 1), then ASSERT().

  @param  OldSize        The size, in bytes, of OldBuffer.
  @param  NewSize        The size, in bytes, of the buffer to reallocate.
  @param  OldBuffer      The buffer to copy to the allocated buffer.  This is an optional 
                         parameter that may be NULL.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
ReallocateRuntimePool (
  IN UINTN  OldSize,
  IN UINTN  NewSize,
  IN VOID   *OldBuffer  OPTIONAL
  )
{
  return InternalReallocatePool (EfiRuntimeServicesData, OldSize, NewSize, OldBuffer);
}

/**
  Reallocates a buffer of type EfiReservedMemoryType.

  Allocates and zeros the number bytes specified by NewSize from memory of type
  EfiReservedMemoryType.  If OldBuffer is not NULL, then the smaller of OldSize and 
  NewSize bytes are copied from OldBuffer to the newly allocated buffer, and 
  OldBuffer is freed.  A pointer to the newly allocated buffer is returned.  
  If NewSize is 0, then a valid buffer of 0 size is  returned.  If there is not 
  enough memory remaining to satisfy the request, then NULL is returned.

  If the allocation of the new buffer is successful and the smaller of NewSize and OldSize
  is greater than (MAX_ADDRESS - OldBuffer + 1), then ASSERT().

  @param  OldSize        The size, in bytes, of OldBuffer.
  @param  NewSize        The size, in bytes, of the buffer to reallocate.
  @param  OldBuffer      The buffer to copy to the allocated buffer.  This is an 
                         optional parameter that may be NULL.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
ReallocateReservedPool (
  IN UINTN  OldSize,
  IN UINTN  NewSize,
  IN VOID   *OldBuffer  OPTIONAL
  )
{
  return InternalReallocatePool (EfiReservedMemoryType, OldSize, NewSize, OldBuffer);
}

/**
  Frees a buffer that was previously allocated with one of the pool allocation functions in the
  Memory Allocation Library.

  Frees the buffer specified by Buffer.  Buffer must have been allocated on a previous call to the
  pool allocation services of the Memory Allocation Library.  If it is not possible to free pool
  resources, then this function will perform no actions.
  
  If Buffer was not allocated with a pool allocation function in the Memory Allocation Library,
  then ASSERT().

  @param  Buffer                The pointer to the buffer to free.

**/
VOID
EFIAPI
FreePool (
  IN VOID   *Buffer
  )
{
  //
  // PEI phase does not support to free pool, so leave it as NOP.
  //
}


