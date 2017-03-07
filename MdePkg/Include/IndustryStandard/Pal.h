/** @file
  Main PAL API's defined in Intel Itanium Architecture Software Developer's Manual.

  Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef __PAL_API_H__
#define __PAL_API_H__

#define PAL_SUCCESS             0x0

///
/// CacheType of PAL_CACHE_FLUSH.
///
#define PAL_CACHE_FLUSH_INSTRUCTION_ALL   1
#define PAL_CACHE_FLUSH_DATA_ALL          2
#define PAL_CACHE_FLUSH_ALL               3
#define PAL_CACHE_FLUSH_SYNC_TO_DATA      4


///
/// Bitmask of Opearation of PAL_CACHE_FLUSH.
///
#define PAL_CACHE_FLUSH_INVALIDATE_LINES     BIT0
#define PAL_CACHE_FLUSH_NO_INVALIDATE_LINES  0
#define PAL_CACHE_FLUSH_POLL_INTERRUPT       BIT1
#define PAL_CACHE_FLUSH_NO_INTERRUPT         0

/**
  PAL Procedure - PAL_CACHE_FLUSH.

  Flush the instruction or data caches. It is required by Itanium processors.
  The PAL procedure supports the Static Registers calling
  convention. It could be called at virtual mode and physical
  mode.

  @param Index              Index of PAL_CACHE_FLUSH within the
                            list of PAL procedures.
  @param CacheType          Unsigned 64-bit integer indicating
                            which cache to flush.
  @param Operation          Formatted bit vector indicating the
                            operation of this call.
  @param ProgressIndicator  Unsigned 64-bit integer specifying
                            the starting position of the flush
                            operation.

  @retval 2                 Call completed without error, but a PMI
                            was taken during the execution of this
                            procedure.
  @retval 1                 Call has not completed flushing due to
                            a pending interrupt.
  @retval 0                 Call completed without error
  @retval -2                Invalid argument
  @retval -3                Call completed with error

  @return R9                Unsigned 64-bit integer specifying the vector
                            number of the pending interrupt.
  @return R10               Unsigned 64-bit integer specifying the
                            starting position of the flush operation.
  @return R11               Unsigned 64-bit integer specifying the vector
                            number of the pending interrupt.

**/
#define PAL_CACHE_FLUSH   1


///
/// Attributes of PAL_CACHE_CONFIG_INFO1
///
#define PAL_CACHE_ATTR_WT   0
#define PAL_CACHE_ATTR_WB   1

///
/// PAL_CACHE_CONFIG_INFO1.StoreHint
///
#define PAL_CACHE_STORE_TEMPORAL      0
#define PAL_CACHE_STORE_NONE_TEMPORAL 3

///
/// PAL_CACHE_CONFIG_INFO1.StoreHint
///
#define PAL_CACHE_STORE_TEMPORAL_LVL_1        0
#define PAL_CACHE_STORE_NONE_TEMPORAL_LVL_ALL 3

///
/// PAL_CACHE_CONFIG_INFO1.StoreHint
///
#define PAL_CACHE_LOAD_TEMPORAL_LVL_1         0
#define PAL_CACHE_LOAD_NONE_TEMPORAL_LVL_1    1
#define PAL_CACHE_LOAD_NONE_TEMPORAL_LVL_ALL  3

///
/// Detail the characteristics of a given processor controlled
/// cache in the cache hierarchy.
///
typedef struct {
  UINT64  IsUnified   : 1;
  UINT64  Attributes  : 2;
  UINT64  Associativity:8;
  UINT64  LineSize:8;
  UINT64  Stride:8;
  UINT64  StoreLatency:8;
  UINT64  StoreHint:8;
  UINT64  LoadHint:8;
} PAL_CACHE_INFO_RETURN1;

///
/// Detail the characteristics of a given processor controlled
/// cache in the cache hierarchy.
///
typedef struct {
  UINT64  CacheSize:32;
  UINT64  AliasBoundary:8;
  UINT64  TagLsBits:8;
  UINT64  TagMsBits:8;
} PAL_CACHE_INFO_RETURN2;

/**
  PAL Procedure - PAL_CACHE_INFO.

  Return detailed instruction or data cache information. It is
  required by Itanium processors. The PAL procedure supports the Static
  Registers calling convention. It could be called at virtual
  mode and physical mode.

  @param Index        Index of PAL_CACHE_INFO within the list of
                      PAL procedures.
  @param CacheLevel   Unsigned 64-bit integer specifying the
                      level in the cache hierarchy for which
                      information is requested. This value must
                      be between 0 and one less than the value
                      returned in the cache_levels return value
                      from PAL_CACHE_SUMMARY.
  @param CacheType    Unsigned 64-bit integer with a value of 1
                      for instruction cache and 2 for data or
                      unified cache. All other values are
                      reserved.
  @param Reserved     Should be 0.

  @retval 0           Call completed without error
  @retval -2          Invalid argument
  @retval -3          Call completed with error

  @return R9          Detail the characteristics of a given
                      processor controlled cache in the cache
                      hierarchy. See PAL_CACHE_INFO_RETURN1.
  @return R10         Detail the characteristics of a given
                      processor controlled cache in the cache
                      hierarchy. See PAL_CACHE_INFO_RETURN2.
  @return R11         Reserved with 0.

**/
#define PAL_CACHE_INFO    2



///
/// Level of PAL_CACHE_INIT.
///
#define PAL_CACHE_INIT_ALL  0xffffffffffffffffULL

///
/// CacheType
///
#define PAL_CACHE_INIT_TYPE_INSTRUCTION                 0x1
#define PAL_CACHE_INIT_TYPE_DATA                        0x2
#define PAL_CACHE_INIT_TYPE_INSTRUCTION_AND_DATA        0x3

///
/// Restrict of PAL_CACHE_INIT.
///
#define PAL_CACHE_INIT_NO_RESTRICT  0
#define PAL_CACHE_INIT_RESTRICTED   1

/**
  PAL Procedure - PAL_CACHE_INIT.

  Initialize the instruction or data caches. It is required by
  Itanium processors. The PAL procedure supports the Static Registers calling
  convention. It could be called at physical mode.

  @param Index      Index of PAL_CACHE_INIT within the list of PAL
                    procedures.
  @param Level      Unsigned 64-bit integer containing the level of
                    cache to initialize. If the cache level can be
                    initialized independently, only that level will
                    be initialized. Otherwise
                    implementation-dependent side-effects will
                    occur.
  @param CacheType  Unsigned 64-bit integer with a value of 1 to
                    initialize the instruction cache, 2 to
                    initialize the data cache, or 3 to
                    initialize both. All other values are
                    reserved.
  @param Restrict   Unsigned 64-bit integer with a value of 0 or
                    1. All other values are reserved. If
                    restrict is 1 and initializing the specified
                    level and cache_type of the cache would
                    cause side-effects, PAL_CACHE_INIT will
                    return -4 instead of initializing the cache.

  @retval 0         Call completed without error
  @retval -2        Invalid argument
  @retval -3        Call completed with error.
  @retval -4        Call could not initialize the specified
                    level and cache_type of the cache without
                    side-effects and restrict was 1.

**/
#define PAL_CACHE_INIT    3


///
/// PAL_CACHE_PROTECTION.Method.
///
#define PAL_CACHE_PROTECTION_NONE_PROTECT   0
#define PAL_CACHE_PROTECTION_ODD_PROTECT    1
#define PAL_CACHE_PROTECTION_EVEN_PROTECT   2
#define PAL_CACHE_PROTECTION_ECC_PROTECT    3



///
/// PAL_CACHE_PROTECTION.TagOrData.
///
#define PAL_CACHE_PROTECTION_PROTECT_DATA   0
#define PAL_CACHE_PROTECTION_PROTECT_TAG    1
#define PAL_CACHE_PROTECTION_PROTECT_TAG_ANDTHEN_DATA   2
#define PAL_CACHE_PROTECTION_PROTECT_DATA_ANDTHEN_TAG   3

///
/// 32-bit protection information structures.
///
typedef struct {
  UINT32  DataBits:8;
  UINT32  TagProtLsb:6;
  UINT32  TagProtMsb:6;
  UINT32  ProtBits:6;
  UINT32  Method:4;
  UINT32  TagOrData:2;
} PAL_CACHE_PROTECTION;

/**
  PAL Procedure - PAL_CACHE_PROT_INFO.

  Return instruction or data cache protection information. It is
  required by Itanium processors. The PAL procedure supports the Static
  Registers calling convention. It could be called at physical
  mode and Virtual mode.

  @param Index      Index of PAL_CACHE_PROT_INFO within the list of
                    PAL procedures.
  @param CacheLevel Unsigned 64-bit integer specifying the level
                    in the cache hierarchy for which information
                    is requested. This value must be between 0
                    and one less than the value returned in the
                    cache_levels return value from
                    PAL_CACHE_SUMMARY.
  @param CacheType  Unsigned 64-bit integer with a value of 1
                    for instruction cache and 2 for data or
                    unified cache. All other values are
                    reserved.

  @retval 0         Call completed without error
  @retval -2        Invalid argument
  @retval -3        Call completed with error.

  @return R9        Detail the characteristics of a given
                    processor controlled cache in the cache
                    hierarchy. See PAL_CACHE_PROTECTION[0..1].
  @return R10       Detail the characteristics of a given
                    processor controlled cache in the cache
                    hierarchy. See PAL_CACHE_PROTECTION[2..3].
  @return R11       Detail the characteristics of a given
                    processor controlled cache in the cache
                    hierarchy. See PAL_CACHE_PROTECTION[4..5].

**/
#define PAL_CACHE_PROT_INFO     38

typedef struct {
  UINT64  ThreadId : 16;    ///< The thread identifier of the logical
                            ///< processor for which information is being
                            ///< returned. This value will be unique on a per core basis.
  UINT64  Reserved1: 16;
  UINT64  CoreId: 16;       ///< The core identifier of the logical processor
                            ///< for which information is being returned.
                            ///< This value will be unique on a per physical
                            ///< processor package basis.
  UINT64  Reserved2: 16;
} PAL_PCOC_N_CACHE_INFO1;


typedef struct {
  UINT64  LogicalAddress : 16;  ///< Logical address: geographical address
                                ///< of the logical processor for which
                                ///< information is being returned. This is
                                ///< the same value that is returned by the
                                ///< PAL_FIXED_ADDR procedure when it is
                                ///< called on the logical processor.
  UINT64  Reserved1: 16;
  UINT64  Reserved2: 32;
} PAL_PCOC_N_CACHE_INFO2;

/**
  PAL Procedure - PAL_CACHE_SHARED_INFO.

  Returns information on which logical processors share caches.
  It is optional. The PAL procedure supports the Static
  Registers calling convention. It could be called at physical
  mode and Virtual mode.

  @param Index       Index of PAL_CACHE_SHARED_INFO within the list
                     of PAL procedures.
  @param CacheLevel  Unsigned 64-bit integer specifying the
                     level in the cache hierarchy for which
                     information is requested. This value must
                     be between 0 and one less than the value
                     returned in the cache_levels return value
                     from PAL_CACHE_SUMMARY.
  @param CacheType   Unsigned 64-bit integer with a value of 1
                     for instruction cache and 2 for data or
                     unified cache. All other values are
                     reserved.
  @param ProcNumber  Unsigned 64-bit integer that specifies for
                     which logical processor information is
                     being requested. This input argument must
                     be zero for the first call to this
                     procedure and can be a maximum value of
                     one less than the number of logical
                     processors sharing this cache, which is
                     returned by the num_shared return value.

  @retval 0          Call completed without error
  @retval -1         Unimplemented procedure
  @retval -2         Invalid argument
  @retval -3         Call completed with error.

  @return R9         Unsigned integer that returns the number of
                     logical processors that share the processor
                     cache level and type, for which information was
                     requested.
  @return R10        The format of PAL_PCOC_N_CACHE_INFO1.
  @return R11        The format of PAL_PCOC_N_CACHE_INFO2.

**/
#define PAL_CACHE_SHARED_INFO   43


/**
  PAL Procedure - PAL_CACHE_SUMMARY.

  Return a summary of the cache hierarchy. It is required by
  Itanium processors. The PAL procedure supports the Static Registers calling
  convention. It could be called at physical mode and Virtual
  mode.

  @param Index  Index of PAL_CACHE_SUMMARY within the list of
                PAL procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    CacheLevels   Unsigned 64-bit integer denoting the
                              number of levels of cache
                              implemented by the processor.
                              Strictly, this is the number of
                              levels for which the cache
                              controller is integrated into the
                              processor (the cache SRAMs may be
                              external to the processor).
  @return R10   UniqueCaches  Unsigned 64-bit integer denoting the
                              number of unique caches implemented
                              by the processor. This has a maximum
                              of 2*cache_levels, but may be less
                              if any of the levels in the cache
                              hierarchy are unified caches or do
                              not have both instruction and data
                              caches.

**/
#define PAL_CACHE_SUMMARY   4


//
// Virtual Memory Attributes implemented by processor.
//
#define PAL_MEMORY_ATTR_WB      0
#define PAL_MEMORY_ATTR_WC      6
#define PAL_MEMORY_ATTR_UC      4
#define PAL_MEMORY_ATTR_UCE     5
#define PAL_MEMORY_ATTR_NATPAGE 7

/**
  PAL Procedure - PAL_MEM_ATTRIB.

  Return a list of supported memory attributes.. It is required
  by Itanium processors. The PAL procedure supports the Static Registers calling
  convention. It could be called at physical mode and Virtual
  mode.

  @param Index  Index of PAL_MEM_ATTRIB within the list of PAL
                procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    Attributes  8-bit vector of memory attributes
                            implemented by processor. See Virtual
                            Memory Attributes above.

**/

#define PAL_MEM_ATTRIB      5

/**
  PAL Procedure - PAL_PREFETCH_VISIBILITY.

  Used in architected sequence to transition pages from a
  cacheable, speculative attribute to an uncacheable attribute.
  It is required by Itanium processors. The PAL procedure supports the Static
  Registers calling convention. It could be called at physical
  mode and Virtual mode.

  @param Index          Index of PAL_PREFETCH_VISIBILITY within the list
                        of PAL procedures.
  @param TransitionType Unsigned integer specifying the type
                        of memory attribute transition that is
                        being performed.

  @retval 1             Call completed without error; this
                        call is not necessary on remote
                        processors.
  @retval 0             Call completed without error
  @retval -2            Invalid argument
  @retval -3            Call completed with error.

**/
#define PAL_PREFETCH_VISIBILITY   41

/**
  PAL Procedure - PAL_PTCE_INFO.

  Return information needed for ptc.e instruction to purge
  entire TC. It is required by Itanium processors. The PAL procedure supports
  the Static Registers calling convention. It could be called at
  physical mode and Virtual mode.

  @param Index  Index of PAL_PTCE_INFO within the list
                of PAL procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    Unsigned 64-bit integer denoting the beginning
                address to be used by the first PTCE instruction
                in the purge loop.
  @return R10   Two unsigned 32-bit integers denoting the loop
                counts of the outer (loop 1) and inner (loop 2)
                purge loops. count1 (loop 1) is contained in bits
                63:32 of the parameter, and count2 (loop 2) is
                contained in bits 31:0 of the parameter.
  @return R11   Two unsigned 32-bit integers denoting the loop
                strides of the outer (loop 1) and inner (loop 2)
                purge loops. stride1 (loop 1) is contained in bits
                63:32 of the parameter, and stride2 (loop 2) is
                contained in bits 31:0 of the parameter.

**/
#define PAL_PTCE_INFO     6

typedef struct {
  UINT64  NumberSets:8;             ///< Unsigned 8-bit integer denoting the number
                                    ///< of hash sets for the specified level
                                    ///< (1=fully associative)
  UINT64  NumberWays:8;             ///< Unsigned 8-bit integer denoting the
                                    ///< associativity of the specified level
                                    ///< (1=direct).
  UINT64  NumberEntries:16;         ///< Unsigned 16-bit integer denoting the
                                    ///< number of entries in the specified TC.
  UINT64  PageSizeIsOptimized:1;    ///< Flag denoting whether the
                                    ///< specified level is optimized for
                                    ///< the region's preferred page size
                                    ///< (1=optimized) tc_pages indicates
                                    ///< which page sizes are usable by
                                    ///< this translation cache.
  UINT64  TcIsUnified:1;            ///< Flag denoting whether the specified TC is
                                    ///< unified (1=unified).
  UINT64  EntriesReduction:1;       ///< Flag denoting whether installed
                                    ///< translation registers will reduce
                                    ///< the number of entries within the
                                    ///< specified TC.
} PAL_TC_INFO;

/**
  PAL Procedure - PAL_VM_INFO.

  Return detailed information about virtual memory features
  supported in the processor. It is required by Itanium processors. The PAL
  procedure supports the Static Registers calling convention. It
  could be called at physical mode and Virtual mode.

  @param Index    Index of PAL_VM_INFO within the list
                  of PAL procedures.
  @param TcLevel  Unsigned 64-bit integer specifying the level
                  in the TLB hierarchy for which information is
                  required. This value must be between 0 and one
                  less than the value returned in the
                  vm_info_1.num_tc_levels return value from
                  PAL_VM_SUMMARY.
  @param TcType   Unsigned 64-bit integer with a value of 1 for
                  instruction translation cache and 2 for data
                  or unified translation cache. All other values
                  are reserved.

  @retval 0       Call completed without error
  @retval -2      Invalid argument
  @retval -3      Call completed with error.

  @return R9      8-byte formatted value returning information
                  about the specified TC. See PAL_TC_INFO above.
  @return R10     64-bit vector containing a bit for each page
                  size supported in the specified TC, where bit
                  position n indicates a page size of 2**n.

**/
#define PAL_VM_INFO       7


/**
  PAL Procedure - PAL_VM_PAGE_SIZE.

  Return virtual memory TC and hardware walker page sizes
  supported in the processor. It is required by Itanium processors. The PAL
  procedure supports the Static Registers calling convention. It
  could be called at physical mode and Virtual mode.

  @param Index  Index of PAL_VM_PAGE_SIZE within the list
                of PAL procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    64-bit vector containing a bit for each
                architected page size that is supported for
                TLB insertions and region registers.
  @return R10   64-bit vector containing a bit for each
                architected page size supported for TLB purge
                operations.

**/
#define PAL_VM_PAGE_SIZE 34

typedef struct {
  UINT64  WalkerPresent:1;              ///< 1-bit flag indicating whether a hardware
                                        ///< TLB walker is implemented (1 = walker
                                        ///< present).
  UINT64  WidthOfPhysicalAddress: 7;    ///< Unsigned 7-bit integer
                                        ///< denoting the number of bits of
                                        ///< physical address implemented.
  UINT64  WidthOfKey:8;                 ///< Unsigned 8-bit integer denoting the number
                                        ///< of bits mplemented in the PKR.key field.
  UINT64  MaxPkrIndex:8;                ///< Unsigned 8-bit integer denoting the
                                        ///< maximum PKR index (number of PKRs-1).
  UINT64  HashTagId:8;                  ///< Unsigned 8-bit integer which uniquely
                                        ///< identifies the processor hash and tag
                                        ///< algorithm.
  UINT64  MaxDtrIndex:8;                ///< Unsigned 8 bit integer denoting the
                                        ///< maximum data translation register index
                                        ///< (number of dtr entries - 1).
  UINT64  MaxItrIndex:8;                ///< Unsigned 8 bit integer denoting the
                                        ///< maximum instruction translation register
                                        ///< index (number of itr entries - 1).
  UINT64  NumberOfUniqueTc:8;           ///< Unsigned 8-bit integer denoting the
                                        ///< number of unique TCs implemented.
                                        ///< This is a maximum of
                                        ///< 2*num_tc_levels.
  UINT64  NumberOfTcLevels:8;           ///< Unsigned 8-bit integer denoting the
                                        ///< number of TC levels.
} PAL_VM_INFO1;

typedef struct {
  UINT64  WidthOfVirtualAddress:8;  ///< Unsigned 8-bit integer denoting
                                    ///< is the total number of virtual
                                    ///< address bits - 1.
  UINT64  WidthOfRid:8;             ///< Unsigned 8-bit integer denoting the number
                                    ///< of bits implemented in the RR.rid field.
  UINT64  MaxPurgedTlbs:16;         ///< Unsigned 16 bit integer denoting the
                                    ///< maximum number of concurrent outstanding
                                    ///< TLB purges allowed by the processor. A
                                    ///< value of 0 indicates one outstanding
                                    ///< purge allowed. A value of 216-1
                                    ///< indicates no limit on outstanding
                                    ///< purges. All other values indicate the
                                    ///< actual number of concurrent outstanding
                                    ///< purges allowed.
  UINT64  Reserved:32;
} PAL_VM_INFO2;

/**
  PAL Procedure - PAL_VM_SUMMARY.

  Return summary information about virtual memory features
  supported in the processor. It is required by Itanium processors. The PAL
  procedure supports the Static Registers calling convention. It
  could be called at physical mode and Virtual mode.

  @param Index  Index of PAL_VM_SUMMARY within the list
                of PAL procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    8-byte formatted value returning global virtual
                memory information. See PAL_VM_INFO1 above.
  @return R10   8-byte formatted value returning global virtual
                memory information. See PAL_VM_INFO2 above.

**/
#define PAL_VM_SUMMARY  8


//
// Bit mask of TR_valid flag.
//
#define PAL_TR_ACCESS_RIGHT_IS_VALID      BIT0
#define PAL_TR_PRIVILEGE_LEVEL_IS_VALID   BIT1
#define PAL_TR_DIRTY_IS_VALID             BIT2
#define PAL_TR_MEMORY_ATTR_IS_VALID       BIT3


/**
  PAL Procedure - PAL_VM_TR_READ.

  Read contents of a translation register. It is required by
  Itanium processors. The PAL procedure supports the Stacked Register calling
  convention. It could be called at physical mode.

  @param Index      Index of PAL_VM_TR_READ within the list
                    of PAL procedures.
  @param RegNumber  Unsigned 64-bit number denoting which TR to
                    read.
  @param TrType     Unsigned 64-bit number denoting whether to
                    read an ITR (0) or DTR (1). All other values
                    are reserved.
  @param TrBuffer   64-bit pointer to the 32-byte memory buffer in
                    which translation data is returned.

  @retval 0         Call completed without error
  @retval -2        Invalid argument
  @retval -3        Call completed with error.

  @return R9        Formatted bit vector denoting which fields are
                    valid. See TR_valid above.

**/
#define PAL_VM_TR_READ  261




//
// Bit Mask of Processor Bus Fesatures .
//

/**

  When 0, bus data errors are detected and single bit errors are
  corrected. When 1, no error detection or correction is done.

**/
#define PAL_BUS_DISABLE_DATA_ERROR_SIGNALLING   BIT63


/**

  When 0, bus address errors are signalled on the bus. When 1,
  no bus errors are signalled on the bus. If Disable Bus Address
  Error Checking is 1, this bit is ignored.

**/
#define PAL_BUS_DISABLE_ADDRESS_ERROR_SIGNALLING   BIT62




/**

  When 0, bus errors are detected, single bit errors are
  corrected., and a CMCI or MCA is generated internally to the
  processor. When 1, no bus address errors are detected or
  corrected.

**/
#define PAL_BUS_DISABLE_ADDRESS_ERROR_CHECK   BIT61


/**

  When 0, bus protocol errors (BINIT#) are signaled by the
  processor on the bus. When 1, bus protocol errors (BINIT#) are
  not signaled on the bus. If Disable Bus Initialization Event
  Checking is 1, this bit is ignored.

**/
#define PAL_BUS_DISABLE_INITIALIZATION_EVENT_SIGNALLING   BIT60


/**

  When 0, bus protocol errors (BINIT#) are detected and sampled
  and an MCA is generated internally to the processor. When 1,
  the processor will ignore bus protocol error conditions
  (BINIT#).

**/
#define PAL_BUS_DISABLE_INITIALIZATION_EVENT_CHECK   BIT59



/**

  When 0, BERR# is signalled if a bus error is detected. When 1,
  bus errors are not signalled on the bus.

**/
#define PAL_BUS_DISABLE_ERROR_SIGNALLING   BIT58




/**

  When 0, BERR# is signalled when internal processor requestor
  initiated bus errors are detected. When 1, internal requester
  bus errors are not signalled on the bus.

**/
#define PAL_BUS_DISABLE__INTERNAL_ERROR_SIGNALLING   BIT57


/**

  When 0, the processor takes an MCA if BERR# is asserted. When
  1, the processor ignores the BERR# signal.

**/
#define PAL_BUS_DISABLE_ERROR_CHECK   BIT56


/**

  When 0, the processor asserts BINIT# if it detects a parity
  error on the signals which identify the transactions to which
  this is a response. When 1, the processor ignores parity on
  these signals.

**/
#define PAL_BUS_DISABLE_RSP_ERROR_CHECK   BIT55


/**

  When 0, the in-order transaction queue is limited only by the
  number of hardware entries. When 1, the processor's in-order
  transactions queue is limited to one entry.

**/
#define PAL_BUS_DISABLE_TRANSACTION_QUEUE   BIT54

/**

  Enable a bus cache line replacement transaction when a cache
  line in the exclusive state is replaced from the highest level
  processor cache and is not present in the lower level processor
  caches. When 0, no bus cache line replacement transaction will
  be seen on the bus. When 1, bus cache line replacement
  transactions will be seen on the bus when the above condition is
  detected.

**/
#define PAL_BUS_ENABLE_EXCLUSIVE_CACHE_LINE_REPLACEMENT   BIT53


/**

  Enable a bus cache line replacement transaction when a cache
  line in the shared or exclusive state is replaced from the
  highest level processor cache and is not present in the lower
  level processor caches.
  When 0, no bus cache line replacement transaction will be seen
  on the bus. When 1, bus cache line replacement transactions
  will be seen on the bus when the above condition is detected.

**/
#define PAL_BUS_ENABLE_SHARED_CACHE_LINE_REPLACEMENT   BIT52



/**

  When 0, the data bus is configured at the 2x data transfer
  rate.When 1, the data bus is configured at the 1x data
  transfer rate, 30 Opt. Req. Disable Bus Lock Mask. When 0, the
  processor executes locked transactions atomically. When 1, the
  processor masks the bus lock signal and executes locked
  transactions as a non-atomic series of transactions.

**/
#define PAL_BUS_ENABLE_HALF_TRANSFER   BIT30

/**

  When 0, the processor will deassert bus request when finished
  with each transaction. When 1, the processor will continue to
  assert bus request after it has finished, if it was the last
  agent to own the bus and if there are no other pending
  requests.

**/
#define PAL_BUS_REQUEST_BUS_PARKING   BIT29


/**
  PAL Procedure - PAL_BUS_GET_FEATURES.

  Return configurable processor bus interface features and their
  current settings. It is required by Itanium processors. The PAL procedure
  supports the Stacked Register calling convention. It could be
  called at physical mode.

  @param Index  Index of PAL_BUS_GET_FEATURES within the list
                of PAL procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    64-bit vector of features implemented.
                (1=implemented, 0=not implemented)
  @return R10   64-bit vector of current feature settings.
  @return R11   64-bit vector of features controllable by
                software. (1=controllable, 0= not controllable)

**/
#define PAL_BUS_GET_FEATURES 9

/**
  PAL Procedure - PAL_BUS_SET_FEATURES.

  Enable or disable configurable features in processor bus
  interface. It is required by Itanium processors. The PAL procedure
  supports the Static Registers calling convention. It could be
  called at physical mode.

  @param Index          Index of PAL_BUS_SET_FEATURES within the list
                        of PAL procedures.
  @param FeatureSelect  64-bit vector denoting desired state of
                        each feature (1=select, 0=non-select).

  @retval 0             Call completed without error
  @retval -2            Invalid argument
  @retval -3            Call completed with error.

**/
#define PAL_BUS_SET_FEATURES 10


/**
  PAL Procedure - PAL_DEBUG_INFO.

  Return the number of instruction and data breakpoint
  registers. It is required by Itanium processors. The
  PAL procedure supports the Static Registers calling
  convention. It could be called at physical mode and virtual
  mode.

  @param Index  Index of PAL_DEBUG_INFO within the list of PAL
                procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    Unsigned 64-bit integer denoting the number of
                pairs of instruction debug registers implemented
                by the processor.
  @return R10   Unsigned 64-bit integer denoting the number of
                pairs of data debug registers implemented by the
                processor.

**/
#define PAL_DEBUG_INFO  11

/**
  PAL Procedure - PAL_FIXED_ADDR.

  Return the fixed component of a processor's directed address.
  It is required by Itanium processors. The PAL
  procedure supports the Static Registers calling convention. It
  could be called at physical mode and virtual mode.

  @param Index  Index of PAL_FIXED_ADDR within the list of PAL
                procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    Fixed geographical address of this processor.

**/
#define PAL_FIXED_ADDR 12

/**
  PAL Procedure - PAL_FREQ_BASE.

  Return the frequency of the output clock for use by the
  platform, if generated by the processor. It is optinal. The
  PAL procedure supports the Static Registers calling
  convention. It could be called at physical mode and virtual
  mode.

  @param Index  Index of PAL_FREQ_BASE within the list of PAL
                procedures.

  @retval 0     Call completed without error
  @retval -1    Unimplemented procedure
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    Base frequency of the platform if generated by the
                processor chip.

**/
#define PAL_FREQ_BASE 13


/**
  PAL Procedure - PAL_FREQ_RATIOS.

  Return ratio of processor, bus, and interval time counter to
  processor input clock or output clock for platform use, if
  generated by the processor. It is required by Itanium processors. The PAL
  procedure supports the Static Registers calling convention. It
  could be called at physical mode and virtual mode.

  @param Index  Index of PAL_FREQ_RATIOS within the list of PAL
                procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    Ratio of the processor frequency to the input
                clock of the processor, if the platform clock is
                generated externally or to the output clock to the
                platform, if the platform clock is generated by
                the processor.
  @return R10   Ratio of the bus frequency to the input clock of
                the processor, if the platform clock is generated
                externally or to the output clock to the platform,
                if the platform clock is generated by the
                processor.
  @return R11   Ratio of the interval timer counter rate to input
                clock of the processor, if the platform clock is
                generated externally or to the output clock to the
                platform, if the platform clock is generated by
                the processor.

**/
#define PAL_FREQ_RATIOS 14

typedef struct {
  UINT64  NumberOfLogicalProcessors:16;     ///< Total number of logical
                                            ///< processors on this physical
                                            ///< processor package that are
                                            ///< enabled.
  UINT64  ThreadsPerCore:8;                 ///< Number of threads per core.
  UINT64  Reserved1:8;
  UINT64  CoresPerProcessor:8;              ///< Total number of cores on this
                                            ///< physical processor package.
  UINT64  Reserved2:8;
  UINT64  PhysicalProcessorPackageId:8;     ///< Physical processor package
                                            ///< identifier which was
                                            ///< assigned at reset by the
                                            ///< platform or bus
                                            ///< controller. This value may
                                            ///< or may not be unique
                                            ///< across the entire platform
                                            ///< since it depends on the
                                            ///< platform vendor's policy.
  UINT64  Reserved3:8;
} PAL_LOGICAL_PROCESSPR_OVERVIEW;

typedef struct {
   UINT64 ThreadId:16;      ///< The thread identifier of the logical
                            ///< processor for which information is being
                            ///< returned. This value will be unique on a per
                            ///< core basis.
   UINT64 Reserved1:16;
   UINT64 CoreId:16;        ///< The core identifier of the logical processor
                            ///< for which information is being returned.
                            ///< This value will be unique on a per physical
                            ///< processor package basis.
   UINT64 Reserved2:16;
} PAL_LOGICAL_PROCESSORN_INFO1;

typedef struct {
   UINT64 LogicalAddress:16;    ///< Geographical address of the logical
                                ///< processor for which information is being
                                ///< returned. This is the same value that is
                                ///< returned by the PAL_FIXED_ADDR procedure
                                ///< when it is called on the logical processor.
   UINT64 Reserved:48;
} PAL_LOGICAL_PROCESSORN_INFO2;

/**
  PAL Procedure - PAL_LOGICAL_TO_PHYSICAL.

  Return information on which logical processors map to a
  physical processor die. It is optinal. The PAL procedure
  supports the Static Registers calling convention. It could be
  called at physical mode and virtual mode.

  @param Index            Index of PAL_LOGICAL_TO_PHYSICAL within the list of PAL
                          procedures.
  @param ProcessorNumber  Signed 64-bit integer that specifies
                          for which logical processor
                          information is being requested. When
                          this input argument is -1, information
                          is returned about the logical
                          processor on which the procedure call
                          is made. This input argument must be
                          in the range of 1 up to one less than
                          the number of logical processors
                          returned by num_log in the
                          log_overview return value.

  @retval 0               Call completed without error
  @retval -1              Unimplemented procedure
  @retval -2              Invalid argument
  @retval -3              Call completed with error.

  @return R9              The format of PAL_LOGICAL_PROCESSPR_OVERVIEW.
  @return R10             The format of PAL_LOGICAL_PROCESSORN_INFO1.
  @return R11             The format of PAL_LOGICAL_PROCESSORN_INFO2.

**/
#define PAL_LOGICAL_TO_PHYSICAL 42

typedef struct {
  UINT64  NumberOfPmcPairs:8;               ///< Unsigned 8-bit number defining the
                                            ///< number of generic PMC/PMD pairs.
  UINT64  WidthOfCounter:8;                 ///< Unsigned 8-bit number in the range
                                            ///< 0:60 defining the number of
                                            ///< implemented counter bits.
  UINT64  TypeOfCycleCounting:8;            ///< Unsigned 8-bit number defining the
                                            ///< event type for counting processor cycles.
  UINT64  TypeOfRetiredInstructionBundle:8; ///< Retired Unsigned 8-bit
                                            ///< number defining the
                                            ///< event type for retired
                                            ///< instruction bundles.
  UINT64  Reserved:32;
} PAL_PERFORMANCE_INFO;

/**
  PAL Procedure - PAL_PERF_MON_INFO.

  Return the number and type of performance monitors. It is
  required by Itanium processors. The PAL procedure supports the Static
  Registers calling convention. It could be called at physical
  mode and virtual mode.

  @param Index              Index of PAL_PERF_MON_INFO within the list of
                            PAL procedures.
  @param PerformanceBuffer  An address to an 8-byte aligned
                            128-byte memory buffer.

  @retval 0                 Call completed without error
  @retval -2                Invalid argument
  @retval -3                Call completed with error.

  @return R9                Information about the performance monitors
                            implemented. See PAL_PERFORMANCE_INFO;

**/
#define PAL_PERF_MON_INFO 15

#define PAL_PLATFORM_ADDR_INTERRUPT_BLOCK_TOKEN                       0x0
#define PAL_PLATFORM_ADDR_IO_BLOCK_TOKEN                              0x1

/**
  PAL Procedure - PAL_PLATFORM_ADDR.

  Specify processor interrupt block address and I/O port space
  address. It is required by Itanium processors. The PAL procedure supports the
  Static Registers calling convention. It could be called at
  physical mode and virtual mode.

  @param Index    Index of PAL_PLATFORM_ADDR within the list of
                  PAL procedures.
  @param Type     Unsigned 64-bit integer specifying the type of
                  block. 0 indicates that the processor interrupt
                  block pointer should be initialized. 1 indicates
                  that the processor I/O block pointer should be
                  initialized.
  @param Address  Unsigned 64-bit integer specifying the address
                  to which the processor I/O block or interrupt
                  block shall be set. The address must specify
                  an implemented physical address on the
                  processor model, bit 63 is ignored.

  @retval 0       Call completed without error
  @retval -1      Unimplemented procedure.
  @retval -2      Invalid argument
  @retval -3      Call completed with error.

**/
#define PAL_PLATFORM_ADDR 16

typedef struct {
  UINT64  Reserved1:36;
  UINT64  FaultInUndefinedIns:1;                ///< Bit36, No Unimplemented
                                                ///< instruction address reported as
                                                ///< fault. Denotes how the processor
                                                ///< reports the detection of
                                                ///< unimplemented instruction
                                                ///< addresses. When 1, the processor
                                                ///< reports an Unimplemented
                                                ///< Instruction Address fault on the
                                                ///< unimplemented address; when 0, it
                                                ///< reports an Unimplemented
                                                ///< Instruction Address trap on the
                                                ///< previous instruction in program
                                                ///< order. This feature may only be
                                                ///< interrogated by
                                                ///< PAL_PROC_GET_FEATURES. It may not
                                                ///< be enabled or disabled by
                                                ///< PAL_PROC_SET_FEATURES. The
                                                ///< corresponding argument is ignored.
                                                
  UINT64  NoPresentPmi:1;                       ///< Bit37, No INIT, PMI, and LINT pins
                                                ///< present. Denotes the absence of INIT,
                                                ///< PMI, LINT0 and LINT1 pins on the
                                                ///< processor. When 1, the pins are absent.
                                                ///< When 0, the pins are present. This
                                                ///< feature may only be interrogated by
                                                ///< PAL_PROC_GET_FEATURES. It may not be
                                                ///< enabled or disabled by
                                                ///< PAL_PROC_SET_FEATURES. The corresponding
                                                ///< argument is ignored.
                                                
  UINT64  NoSimpleImpInUndefinedIns:1;          ///< Bit38, No Simple
                                                ///< implementation of
                                                ///< unimplemented instruction
                                                ///< addresses. Denotes how an
                                                ///< unimplemented instruction
                                                ///< address is recorded in IIP
                                                ///< on an Unimplemented
                                                ///< Instruction Address trap or
                                                ///< fault. When 1, the full
                                                ///< unimplemented address is
                                                ///< recorded in IIP; when 0, the
                                                ///< address is sign extended
                                                ///< (virtual addresses) or zero
                                                ///< extended (physical
                                                ///< addresses). This feature may
                                                ///< only be interrogated by
                                                ///< PAL_PROC_GET_FEATURES. It
                                                ///< may not be enabled or
                                                ///< disabled by
                                                ///< PAL_PROC_SET_FEATURES. The
                                                ///< corresponding argument is
                                                ///< ignored.

  UINT64  NoVariablePState:1;                   ///< Bit39, No Variable P-state
                                                ///< performance: A value of 1, indicates
                                                ///< that a processor implements
                                                ///< techniques to optimize performance
                                                ///< for the given P-state power budget
                                                ///< by dynamically varying the
                                                ///< frequency, such that maximum
                                                ///< performance is achieved for the
                                                ///< power budget. A value of 0,
                                                ///< indicates that P-states have no
                                                ///< frequency variation or very small
                                                ///< frequency variations for their given
                                                ///< power budget. This feature may only
                                                ///< be interrogated by
                                                ///< PAL_PROC_GET_FEATURES. it may not be
                                                ///< enabled or disabled by
                                                ///< PAL_PROC_SET_FEATURES. The
                                                ///< corresponding argument is ignored.

  UINT64  NoVM:1;                               ///< Bit40, No Virtual Machine features implemented.
                                                ///< Denotes whether PSR.vm is implemented. This
                                                ///< feature may only be interrogated by
                                                ///< PAL_PROC_GET_FEATURES. It may not be enabled or
                                                ///< disabled by PAL_PROC_SET_FEATURES. The
                                                ///< corresponding argument is ignored.

  UINT64  NoXipXpsrXfs:1;                       ///< Bit41, No XIP, XPSR, and XFS
                                                ///< implemented. Denotes whether XIP, XPSR,
                                                ///< and XFS are implemented for machine
                                                ///< check recovery. This feature may only be
                                                ///< interrogated by PAL_PROC_GET_FEATURES.
                                                ///< It may not be enabled or disabled by
                                                ///< PAL_PROC_SET_FEATURES. The corresponding
                                                ///< argument is ignored.

  UINT64  NoXr1ThroughXr3:1;                    ///< Bit42, No XR1 through XR3 implemented.
                                                ///<   Denotes whether XR1 XR3 are
                                                ///<   implemented for machine check
                                                ///<   recovery. This feature may only be
                                                ///<   interrogated by PAL_PROC_GET_FEATURES.
                                                ///<   It may not be enabled or disabled by
                                                ///<   PAL_PROC_SET_FEATURES. The
                                                ///<   corresponding argument is ignored.

  UINT64  DisableDynamicPrediction:1;           ///< Bit43, Disable Dynamic
                                                ///< Predicate Prediction. When
                                                ///< 0, the processor may predict
                                                ///< predicate results and
                                                ///< execute speculatively, but
                                                ///< may not commit results until
                                                ///< the actual predicates are
                                                ///< known. When 1, the processor
                                                ///< shall not execute predicated
                                                ///< instructions until the
                                                ///< actual predicates are known.

  UINT64  DisableSpontaneousDeferral:1;         ///< Bit44, Disable Spontaneous
                                                ///<   Deferral. When 1, the
                                                ///<   processor may optionally
                                                ///<   defer speculative loads
                                                ///<   that do not encounter any
                                                ///<   exception conditions, but
                                                ///<   that trigger other
                                                ///<   implementation-dependent
                                                ///<   conditions (e.g., cache
                                                ///<   miss). When 0, spontaneous
                                                ///<   deferral is disabled.

  UINT64  DisableDynamicDataCachePrefetch:1;    ///< Bit45, Disable Dynamic
                                                ///<   Data Cache Prefetch.
                                                ///<   When 0, the processor
                                                ///<   may prefetch into the
                                                ///<   caches any data which
                                                ///<   has not been accessed
                                                ///<   by instruction
                                                ///<   execution, but which
                                                ///<   is likely to be
                                                ///<   accessed. When 1, no
                                                ///<   data may be fetched
                                                ///<   until it is needed for
                                                ///<   instruction execution
                                                ///<   or is fetched by an
                                                ///<   lfetch instruction.

  UINT64  DisableDynamicInsCachePrefetch:1;     ///< Bit46, Disable
                                                ///< DynamicInstruction Cache
                                                ///< Prefetch. When 0, the
                                                ///< processor may prefetch
                                                ///< into the caches any
                                                ///< instruction which has
                                                ///< not been executed, but
                                                ///< whose execution is
                                                ///< likely. When 1,
                                                ///< instructions may not be
                                                ///< fetched until needed or
                                                ///< hinted for execution.
                                                ///< (Prefetch for a hinted
                                                ///< branch is allowed even
                                                ///< when dynamic instruction
                                                ///< cache prefetch is
                                                ///< disabled.)

  UINT64  DisableBranchPrediction:1;            ///< Bit47, Disable Dynamic branch
                                                ///<   prediction. When 0, the
                                                ///<   processor may predict branch
                                                ///<   targets and speculatively
                                                ///<   execute, but may not commit
                                                ///<   results. When 1, the processor
                                                ///<   must wait until branch targets
                                                ///<   are known to execute.
  UINT64  Reserved2:4;
  UINT64  DisablePState:1;                      ///< Bit52, Disable P-states. When 1, the PAL
                                                ///< P-state procedures (PAL_PSTATE_INFO,
                                                ///< PAL_SET_PSTATE, PAL_GET_PSTATE) will
                                                ///< return with a status of -1
                                                ///< (Unimplemented procedure).

  UINT64  EnableMcaOnDataPoisoning:1;           ///< Bit53, Enable MCA signaling
                                                ///< on data-poisoning event
                                                ///< detection. When 0, a CMCI
                                                ///< will be signaled on error
                                                ///< detection. When 1, an MCA
                                                ///< will be signaled on error
                                                ///< detection. If this feature
                                                ///< is not supported, then the
                                                ///< corresponding argument is
                                                ///< ignored when calling
                                                ///< PAL_PROC_SET_FEATURES. Note
                                                ///< that the functionality of
                                                ///< this bit is independent of
                                                ///< the setting in bit 60
                                                ///< (Enable CMCI promotion), and
                                                ///< that the bit 60 setting does
                                                ///< not affect CMCI signaling
                                                ///< for data-poisoning related
                                                ///< events. Volume 2: Processor
                                                ///< Abstraction Layer 2:431
                                                ///< PAL_PROC_GET_FEATURES

  UINT64  EnableVmsw:1;                         ///< Bit54, Enable the use of the vmsw
                                                ///<   instruction. When 0, the vmsw instruction
                                                ///<   causes a Virtualization fault when
                                                ///<   executed at the most privileged level.
                                                ///<   When 1, this bit will enable normal
                                                ///<   operation of the vmsw instruction.

  UINT64  EnableEnvNotification:1;              ///< Bit55, Enable external
                                                ///< notification when the processor
                                                ///< detects hardware errors caused
                                                ///< by environmental factors that
                                                ///< could cause loss of
                                                ///< deterministic behavior of the
                                                ///< processor. When 1, this bit will
                                                ///< enable external notification,
                                                ///< when 0 external notification is
                                                ///< not provided. The type of
                                                ///< external notification of these
                                                ///< errors is processor-dependent. A
                                                ///< loss of processor deterministic
                                                ///< behavior is considered to have
                                                ///< occurred if these
                                                ///< environmentally induced errors
                                                ///< cause the processor to deviate
                                                ///< from its normal execution and
                                                ///< eventually causes different
                                                ///< behavior which can be observed
                                                ///<  at the processor bus pins.
                                                ///< Processor errors that do not
                                                ///< have this effects (i.e.,
                                                ///< software induced machine checks)
                                                ///< may or may not be promoted
                                                ///< depending on the processor
                                                ///< implementation.

  UINT64  DisableBinitWithTimeout:1;            ///< Bit56, Disable a BINIT on
                                                ///<   internal processor time-out.
                                                ///<   When 0, the processor may
                                                ///<   generate a BINIT on an
                                                ///<   internal processor time-out.
                                                ///<   When 1, the processor will not
                                                ///<   generate a BINIT on an
                                                ///<   internal processor time-out.
                                                ///<   The event is silently ignored.

  UINT64  DisableDPM:1;                         ///< Bit57, Disable Dynamic Power Management
                                                ///<   (DPM). When 0, the hardware may reduce
                                                ///<   power consumption by removing the clock
                                                ///<   input from idle functional units. When 1,
                                                ///<   all functional units will receive clock
                                                ///<   input, even when idle.

  UINT64  DisableCoherency:1;                   ///< Bit58, Disable Coherency. When 0,
                                                ///< the processor uses normal coherency
                                                ///< requests and responses. When 1, the
                                                ///< processor answers all requests as if
                                                ///< the line were not present.

  UINT64  DisableCache:1;                       ///< Bit59, Disable Cache. When 0, the
                                                ///< processor performs cast outs on
                                                ///< cacheable pages and issues and responds
                                                ///< to coherency requests normally. When 1,
                                                ///< the processor performs a memory access
                                                ///< for each reference regardless of cache
                                                ///< contents and issues no coherence
                                                ///< requests and responds as if the line
                                                ///< were not present. Cache contents cannot
                                                ///< be relied upon when the cache is
                                                ///< disabled. WARNING: Semaphore
                                                ///< instructions may not be atomic or may
                                                ///< cause Unsupported Data Reference faults
                                                ///< if caches are disabled.

  UINT64  EnableCmciPromotion:1;                ///< Bit60, Enable CMCI promotion When
                                                ///<   1, Corrected Machine Check
                                                ///<   Interrupts (CMCI) are promoted to
                                                ///<   MCAs. They are also further
                                                ///<   promoted to BERR if bit 39, Enable
                                                ///<   MCA promotion, is also set and
                                                ///<   they are promoted to BINIT if bit
                                                ///<   38, Enable MCA to BINIT promotion,
                                                ///<   is also set. This bit has no
                                                ///<   effect if MCA signalling is
                                                ///<   disabled (see
                                                ///<   PAL_BUS_GET/SET_FEATURES)

  UINT64  EnableMcaToBinitPromotion:1;          ///< Bit61, Enable MCA to BINIT
                                                ///< promotion. When 1, machine
                                                ///< check aborts (MCAs) are
                                                ///< promoted to the Bus
                                                ///< Initialization signal, and
                                                ///< the BINIT pin is assert on
                                                ///< each occurrence of an MCA.
                                                ///< Setting this bit has no
                                                ///< effect if BINIT signalling
                                                ///< is disabled. (See
                                                ///< PAL_BUS_GET/SET_FEATURES)

  UINT64  EnableMcaPromotion:1;                 ///< Bit62, Enable MCA promotion. When
                                                ///<   1, machine check aborts (MCAs) are
                                                ///<   promoted to the Bus Error signal,
                                                ///<   and the BERR pin is assert on each
                                                ///<   occurrence of an MCA. Setting this
                                                ///<   bit has no effect if BERR
                                                ///<   signalling is disabled. (See
                                                ///<   PAL_BUS_GET/SET_FEATURES)
                                                
  UINT64  EnableBerrPromotion:1;                ///< Bit63. Enable BERR promotion. When
                                                ///<   1, the Bus Error (BERR) signal is
                                                ///<   promoted to the Bus Initialization
                                                ///<   (BINIT) signal, and the BINIT pin
                                                ///<   is asserted on the occurrence of
                                                ///<   each Bus Error. Setting this bit
                                                ///<   has no effect if BINIT signalling
                                                ///<   is disabled. (See
                                                ///<   PAL_BUS_GET/SET_FEATURES)
} PAL_PROCESSOR_FEATURES;

/**
  PAL Procedure - PAL_PROC_GET_FEATURES.

  Return configurable processor features and their current
  setting. It is required by Itanium processors. The PAL procedure supports the
  Static Registers calling convention. It could be called at
  physical mode and virtual mode.

  @param Index      Index of PAL_PROC_GET_FEATURES within the list of
                    PAL procedures.
  @param Reserved   Reserved parameter.
  @param FeatureSet Feature set information is being requested
                    for.

  @retval 1         Call completed without error; The
                    feature_set passed is not supported but a
                    feature_set of a larger value is supported.
  @retval 0         Call completed without error
  @retval -2        Invalid argument
  @retval -3        Call completed with error.
  @retval -8        feature_set passed is beyond the maximum
                    feature_set supported

  @return R9        64-bit vector of features implemented. See
                    PAL_PROCESSOR_FEATURES.
  @return R10       64-bit vector of current feature settings. See
                    PAL_PROCESSOR_FEATURES.
  @return R11       64-bit vector of features controllable by
                    software.

**/
#define PAL_PROC_GET_FEATURES 17


/**
  PAL Procedure - PAL_PROC_SET_FEATURES.

  Enable or disable configurable processor features. It is
  required by Itanium processors. The PAL procedure supports the Static
  Registers calling convention. It could be called at physical
  mode.

  @param Index          Index of PAL_PROC_SET_FEATURES within the list of
                        PAL procedures.
  @param FeatureSelect  64-bit vector denoting desired state of
                        each feature (1=select, 0=non-select).
  @param FeatureSet     Feature set to apply changes to. See
                        PAL_PROC_GET_FEATURES for more information
                        on feature sets.

  @retval 1             Call completed without error; The
                        feature_set passed is not supported but a
                        feature_set of a larger value is supported
  @retval 0             Call completed without error
  @retval -2            Invalid argument
  @retval -3            Call completed with error.
  @retval -8            feature_set passed is beyond the maximum
                        feature_set supported

**/
#define PAL_PROC_SET_FEATURES 18


//
// Value of PAL_REGISTER_INFO.InfoRequest.
//
#define PAL_APPLICATION_REGISTER_IMPLEMENTED  0
#define PAL_APPLICATION_REGISTER_READABLE     1
#define PAL_CONTROL_REGISTER_IMPLEMENTED      2
#define PAL_CONTROL_REGISTER_READABLE         3


/**
  PAL Procedure - PAL_REGISTER_INFO.

  Return AR and CR register information. It is required by Itanium processors.
  The PAL procedure supports the Static Registers calling
  convention. It could be called at physical mode and virtual
  mode.

  @param Index        Index of PAL_REGISTER_INFO within the list of
                      PAL procedures.
  @param InfoRequest  Unsigned 64-bit integer denoting what
                      register information is requested. See
                      PAL_REGISTER_INFO.InfoRequest above.

  @retval 0           Call completed without error
  @retval -2          Invalid argument
  @retval -3          Call completed with error.

  @return R9          64-bit vector denoting information for registers
                      0-63. Bit 0 is register 0, bit 63 is register 63.
  @return R10         64-bit vector denoting information for registers
                      64-127. Bit 0 is register 64, bit 63 is register
                      127.

**/
#define PAL_REGISTER_INFO 39

/**
  PAL Procedure - PAL_RSE_INFO.

  Return RSE information. It is required by Itanium processors. The PAL
  procedure supports the Static Registers calling convention. It
  could be called at physical mode and virtual mode.

  @param Index        Index of PAL_RSE_INFO within the list of
                      PAL procedures.
  @param InfoRequest  Unsigned 64-bit integer denoting what
                      register information is requested. See
                      PAL_REGISTER_INFO.InfoRequest above.

  @retval 0           Call completed without error
  @retval -2          Invalid argument
  @retval -3          Call completed with error.

  @return R9          Number of physical stacked general registers.
  @return R10         RSE hints supported by processor.

**/
#define PAL_RSE_INFO 19

typedef struct {
  UINT64  VersionOfPalB:16;     ///< Is a 16-bit binary coded decimal (BCD)
                                ///< number that provides identification
                                ///< information about the PAL_B firmware.
  UINT64  Reserved1:8;
  UINT64  PalVendor:8;          ///< Is an unsigned 8-bit integer indicating the
                                ///< vendor of the PAL code.
  UINT64  VersionOfPalA:16;     ///< Is a 16-bit binary coded decimal (BCD)
                                ///< number that provides identification
                                ///< information about the PAL_A firmware. In
                                ///< the split PAL_A model, this return value
                                ///< is the version number of the
                                ///< processor-specific PAL_A. The generic
                                ///< PAL_A version is not returned by this
                                ///< procedure in the split PAL_A model.
  UINT64  Reserved2:16;
} PAL_VERSION_INFO;

/**
  PAL Procedure - PAL_VERSION.

  Return version of PAL code. It is required by Itanium processors. The PAL
  procedure supports the Static Registers calling convention. It
  could be called at physical mode and virtual mode.

  @param Index        Index of PAL_VERSION within the list of
                      PAL procedures.
  @param InfoRequest  Unsigned 64-bit integer denoting what
                      register information is requested. See
                      PAL_REGISTER_INFO.InfoRequest above.

  @retval 0           Call completed without error
  @retval -2          Invalid argument
  @retval -3          Call completed with error.

  @return R9          8-byte formatted value returning the minimum PAL
                      version needed for proper operation of the
                      processor. See PAL_VERSION_INFO above.
  @return R10         8-byte formatted value returning the current PAL
                      version running on the processor. See
                      PAL_VERSION_INFO above.

**/
#define PAL_VERSION 20



//
// Vectors of PAL_MC_CLEAR_LOG.pending
//
#define PAL_MC_PENDING    BIT0
#define PAL_INIT_PENDING  BIT1

/**
  PAL Procedure - PAL_MC_CLEAR_LOG.

  Clear all error information from processor error logging
  registers. It is required by Itanium processors. The PAL procedure supports
  the Static Registers calling convention. It could be called at
  physical mode and virtual mode.

  @param Index  Index of PAL_MC_CLEAR_LOG within the list of
                PAL procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    64-bit vector denoting whether an event is
                pending. See PAL_MC_CLEAR_LOG.pending above.

**/
#define PAL_MC_CLEAR_LOG 21

/**
  PAL Procedure - PAL_MC_DRAIN.

  Ensure that all operations that could cause an MCA have
  completed. It is required by Itanium processors. The PAL procedure supports
  the Static Registers calling convention. It could be called at
  physical mode and virtual mode.

  @param Index  Index of PAL_MC_DRAIN within the list of PAL
                procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

**/
#define PAL_MC_DRAIN 22


/**
  PAL Procedure - PAL_MC_DYNAMIC_STATE.

  Return Processor Dynamic State for logging by SAL. It is
  optional. The PAL procedure supports the Static Registers
  calling convention. It could be called at physical mode.

  @param Index  Index of PAL_MC_DYNAMIC_STATE within the list of PAL
                procedures.
  @param Offset Offset of the next 8 bytes of Dynamic Processor
                State to return. (multiple of 8).

  @retval 0     Call completed without error
  @retval -1    Unimplemented procedure.
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

  @return R9    Unsigned 64-bit integer denoting bytes of Dynamic
                Processor State returned.
  @return R10   Next 8 bytes of Dynamic Processor State.

**/
#define PAL_MC_DYNAMIC_STATE 24



//
// Values of PAL_MC_ERROR_INFO.InfoIndex.
//
#define PAL_PROCESSOR_ERROR_MAP       0
#define PAL_PROCESSOR_STATE_PARAM     1
#define PAL_STRUCTURE_SPECIFIC_ERROR  2

typedef struct {
 UINT64 CoreId:4;                   ///< Bit3:0,  Processor core ID (default is 0 for
                                    ///< processors with a single core)

 UINT64 ThreadId:4;                 ///< Bit7:4, Logical thread ID (default is 0 for
                                    ///< processors that execute a single thread)

 UINT64 InfoOfInsCache:4;           ///< Bit11:8, Error information is
                                    ///< available for 1st, 2nd, 3rd, and 4th
                                    ///< level instruction caches.

 UINT64 InfoOfDataCache:4;          ///< Bit15:12, Error information is
                                    ///<   available for 1st, 2nd, 3rd, and 4th
                                    ///<   level data/unified caches.

 UINT64 InfoOfInsTlb:4;             ///< Bit19:16 Error information is available
                                    ///< for 1st, 2nd, 3rd, and 4th level
                                    ///< instruction TLB.

 UINT64 InfoOfDataTlb:4;            ///< Bit23:20, Error information is available
                                    ///< for 1st, 2nd, 3rd, and 4th level
                                    ///< data/unified TLB

 UINT64 InfoOfProcessorBus:4;       ///< Bit27:24 Error information is
                                    ///<   available for the 1st, 2nd, 3rd,
                                    ///<   and 4th level processor bus
                                    ///<   hierarchy.
 UINT64 InfoOfRegisterFile:4;       ///< Bit31:28 Error information is
                                    ///<   available on register file
                                    ///<   structures.
 UINT64 InfoOfMicroArch:4;          ///< Bit47:32, Error information is
                                    ///<   available on micro-architectural
                                    ///<   structures.
 UINT64 Reserved:16;
} PAL_MC_ERROR_INFO_LEVEL_INDEX;

//
// Value of PAL_MC_ERROR_INFO.ErrorTypeIndex
//
#define PAL_ERR_INFO_BY_LEVEL_INDEX               0
#define PAL_ERR_INFO_TARGET_ADDRESS               1
#define PAL_ERR_INFO_REQUESTER_IDENTIFIER         2
#define PAL_ERR_INFO_REPONSER_INDENTIFIER         3
#define PAL_ERR_INFO_PRECISE_INSTRUCTION_POINTER  4

typedef struct {
  UINT64  Operation:4;                  ///< Bit3:0, Type of cache operation that caused
                                        ///< the machine check: 0 - unknown or internal
                                        ///< error 1 - load 2 - store 3 - instruction
                                        ///< fetch or instruction prefetch 4 - data
                                        ///< prefetch (both hardware and software) 5 -
                                        ///< snoop (coherency check) 6 - cast out
                                        ///< (explicit or implicit write-back of a cache
                                        ///< line) 7 - move in (cache line fill)

  UINT64  FailedCacheLevel:2;           ///< Bit5:4 Level of cache where the
                                        ///< error occurred. A value of 0
                                        ///< indicates the first level of cache.
  UINT64  Reserved1:2;
  UINT64  FailedInDataPart:1;           ///< Bit8, Failure located in the data part of the cache line.
  UINT64  FailedInTagPart:1;            ///< Bit9, Failure located in the tag part of the cache line.
  UINT64  FailedInDataCache:1;          ///< Bit10, Failure located in the data cache

  UINT64  FailedInInsCache:1;           ///< Bit11, Failure located in the
                                        ///< instruction cache.
                                        
  UINT64  Mesi:3;                       ///< Bit14:12,  0 - cache line is invalid. 1 - cache
                                        ///< line is held shared. 2 - cache line is held
                                        ///< exclusive. 3 - cache line is modified. All other
                                        ///< values are reserved.
                                        
  UINT64  MesiIsValid:1;                ///< Bit15, The mesi field in the cache_check
                                        ///< parameter is valid.
                                        
  UINT64  FailedWay:5;                  ///< Bit20:16, Failure located in the way of
                                        ///< the cache indicated by this value.

  UINT64  WayIndexIsValid:1;            ///< Bit21, The way and index field in the
                                        ///< cache_check parameter is valid.

  UINT64  Reserved2:1;
  UINT64  MultipleBitsError:1;          ///< Bit23, A multiple-bit error was
                                        ///< detected, and data was poisoned for
                                        ///< the corresponding cache line during
                                        ///< castout.
  UINT64  Reserved3:8;
  UINT64  IndexOfCacheLineError:20;     ///< Bit51:32, Index of the cache
                                        ///< line where the error occurred.
  UINT64  Reserved4:2;

  UINT64  InstructionSet:1;             ///< Bit54, Instruction set. If this value
                                        ///<   is set to zero, the instruction that
                                        ///<   generated the machine check was an
                                        ///<   Intel Itanium instruction. If this bit
                                        ///<   is set to one, the instruction that
                                        ///<   generated the machine check was IA-32
                                        ///<   instruction.

  UINT64  InstructionSetIsValid:1;      ///< Bit55, The is field in the
                                        ///< cache_check parameter is valid.

  UINT64  PrivilegeLevel:2;             ///< Bit57:56, Privilege level. The
                                        ///<   privilege level of the instruction
                                        ///<   bundle responsible for generating the
                                        ///<   machine check.

  UINT64  PrivilegeLevelIsValide:1;     ///< Bit58, The pl field of the
                                        ///<   cache_check parameter is
                                        ///<   valid.

  UINT64  McCorrected:1;                ///< Bit59, Machine check corrected: This bit
                                        ///<   is set to one to indicate that the machine
                                        ///<   check has been corrected.

  UINT64  TargetAddressIsValid:1;       ///< Bit60, Target address is valid:
                                        ///< This bit is set to one to
                                        ///< indicate that a valid target
                                        ///< address has been logged.

  UINT64  RequesterIdentifier:1;        ///< Bit61, Requester identifier: This
                                        ///<   bit is set to one to indicate that
                                        ///<   a valid requester identifier has
                                        ///<   been logged.

  UINT64  ResponserIdentifier:1;        ///< Bit62, Responder identifier: This
                                        ///<   bit is set to one to indicate that
                                        ///<   a valid responder identifier has
                                        ///<   been logged.

  UINT64  PreciseInsPointer:1;          ///< Bit63,  Precise instruction pointer.
                                        ///< This bit is set to one to indicate
                                        ///< that a valid precise instruction
                                        ///< pointer has been logged.

} PAL_CACHE_CHECK_INFO;


typedef struct {
  UINT64  FailedSlot:8;                 ///< Bit7:0, Slot number of the translation
                                        ///< register where the failure occurred.
  UINT64  FailedSlotIsValid:1;          ///< Bit8, The tr_slot field in the
                                        ///< TLB_check parameter is valid.
  UINT64  Reserved1 :1;
  UINT64  TlbLevel:2;                   ///< Bit11:10,  The level of the TLB where the
                                        ///< error occurred. A value of 0 indicates the
                                        ///< first level of TLB
  UINT64  Reserved2 :4;

  UINT64  FailedInDataTr:1;             ///< Bit16, Error occurred in the data
                                        ///< translation registers.

  UINT64  FailedInInsTr:1;              ///< Bit17, Error occurred in the instruction
                                        ///< translation registers

  UINT64  FailedInDataTc:1;             ///< Bit18, Error occurred in data
                                        ///< translation cache.

  UINT64  FailedInInsTc:1;              ///< Bit19, Error occurred in the instruction
                                        ///< translation cache.

  UINT64  FailedOperation:4;            ///< Bit23:20, Type of cache operation that
                                        ///<   caused the machine check: 0 - unknown
                                        ///<   1 - TLB access due to load instruction
                                        ///<   2 - TLB access due to store
                                        ///<   instruction 3 - TLB access due to
                                        ///<   instruction fetch or instruction
                                        ///<   prefetch 4 - TLB access due to data
                                        ///<   prefetch (both hardware and software)
                                        ///<   5 - TLB shoot down access 6 - TLB
                                        ///<   probe instruction (probe, tpa) 7 -
                                        ///<   move in (VHPT fill) 8 - purge (insert
                                        ///<   operation that purges entries or a TLB
                                        ///<   purge instruction) All other values
                                        ///<   are reserved.

  UINT64  Reserved3:30;
  UINT64  InstructionSet:1;             ///< Bit54, Instruction set. If this value
                                        ///<   is set to zero, the instruction that
                                        ///<   generated the machine check was an
                                        ///<   Intel Itanium instruction. If this bit
                                        ///<   is set to one, the instruction that
                                        ///<   generated the machine check was IA-32
                                        ///<   instruction.

  UINT64  InstructionSetIsValid:1;      ///< Bit55, The is field in the
                                        ///< TLB_check parameter is valid.

  UINT64  PrivelegeLevel:2;             ///< Bit57:56, Privilege level. The
                                        ///<   privilege level of the instruction
                                        ///<   bundle responsible for generating the
                                        ///<   machine check.

  UINT64  PrivelegeLevelIsValid:1;      ///< Bit58,  The pl field of the
                                        ///< TLB_check parameter is valid.

  UINT64  McCorrected:1;                ///< Bit59, Machine check corrected: This bit
                                        ///<   is set to one to indicate that the machine
                                        ///<   check has been corrected.

  UINT64  TargetAddressIsValid:1;       ///< Bit60, Target address is valid:
                                        ///< This bit is set to one to
                                        ///< indicate that a valid target
                                        ///< address has been logged.

  UINT64  RequesterIdentifier:1;        ///< Bit61 Requester identifier: This
                                        ///<   bit is set to one to indicate that
                                        ///<   a valid requester identifier has
                                        ///<   been logged.

  UINT64  ResponserIdentifier:1;        ///< Bit62, Responder identifier:  This
                                        ///<   bit is set to one to indicate that
                                        ///<   a valid responder identifier has
                                        ///<   been logged.

  UINT64  PreciseInsPointer:1;          ///< Bit63 Precise instruction pointer.
                                        ///<   This bit is set to one to indicate
                                        ///<   that a valid precise instruction
                                        ///<   pointer has been logged.
} PAL_TLB_CHECK_INFO;

/**
  PAL Procedure - PAL_MC_ERROR_INFO.

  Return Processor Machine Check Information and Processor
  Static State for logging by SAL. It is required by Itanium processors. The
  PAL procedure supports the Static Registers calling
  convention. It could be called at physical and virtual mode.

  @param Index            Index of PAL_MC_ERROR_INFO within the list of PAL
                          procedures.
  @param InfoIndex        Unsigned 64-bit integer identifying the
                          error information that is being requested.
                          See PAL_MC_ERROR_INFO.InfoIndex.
  @param LevelIndex       8-byte formatted value identifying the
                          structure to return error information
                          on. See PAL_MC_ERROR_INFO_LEVEL_INDEX.
  @param ErrorTypeIndex   Unsigned 64-bit integer denoting the
                          type of error information that is
                          being requested for the structure
                          identified in LevelIndex.

  @retval 0               Call completed without error
  @retval -2              Invalid argument
  @retval -3              Call completed with error.
  @retval -6              Argument was valid, but no error
                          information was available

  @return R9              Error information returned. The format of this
                          value is dependant on the input values passed.
  @return R10             If this value is zero, all the error information
                          specified by err_type_index has been returned. If
                          this value is one, more structure-specific error
                          information is available and the caller needs to
                          make this procedure call again with level_index
                          unchanged and err_type_index, incremented.

**/
#define PAL_MC_ERROR_INFO 25

/**
  PAL Procedure - PAL_MC_EXPECTED.

  Set/Reset Expected Machine Check Indicator. It is required by
  Itanium processors. The PAL procedure supports the Static Registers calling
  convention. It could be called at physical mode.

  @param Index      Index of PAL_MC_EXPECTED within the list of PAL
                    procedures.
  @param Expected   Unsigned integer with a value of 0 or 1 to
                    set or reset the hardware resource
                    PALE_CHECK examines for expected machine
                    checks.

  @retval 0         Call completed without error
  @retval -2        Invalid argument
  @retval -3        Call completed with error.

  @return R9        Unsigned integer denoting whether a machine check
                    was previously expected.

**/
#define PAL_MC_EXPECTED 23

/**
  PAL Procedure - PAL_MC_REGISTER_MEM.

  Register min-state save area with PAL for machine checks and
  inits. It is required by Itanium processors. The PAL procedure supports the
  Static Registers calling convention. It could be called at
  physical mode.

  @param Index    Index of PAL_MC_REGISTER_MEM within the list of PAL
                  procedures.
  @param Address  Physical address of the buffer to be
                  registered with PAL.

  @retval 0       Call completed without error
  @retval -2      Invalid argument
  @retval -3      Call completed with error.

**/
#define PAL_MC_REGISTER_MEM 27

/**
  PAL Procedure - PAL_MC_RESUME.

  Restore minimal architected state and return to interrupted
  process. It is required by Itanium processors. The PAL procedure supports the
  Static Registers calling convention. It could be called at
  physical mode.

  @param Index        Index of PAL_MC_RESUME within the list of PAL
                      procedures.
  @param SetCmci      Unsigned 64 bit integer denoting whether to
                      set the CMC interrupt. A value of 0 indicates
                      not to set the interrupt, a value of 1
                      indicated to set the interrupt, and all other
                      values are reserved.
  @param SavePtr      Physical address of min-state save area used
                      to used to restore processor state.
  @param NewContext   Unsigned 64-bit integer denoting whether
                      the caller is returning to a new context.
                      A value of 0 indicates the caller is
                      returning to the interrupted context, a
                      value of 1 indicates that the caller is
                      returning to a new context.

  @retval -2          Invalid argument
  @retval -3          Call completed with error.

**/
#define PAL_MC_RESUME 26

/**
  PAL Procedure - PAL_HALT.

  Enter the low-power HALT state or an implementation-dependent
  low-power state. It is optinal. The PAL procedure supports the
  Static Registers calling convention. It could be called at
  physical mode.

  @param Index        Index of PAL_HALT within the list of PAL
                      procedures.
  @param HaltState    Unsigned 64-bit integer denoting low power
                      state requested.
  @param IoDetailPtr  8-byte aligned physical address pointer to
                      information on the type of I/O
                      (load/store) requested.

  @retval 0           Call completed without error
  @retval -1          Unimplemented procedure
  @retval -2          Invalid argument
  @retval -3          Call completed with error.

  @return R9          Value returned if a load instruction is requested
                      in the io_detail_ptr

**/
#define PAL_HALT 28


/**
  PAL Procedure - PAL_HALT_INFO.

  Return the low power capabilities of the processor. It is
  required by Itanium processors. The PAL procedure supports the
  Stacked Registers calling convention. It could be called at
  physical and virtual mode.

  @param Index        Index of PAL_HALT_INFO within the list of PAL
                      procedures.
  @param PowerBuffer  64-bit pointer to a 64-byte buffer aligned
                      on an 8-byte boundary.

  @retval 0           Call completed without error
  @retval -2          Invalid argument
  @retval -3          Call completed with error.

**/
#define PAL_HALT_INFO 257


/**
  PAL Procedure - PAL_HALT_LIGHT.

  Enter the low power LIGHT HALT state. It is required by
  Itanium processors. The PAL procedure supports the Static Registers calling
  convention. It could be called at physical and virtual mode.

  @param Index  Index of PAL_HALT_LIGHT within the list of PAL
                procedures.

  @retval 0     Call completed without error
  @retval -2    Invalid argument
  @retval -3    Call completed with error.

**/
#define PAL_HALT_LIGHT 29

/**
  PAL Procedure - PAL_CACHE_LINE_INIT.

  Initialize tags and data of a cache line for processor
  testing. It is required by Itanium processors. The PAL procedure supports the
  Static Registers calling convention. It could be called at
  physical and virtual mode.

  @param Index      Index of PAL_CACHE_LINE_INIT within the list of PAL
                    procedures.
  @param Address    Unsigned 64-bit integer value denoting the
                    physical address from which the physical page
                    number is to be generated. The address must be
                    an implemented physical address, bit 63 must
                    be zero.
  @param DataValue  64-bit data value which is used to
                    initialize the cache line.

  @retval 0         Call completed without error
  @retval -2        Invalid argument
  @retval -3        Call completed with error.

**/
#define PAL_CACHE_LINE_INIT 31

/**
  PAL Procedure - PAL_CACHE_READ.

  Read tag and data of a cache line for diagnostic testing. It
  is optional. The PAL procedure supports the
  Satcked Registers calling convention. It could be called at
  physical mode.

  @param Index    Index of PAL_CACHE_READ within the list of PAL
                  procedures.
  @param LineId   8-byte formatted value describing where in the
                  cache to read the data.
  @param Address  64-bit 8-byte aligned physical address from
                  which to read the data. The address must be an
                  implemented physical address on the processor
                  model with bit 63 set to zero.

  @retval 1       The word at address was found in the
                  cache, but the line was invalid.
  @retval 0       Call completed without error
  @retval -2      Invalid argument
  @retval -3      Call completed with error.
  @retval -5      The word at address was not found in the
                  cache.
  @retval -7      The operation requested is not supported
                  for this cache_type and level.

  @return R9      Right-justified value returned from the cache
                  line.
  @return R10     The number of bits returned in data.
  @return R11     The status of the cache line.

**/
#define PAL_CACHE_READ 259


/**
  PAL Procedure - PAL_CACHE_WRITE.

  Write tag and data of a cache for diagnostic testing. It is
  optional. The PAL procedure supports the Satcked Registers
  calling convention. It could be called at physical mode.

  @param Index    Index of PAL_CACHE_WRITE within the list of PAL
                  procedures.
  @param LineId   8-byte formatted value describing where in the
                  cache to write the data.
  @param Address  64-bit 8-byte aligned physical address at
                  which the data should be written. The address
                  must be an implemented physical address on the
                  processor model with bit 63 set to 0.
  @param Data     Unsigned 64-bit integer value to write into
                  the specified part of the cache.

  @retval 0       Call completed without error
  @retval -2      Invalid argument
  @retval -3      Call completed with error.
  @retval -7      The operation requested is not supported
                  for this cache_type and level.

**/
#define PAL_CACHE_WRITE 260

/**
  PAL Procedure - PAL_TEST_INFO.

  Returns alignment and size requirements needed for the memory
  buffer passed to the PAL_TEST_PROC procedure as well as
  information on self-test control words for the processor self
  tests. It is required by Itanium processors. The PAL procedure supports the
  Static Registers calling convention. It could be called at
  physical mode.

  @param Index      Index of PAL_TEST_INFO within the list of PAL
                    procedures.
  @param TestPhase  Unsigned integer that specifies which phase
                    of the processor self-test information is
                    being requested on. A value of 0 indicates
                    the phase two of the processor self-test and
                    a value of 1 indicates phase one of the
                    processor self-test. All other values are
                    reserved.

  @retval 0         Call completed without error
  @retval -2        Invalid argument
  @retval -3        Call completed with error.

  @return R9        Unsigned 64-bit integer denoting the number of
                    bytes of main memory needed to perform the second
                    phase of processor self-test.
  @return R10       Unsigned 64-bit integer denoting the alignment
                    required for the memory buffer.
  @return R11       48-bit wide bit-field indicating if control of
                    the processor self-tests is supported and which
                    bits of the test_control field are defined for
                    use.

**/
#define PAL_TEST_INFO 37

typedef struct {
  UINT64  BufferSize:56;    ///< Indicates the size in bytes of the memory
                            ///< buffer that is passed to this procedure.
                            ///< BufferSize must be greater than or equal in
                            ///< size to the bytes_needed return value from
                            ///< PAL_TEST_INFO, otherwise this procedure will
                            ///< return with an invalid argument return
                            ///< value.

  UINT64  TestPhase:8;      ///< Defines which phase of the processor
                            ///< self-tests are requested to be run. A value
                            ///< of zero indicates to run phase two of the
                            ///< processor self-tests. Phase two of the
                            ///< processor self-tests are ones that require
                            ///< external memory to execute correctly. A
                            ///< value of one indicates to run phase one of
                            ///< the processor self-tests. Phase one of the
                            ///< processor self-tests are tests run during
                            ///< PALE_RESET and do not depend on external
                            ///< memory to run correctly. When the caller
                            ///< requests to have phase one of the processor
                            ///< self-test run via this procedure call, a
                            ///< memory buffer may be needed to save and
                            ///< restore state as required by the PAL calling
                            ///< conventions. The procedure PAL_TEST_INFO
                            ///< informs the caller about the requirements of
                            ///< the memory buffer.
} PAL_TEST_INFO_INFO;

typedef struct {
  UINT64  TestControl:47;       ///< This is an ordered implementation-specific
                                ///<   control word that allows the user control
                                ///<   over the length and runtime of the
                                ///<   processor self-tests. This control word is
                                ///<   ordered from the longest running tests up
                                ///<   to the shortest running tests with bit 0
                                ///<   controlling the longest running test. PAL
                                ///<   may not implement all 47-bits of the
                                ///<   test_control word. PAL communicates if a
                                ///<   bit provides control by placing a zero in
                                ///<   that bit. If a bit provides no control,
                                ///<   PAL will place a one in it. PAL will have
                                ///<   two sets of test_control bits for the two
                                ///<   phases of the processor self-test. PAL
                                ///<   provides information about implemented
                                ///<   test_control bits at the hand-off from PAL
                                ///<   to SAL for the firmware recovery check.
                                ///<   These test_control bits provide control
                                ///<   for phase one of processor self-test. It
                                ///<   also provides this information via the PAL
                                ///<   procedure call PAL_TEST_INFO for both the
                                ///<   phase one and phase two processor tests
                                ///<   depending on which information the caller
                                ///<   is requesting. PAL interprets these bits
                                ///<   as input parameters on two occasions. The
                                ///<   first time is when SAL passes control back
                                ///<   to PAL after the firmware recovery check.
                                ///<   The second time is when a call to
                                ///<   PAL_TEST_PROC is made. When PAL interprets
                                ///<   these bits it will only interpret
                                ///<   implemented test_control bits and will
                                ///<   ignore the values located in the
                                ///<   unimplemented test_control bits. PAL
                                ///<   interprets the implemented bits such that
                                ///<   if a bit contains a zero, this indicates
                                ///<   to run the test. If a bit contains a one,
                                ///<   this indicates to PAL to skip the test. If
                                ///<   the cs bit indicates that control is not
                                ///<    available, the test_control bits will be
                                ///<   ignored or generate an illegal argument in
                                ///<   procedure calls if the caller sets these
                                ///<   bits.
                                
  UINT64  ControlSupport:1;     ///< This bit defines if an implementation
                                ///<  supports control of the PAL self-tests
                                ///<  via the self-test control word. If
                                ///<  this bit is 0, the implementation does
                                ///<  not support control of the processor
                                ///<  self-tests via the self-test control
                                ///<  word. If this bit is 1, the
                                ///<  implementation does support control of
                                ///<  the processor self-tests via the
                                ///<  self-test control word. If control is
                                ///<  not supported, GR37 will be ignored at
                                ///<  the hand-off between SAL and PAL after
                                ///<  the firmware recovery check and the
                                ///<  PAL procedures related to the
                                ///<  processor self-tests may return
                                ///<  illegal arguments if a user tries to
                                ///<  use the self-test control features.
  UINT64  Reserved:16;
} PAL_SELF_TEST_CONTROL;

typedef struct {
  UINT64  Attributes:8;         ///< Specifies the memory attributes that are
                                ///<  allowed to be used with the memory buffer
                                ///<  passed to this procedure. The attributes
                                ///<  parameter is a vector where each bit
                                ///<  represents one of the virtual memory
                                ///<  attributes defined by the architecture.See
                                ///<  MEMORY_AATRIBUTES. The caller is required
                                ///<  to support the cacheable attribute for the
                                ///<  memory buffer, otherwise an invalid
                                ///<  argument will be returned.
  UINT64  Reserved:8;
  UINT64  TestControl:48;       ///< Is the self-test control word
                                ///<  corresponding to the test_phase passed.
                                ///<  This test_control directs the coverage and
                                ///<  runtime of the processor self-tests
                                ///<  specified by the test_phase input
                                ///<  argument. Information on if this
                                ///<  feature is implemented and the number of
                                ///<  bits supported can be obtained by the
                                ///<  PAL_TEST_INFO procedure call. If this
                                ///<  feature is implemented by the processor,
                                ///<  the caller can selectively skip parts of
                                ///<  the processor self-test by setting
                                ///<  test_control bits to a one. If a bit has a
                                ///<  zero, this test will be run. The values in
                                ///<  the unimplemented bits are ignored. If
                                ///<  PAL_TEST_INFO indicated that the self-test
                                ///<  control word is not implemented, this
                                ///<  procedure will return with an invalid
                                ///<  argument status if the caller sets any of
                                ///<  the test_control bits. See
                                ///<  PAL_SELF_TEST_CONTROL.
} PAL_TEST_CONTROL;

/**
  PAL Procedure - PAL_TEST_PROC.

  Perform late processor self test. It is required by Itanium processors. The
  PAL procedure supports the Static Registers calling
  convention. It could be called at physical mode.

  @param Index        Index of PAL_TEST_PROC within the list of PAL
                      procedures.
  @param TestAddress  64-bit physical address of main memory
                      area to be used by processor self-test.
                      The memory region passed must be
                      cacheable, bit 63 must be zero.
  @param TestInfo     Input argument specifying the size of the
                      memory buffer passed and the phase of the
                      processor self-test that should be run. See
                      PAL_TEST_INFO.
  @param TestParam    Input argument specifying the self-test
                      control word and the allowable memory
                      attributes that can be used with the memory
                      buffer. See PAL_TEST_CONTROL.

  @retval 1           Call completed without error, but hardware
                      failures occurred during self-test.
  @retval 0           Call completed without error
  @retval -2          Invalid argument
  @retval -3          Call completed with error.

  @return R9          Formatted 8-byte value denoting the state of the
                      processor after self-test

**/
#define PAL_TEST_PROC 258

typedef struct {
  UINT32  NumberOfInterruptControllers;     ///< Number of interrupt
                                            ///< controllers currently
                                            ///< enabled on the system.

  UINT32  NumberOfProcessors;               ///< Number of processors currently
                                            ///< enabled on the system.
} PAL_PLATFORM_INFO;

/**
  PAL Procedure - PAL_COPY_INFO.

  Return information needed to relocate PAL procedures and PAL
  PMI code to memory. It is required by Itanium processors. The PAL procedure
  supports the Static Registers calling convention. It could be
  called at physical mode.

  @param Index              Index of PAL_COPY_INFO within the list of PAL
                            procedures.
  @param CopyType           Unsigned integer denoting type of procedures
                            for which copy information is requested.
  @param PlatformInfo       8-byte formatted value describing the
                            number of processors and the number of
                            interrupt controllers currently enabled
                            on the system. See PAL_PLATFORM_INFO.
  @param McaProcStateInfo   Unsigned integer denoting the number
                            of bytes that SAL needs for the
                            min-state save area for each
                            processor.

  @retval 0                 Call completed without error
  @retval -2                Invalid argument
  @retval -3                Call completed with error.

  @return R9                Unsigned integer denoting the number of bytes of
                            PAL information that must be copied to main
                            memory.
  @return R10               Unsigned integer denoting the starting alignment
                            of the data to be copied.

**/
#define PAL_COPY_INFO 30

/**
  PAL Procedure - PAL_COPY_PAL.

  Relocate PAL procedures and PAL PMI code to memory. It is
  required by Itanium processors. The PAL procedure supports the Stacked
  Registers calling convention. It could be called at physical
  mode.

  @param Index          Index of PAL_COPY_PAL within the list of PAL
                        procedures.
  @param TargetAddress  Physical address of a memory buffer to
                        copy relocatable PAL procedures and PAL
                        PMI code.
  @param AllocSize      Unsigned integer denoting the size of the
                        buffer passed by SAL for the copy operation.
  @param CopyOption     Unsigned integer indicating whether
                        relocatable PAL code and PAL PMI code
                        should be copied from firmware address
                        space to main memory.

  @retval 0             Call completed without error
  @retval -2            Invalid argument
  @retval -3            Call completed with error.

  @return R9            Unsigned integer denoting the offset of PAL_PROC
                        in the relocatable segment copied.

**/
#define PAL_COPY_PAL 256

/**
  PAL Procedure - PAL_ENTER_IA_32_ENV.

  Enter IA-32 System environment. It is optional. The PAL
  procedure supports the Static Registers calling convention.
  It could be called at physical mode.

  Note: Since this is a special call, it does not follow the PAL
  static register calling convention. GR28 contains the index of
  PAL_ENTER_IA_32_ENV within the list of PAL procedures. All other
  input arguments including GR29-GR31 are setup by SAL to values
  as required by the IA-32 operating system defined in Table
  11-67. The registers that are designated as preserved, scratch,
  input arguments and procedure return values by the static
  procedure calling convention are not followed by this call. For
  instance, GR5 and GR6 need not be preserved since these are
  regarded as scratch by the IA-32 operating system. Note: In an
  MP system, this call must be COMPLETED on the first CPU to enter
  the IA-32 System Environment (may or may not be the BSP) prior
  to being called on the remaining processors in the MP system.

  @param Index  GR28 contains the index of the
                PAL_ENTER_IA_32_ENV call within the list of PAL
                procedures.


  @retval  The status is returned in GR4.
                  -1 - Un-implemented procedure 0 JMPE detected
                  at privilege level

                  0 - 1 SAL allocated buffer for IA-32 System
                  Environment operation is too small

                  2 - IA-32 Firmware Checksum Error

                  3 -  SAL allocated buffer for IA-32 System
                  Environment operation is not properly aligned

                  4 - Error in SAL MP Info Table

                  5 - Error in SAL Memory Descriptor Table

                  6 - Error in SAL System Table

                  7 - Inconsistent IA-32 state

                  8 - IA-32 Firmware Internal Error

                  9 - IA-32 Soft Reset (Note: remaining register
                  state is undefined for this termination
                  reason)

                  10 - Machine Check Error

                  11 - Error in SAL I/O Intercept Table

                  12 - Processor exit due to other processor in
                  MP system terminating the IA32 system
                  environment. (Note: remaining register state
                  is undefined for this termination reason.)

                  13 - Itanium architecture-based state
                  corruption by either SAL PMI handler or I/O
                  Intercept callback function.


**/
#define PAL_ENTER_IA_32_ENV 33

/**
  PAL Procedure - PAL_PMI_ENTRYPOINT.

  Register PMI memory entrypoints with processor. It is required
  by Itanium processors. The PAL procedure supports the Stacked Registers
  calling convention. It could be called at physical mode.

  @param Index        Index of PAL_PMI_ENTRYPOINT within the list of
                      PAL procedures.
  @param SalPmiEntry  256-byte aligned physical address of SAL
                      PMI entrypoint in memory.

  @retval 0           Call completed without error
  @retval -2          Invalid argument
  @retval -3          Call completed with error.

**/
#define PAL_PMI_ENTRYPOINT 32


/**

  The ASCII brand identification string will be copied to the
  address specified in the address input argument. The processor
  brand identification string is defined to be a maximum of 128
  characters long; 127 bytes will contain characters and the 128th
  byte is defined to be NULL (0). A processor may return less than
  the 127 ASCII characters as long as the string is null
  terminated. The string length will be placed in the brand_info
  return argument.

**/
#define PAL_BRAND_INFO_ID_REQUEST  0

/**
  PAL Procedure - PAL_BRAND_INFO.

  Provides processor branding information. It is optional by
  Itanium processors. The PAL procedure supports the Stacked Registers calling
  convention. It could be called at physical and Virtual mode.

  @param Index        Index of PAL_BRAND_INFO within the list of PAL
                      procedures.
  @param InfoRequest  Unsigned 64-bit integer specifying the
                      information that is being requested. (See
                      PAL_BRAND_INFO_ID_REQUEST)
  @param Address      Unsigned 64-bit integer specifying the
                      address of the 128-byte block to which the
                      processor brand string shall be written.

  @retval 0           Call completed without error
  @retval -1          Unimplemented procedure
  @retval -2          Invalid argument
  @retval -3          Call completed with error.
  @retval -6          Input argument is not implemented.

  @return R9          Brand information returned. The format of this
                      value is dependent on the input values passed.

**/
#define PAL_BRAND_INFO  274

/**
  PAL Procedure - PAL_GET_HW_POLICY.

  Returns the current hardware resource sharing policy of the
  processor. It is optional by Itanium processors. The PAL procedure supports
  the Static Registers calling convention. It could be called at
  physical and Virtual mode.


  @param Index            Index of PAL_GET_HW_POLICY within the list of PAL
                          procedures.
  @param ProcessorNumber  Unsigned 64-bit integer that specifies
                          for which logical processor
                          information is being requested. This
                          input argument must be zero for the
                          first call to this procedure and can
                          be a maximum value of one less than
                          the number of logical processors
                          impacted by the hardware resource
                          sharing policy, which is returned by
                          the R10 return value.

  @retval 0               Call completed without error
  @retval -1              Unimplemented procedure
  @retval -2              Invalid argument
  @retval -3              Call completed with error.
  @retval -9              Call requires PAL memory buffer.

  @return R9              Unsigned 64-bit integer representing the current
                          hardware resource sharing policy.
  @return R10             Unsigned 64-bit integer that returns the number
                          of logical processors impacted by the policy
                          input argument.
  @return R11             Unsigned 64-bit integer containing the logical
                          address of one of the logical processors
                          impacted by policy modification.

**/
#define PAL_GET_HW_POLICY   48


//
// Value of PAL_SET_HW_POLICY.Policy
//
#define PAL_SET_HW_POLICY_PERFORMANCE               0
#define PAL_SET_HW_POLICY_FAIRNESS                  1
#define PAL_SET_HW_POLICY_HIGH_PRIORITY             2
#define PAL_SET_HW_POLICY_EXCLUSIVE_HIGH_PRIORITY   3

/**
  PAL Procedure - PAL_SET_HW_POLICY.

  Sets the current hardware resource sharing policy of the
  processor. It is optional by Itanium processors. The PAL procedure supports
  the Static Registers calling convention. It could be called at
  physical and Virtual mode.

  @param Index    Index of PAL_SET_HW_POLICY within the list of PAL
                  procedures.
  @param Policy   Unsigned 64-bit integer specifying the hardware
                  resource sharing policy the caller is setting.
                  See Value of PAL_SET_HW_POLICY.Policy above.

  @retval 1       Call completed successfully but could not
                  change the hardware policy since a
                  competing logical processor is set in
                  exclusive high priority.
  @retval 0       Call completed without error
  @retval -1      Unimplemented procedure
  @retval -2      Invalid argument
  @retval -3      Call completed with error.
  @retval -9      Call requires PAL memory buffer.

**/
#define PAL_SET_HW_POLICY   49

typedef struct {
  UINT64  Mode:3;                   ///< Bit2:0, Indicates the mode of operation for this
                                    ///<    procedure: 0 - Query mode 1 - Error inject mode
                                    ///<    (err_inj should also be specified) 2 - Cancel
                                    ///<    outstanding trigger. All other fields in
                                    ///<    PAL_MC_ERROR_TYPE_INFO,
                                    ///<    PAL_MC_ERROR_STRUCTURE_INFO and
                                    ///<    PAL_MC_ERROR_DATA_BUFFER are ignored. All other
                                    ///<    values are reserved.

  UINT64  ErrorInjection:3;         ///< Bit5:3, indicates the mode of error
                                    ///<  injection: 0 - Error inject only (no
                                    ///<  error consumption) 1 - Error inject
                                    ///<  and consume All other values are
                                    ///<  reserved.

  UINT64  ErrorSeverity:2;          ///< Bit7:6, indicates the severity desired
                                    ///<  for error injection/query. Definitions
                                    ///<  of the different error severity types
                                    ///<  0 - Corrected error 1 - Recoverable
                                    ///<  error 2 - Fatal error 3 - Reserved

  UINT64  ErrorStructure:5;         ///< Bit12:8, Indicates the structure
                                    ///<  identification for error
                                    ///<  injection/query: 0 - Any structure
                                    ///<  (cannot be used during query mode).
                                    ///<  When selected, the structure type used
                                    ///<  for error injection is determined by
                                    ///<  PAL. 1 - Cache 2 - TLB 3 - Register
                                    ///<  file 4 - Bus/System interconnect 5-15
                                    ///<  - Reserved 16-31 - Processor
                                    ///<  specific error injection
                                    ///<  capabilities.ErrorDataBuffer is used
                                    ///<  to specify error types. Please refer
                                    ///<  to the processor specific
                                    ///<  documentation for additional details.

  UINT64  StructureHierarchy:3;     ///< Bit15:13, Indicates the structure
                                    ///<  hierarchy for error
                                    ///<  injection/query: 0 - Any level of
                                    ///<  hierarchy (cannot be used during
                                    ///<  query mode). When selected, the
                                    ///<  structure hierarchy used for error
                                    ///<  injection is determined by PAL. 1
                                    ///<  - Error structure hierarchy
                                    ///<  level-1 2 - Error structure
                                    ///<  hierarchy level-2 3 - Error
                                    ///<  structure hierarchy level-3 4 -
                                    ///<  Error structure hierarchy level-4
                                    ///<  All other values are reserved.

  UINT64  Reserved:32;              ///< Reserved 47:16 Reserved

  UINT64  ImplSpec:16;              ///< Bit63:48, Processor specific error injection capabilities.
} PAL_MC_ERROR_TYPE_INFO;

typedef struct {
  UINT64  StructInfoIsValid:1;              ///< Bit0 When 1, indicates that the
                                            ///< structure information fields
                                            ///< (c_t,cl_p,cl_id) are valid and
                                            ///< should be used for error injection.
                                            ///< When 0, the structure information
                                            ///< fields are ignored, and the values
                                            ///< of these fields used for error
                                            ///< injection are
                                            ///< implementation-specific.

  UINT64  CacheType:2;                      ///< Bit2:1  Indicates which cache should be used
                                            ///< for error injection: 0 - Reserved 1 -
                                            ///< Instruction cache 2 - Data or unified cache
                                            ///< 3 - Reserved

  UINT64  PortionOfCacheLine:3;             ///< Bit5:3 Indicates the portion of the
                                            ///<   cache line where the error should
                                            ///<   be injected: 0 - Reserved 1 - Tag
                                            ///<   2 - Data 3 - mesi All other
                                            ///<   values are reserved.

  UINT64  Mechanism:3;                      ///< Bit8:6 Indicates which mechanism is used to
                                            ///< identify the cache line to be used for error
                                            ///< injection: 0 - Reserved 1 - Virtual address
                                            ///< provided in the inj_addr field of the buffer
                                            ///< pointed to by err_data_buffer should be used
                                            ///< to identify the cache line for error
                                            ///< injection. 2 - Physical address provided in
                                            ///< the inj_addr field of the buffer pointed to
                                            ///< by err_data_buffershould be used to identify
                                            ///< the cache line for error injection. 3 - way
                                            ///< and index fields provided in err_data_buffer
                                            ///< should be used to identify the cache line
                                            ///< for error injection. All other values are
                                            ///< reserved.

  UINT64  DataPoisonOfCacheLine:1;          ///< Bit9 When 1, indicates that a
                                            ///< multiple bit, non-correctable
                                            ///< error should be injected in the
                                            ///< cache line specified by cl_id.
                                            ///< If this injected error is not
                                            ///< consumed, it may eventually
                                            ///< cause a data-poisoning event
                                            ///< resulting in a corrected error
                                            ///< signal, when the associated
                                            ///< cache line is cast out (implicit
                                            ///< or explicit write-back of the
                                            ///< cache line). The error severity
                                            ///< specified by err_sev in
                                            ///< err_type_info must be set to 0
                                            ///< (corrected error) when this bit
                                            ///< is set.

  UINT64  Reserved1:22;

  UINT64  TrigerInfoIsValid:1;              ///< Bit32 When 1, indicates that the
                                            ///< trigger information fields (trigger,
                                            ///< trigger_pl) are valid and should be
                                            ///< used for error injection. When 0,
                                            ///< the trigger information fields are
                                            ///< ignored and error injection is
                                            ///< performed immediately.

  UINT64  Triger:4;                         ///< Bit36:33 Indicates the operation type to be
                                            ///<   used as the error trigger condition. The
                                            ///<   address corresponding to the trigger is
                                            ///<   specified in the trigger_addr field of the
                                            ///<   buffer pointed to by err_data_buffer: 0 -
                                            ///<   Instruction memory access. The trigger match
                                            ///<   conditions for this operation type are similar
                                            ///<   to the IBR address breakpoint match conditions
                                            ///<   1 - Data memory access. The trigger match
                                            ///<   conditions for this operation type are similar
                                            ///<   to the DBR address breakpoint match conditions
                                            ///<   All other values are reserved.

  UINT64  PrivilegeOfTriger:3;              ///< Bit39:37  Indicates the privilege
                                            ///< level of the context during which
                                            ///< the error should be injected: 0 -
                                            ///< privilege level 0 1 - privilege
                                            ///< level 1 2 - privilege level 2 3 -
                                            ///< privilege level 3 All other values
                                            ///< are reserved. If the implementation
                                            ///< does not support privilege level
                                            ///< qualifier for triggers (i.e. if
                                            ///< trigger_pl is 0 in the capabilities
                                            ///< vector), this field is ignored and
                                            ///< triggers can be taken at any
                                            ///< privilege level.

  UINT64  Reserved2:24;
} PAL_MC_ERROR_STRUCT_INFO;

/**

   Buffer Pointed to by err_data_buffer - TLB

**/
typedef struct {
  UINT64  TrigerAddress;
  UINT64  VirtualPageNumber:52;
  UINT64  Reserved1:8;
  UINT64  RegionId:24;
  UINT64  Reserved2:40;
} PAL_MC_ERROR_DATA_BUFFER_TLB;

/**
  PAL Procedure - PAL_MC_ERROR_INJECT.

  Injects the requested processor error or returns information
  on the supported injection capabilities for this particular
  processor implementation. It is optional by Itanium processors. The PAL
  procedure supports the Stacked Registers calling convention.
  It could be called at physical and Virtual mode.

  @param Index            Index of PAL_MC_ERROR_INJECT within the list of PAL
                          procedures.
  @param ErrorTypeInfo    Unsigned 64-bit integer specifying the
                          first level error information which
                          identifies the error structure and
                          corresponding structure hierarchy, and
                          the error severity.
  @param ErrorStructInfo  Unsigned 64-bit integer identifying
                          the optional structure specific
                          information that provides the  second
                          level details for the requested error.
  @param ErrorDataBuffer  64-bit physical address of a buffer
                          providing additional parameters for
                          the requested error. The address of
                          this buffer must be 8-byte aligned.

  @retval 0               Call completed without error
  @retval -1              Unimplemented procedure
  @retval -2              Invalid argument
  @retval -3              Call completed with error.
  @retval -4              Call completed with error; the requested
                          error could not be injected due to failure in
                          locating the target location in the specified
                          structure.
  @retval -5              Argument was valid, but requested error
                          injection capability is not supported.
  @retval -9              Call requires PAL memory buffer.

  @return R9              64-bit vector specifying the supported error
                          injection capabilities for the input argument
                          combination of struct_hier, err_struct and
                          err_sev fields in ErrorTypeInfo.
  @return R10             64-bit vector specifying the architectural
                          resources that are used by the procedure.

**/
#define PAL_MC_ERROR_INJECT 276


//
// Types of PAL_GET_PSTATE.Type
//
#define PAL_GET_PSTATE_RECENT                 0
#define PAL_GET_PSTATE_AVERAGE_NEW_START      1
#define PAL_GET_PSTATE_AVERAGE                2
#define PAL_GET_PSTATE_NOW                    3

/**
  PAL Procedure - PAL_GET_PSTATE.

  Returns the performance index of the processor. It is optional
  by Itanium processors. The PAL procedure supports the Stacked Registers
  calling convention. It could be called at physical and Virtual
  mode.

  @param Index  Index of PAL_GET_PSTATE within the list of PAL
                procedures.
  @param Type   Type of performance_index value to be returned
                by this procedure.See PAL_GET_PSTATE.Type above.

  @retval 1     Call completed without error, but accuracy
                of performance index has been impacted by a
                thermal throttling event, or a
                hardware-initiated event.
  @retval 0     Call completed without error
  @retval -1    Unimplemented procedure
  @retval -2    Invalid argument
  @retval -3    Call completed with error.
  @retval -9    Call requires PAL memory buffer.

  @return R9    Unsigned integer denoting the processor
                performance for the time duration since the last
                PAL_GET_PSTATE procedure call was made. The
                value returned is between 0 and 100, and is
                relative to the performance index of the highest
                available P-state.

**/
#define PAL_GET_PSTATE      262

/**

  Layout of PAL_PSTATE_INFO.PStateBuffer

**/
typedef struct {
  UINT32  PerformanceIndex:7;
  UINT32  Reserved1:5;
  UINT32  TypicalPowerDissipation:20;
  UINT32  TransitionLatency1;
  UINT32  TransitionLatency2;
  UINT32  Reserved2;
} PAL_PSTATE_INFO_BUFFER;


/**
  PAL Procedure - PAL_PSTATE_INFO.

  Returns information about the P-states supported by the
  processor. It is optional by Itanium processors. The PAL procedure supports
  the Static Registers calling convention. It could be called
  at physical and Virtual mode.

  @param Index          Index of PAL_PSTATE_INFO within the list of PAL
                        procedures.
  @param PStateBuffer   64-bit pointer to a 256-byte buffer
                        aligned on an 8-byte boundary. See
                        PAL_PSTATE_INFO_BUFFER above.

  @retval 0             Call completed without error
  @retval -1            Unimplemented procedure
  @retval -2            Invalid argument
  @retval -3            Call completed with error.

  @return R9            Unsigned integer denoting the number of P-states
                        supported. The maximum value of this field is 16.
  @return R10           Dependency domain information

**/
#define PAL_PSTATE_INFO     44


/**
  PAL Procedure - PAL_SET_PSTATE.

  To request a processor transition to a given P-state. It is
  optional by Itanium processors. The PAL procedure supports the Stacked
  Registers calling convention. It could be called at physical
  and Virtual mode.

  @param Index        Index of PAL_SET_PSTATE within the list of PAL
                      procedures.
  @param PState       Unsigned integer denoting the processor
                      P-state being requested.
  @param ForcePState  Unsigned integer denoting whether the
                      P-state change should be forced for the
                      logical processor.

  @retval 1           Call completed without error, but
                      transition request was not accepted
  @retval 0           Call completed without error
  @retval -1          Unimplemented procedure
  @retval -2          Invalid argument
  @retval -3          Call completed with error.
  @retval -9          Call requires PAL memory buffer.

**/
#define PAL_SET_PSTATE      263

/**
  PAL Procedure - PAL_SHUTDOWN.

  Put the logical processor into a low power state which can be
  exited only by a reset event. It is optional by Itanium processors. The PAL
  procedure supports the Static Registers calling convention. It
  could be called at physical mode.

  @param Index            Index of PAL_SHUTDOWN within the list of PAL
                          procedures.
  @param NotifyPlatform   8-byte aligned physical address
                          pointer providing details on how to
                          optionally notify the platform that
                          the processor is entering a shutdown
                          state.

  @retval -1              Unimplemented procedure
  @retval -2              Invalid argument
  @retval -3              Call completed with error.
  @retval -9              Call requires PAL memory buffer.

**/
#define PAL_SHUTDOWN        45

/**

  Layout of PAL_MEMORY_BUFFER.ControlWord

**/
typedef struct {
  UINT64  Registration:1;
  UINT64  ProbeInterrupt:1;
  UINT64  Reserved:62;
} PAL_MEMORY_CONTROL_WORD;

/**
  PAL Procedure - PAL_MEMORY_BUFFER.

  Provides cacheable memory to PAL for exclusive use during
  runtime. It is optional by Itanium processors. The PAL procedure supports the
  Static Registers calling convention. It could be called at
  physical mode.

  @param Index        Index of PAL_MEMORY_BUFFER within the list of PAL
                      procedures.
  @param BaseAddress  Physical address of the memory buffer
                      allocated for PAL use.
  @param AllocSize    Unsigned integer denoting the size of the
                      memory buffer.
  @param ControlWord  Formatted bit vector that provides control
                      options for this procedure. See
                      PAL_MEMORY_CONTROL_WORD above.

  @retval 1           Call has not completed a buffer relocation
                      due to a pending interrupt
  @retval 0           Call completed without error
  @retval -1          Unimplemented procedure
  @retval -2          Invalid argument
  @retval -3          Call completed with error.
  @retval -9          Call requires PAL memory buffer.

  @return R9          Returns the minimum size of the memory buffer
                      required if the alloc_size input argument was
                      not large enough.

**/
#define PAL_MEMORY_BUFFER   277


/**
  PAL Procedure - PAL_VP_CREATE.

  Initializes a new vpd for the operation of a new virtual
  processor in the virtual environment. It is optional by Itanium processors.
  The PAL procedure supports the Stacked Registers calling
  convention. It could be called at Virtual mode.

  @param Index            Index of PAL_VP_CREATE within the list of PAL
                          procedures.
  @param Vpd              64-bit host virtual pointer to the Virtual
                          Processor Descriptor (VPD).
  @param HostIva          64-bit host virtual pointer to the host IVT
                          for the virtual processor
  @param OptionalHandler  64-bit non-zero host-virtual pointer
                          to an optional handler for
                          virtualization intercepts.

  @retval 0               Call completed without error
  @retval -1              Unimplemented procedure
  @retval -2              Invalid argument
  @retval -3              Call completed with error.
  @retval -9              Call requires PAL memory buffer.

**/
#define PAL_VP_CREATE       265

/**

  Virtual Environment Information Parameter

**/
typedef struct {
  UINT64    Reserved1:8;
  UINT64    Opcode:1;
  UINT64    Reserved:53;
} PAL_VP_ENV_INFO_RETURN;

/**
  PAL Procedure - PAL_VP_ENV_INFO.

  Returns the parameters needed to enter a virtual environment.
  It is optional by Itanium processors. The PAL procedure supports the Stacked
  Registers calling convention. It could be called at Virtual
  mode.

  @param Index            Index of PAL_VP_ENV_INFO within the list of PAL
                          procedures.
  @param Vpd              64-bit host virtual pointer to the Virtual
                          Processor Descriptor (VPD).
  @param HostIva          64-bit host virtual pointer to the host IVT
                          for the virtual processor
  @param OptionalHandler  64-bit non-zero host-virtual pointer
                          to an optional handler for
                          virtualization intercepts.

  @retval 0               Call completed without error
  @retval -1              Unimplemented procedure
  @retval -2              Invalid argument
  @retval -3              Call completed with error.
  @retval -9              Call requires PAL memory buffer.

  @return R9              Unsigned integer denoting the number of bytes
                          required by the PAL virtual environment buffer
                          during PAL_VP_INIT_ENV
  @return R10             64-bit vector of virtual environment
                          information. See PAL_VP_ENV_INFO_RETURN.


**/
#define PAL_VP_ENV_INFO       266

/**
  PAL Procedure - PAL_VP_EXIT_ENV.

  Allows a logical processor to exit a virtual environment.
  It is optional by Itanium processors. The PAL procedure supports the Stacked
  Registers calling convention. It could be called at Virtual
  mode.

  @param Index  Index of PAL_VP_EXIT_ENV within the list of PAL
                procedures.
  @param Iva    Optional 64-bit host virtual pointer to the IVT
                when this procedure is done

  @retval 0     Call completed without error
  @retval -1    Unimplemented procedure
  @retval -2    Invalid argument
  @retval -3    Call completed with error.
  @retval -9    Call requires PAL memory buffer.

**/
#define PAL_VP_EXIT_ENV       267



/**
  PAL Procedure - PAL_VP_INIT_ENV.

  Allows a logical processor to enter a virtual environment. It
  is optional by Itanium processors. The PAL procedure supports the Stacked
  Registers calling convention. It could be called at Virtual
  mode.

  @param Index          Index of PAL_VP_INIT_ENV within the list of PAL
                        procedures.
  @param ConfigOptions  64-bit vector of global configuration
                        settings.
  @param PhysicalBase   Host physical base address of a block of
                        contiguous physical memory for the PAL
                        virtual environment buffer 1) This
                        memory area must be allocated by the VMM
                        and be 4K aligned. The first logical
                        processor to enter the environment will
                        initialize the physical block for
                        virtualization operations.
  @param VirtualBase    Host virtual base address of the
                        corresponding physical memory block for
                        the PAL virtual environment buffer : The
                        VMM must maintain the host virtual to host
                        physical data and instruction translations
                        in TRs for addresses within the allocated
                        address space. Logical processors in this
                        virtual environment will use this address
                        when transitioning to virtual mode
                        operations.

  @retval 0             Call completed without error
  @retval -1            Unimplemented procedure
  @retval -2            Invalid argument
  @retval -3            Call completed with error.
  @retval -9            Call requires PAL memory buffer.

  @return R9            Virtualization Service Address - VSA specifies
                        the virtual base address of the PAL
                        virtualization services in this virtual
                        environment.


**/
#define PAL_VP_INIT_ENV       268


/**
  PAL Procedure - PAL_VP_REGISTER.

  Register a different host IVT and/or a different optional
  virtualization intercept handler for the virtual processor
  specified by vpd. It is optional by Itanium processors. The PAL procedure
  supports the Stacked Registers calling convention. It could be
  called at Virtual mode.

  @param Index            Index of PAL_VP_REGISTER within the list of PAL
                          procedures.
  @param Vpd              64-bit host virtual pointer to the Virtual
                          Processor Descriptor (VPD) host_iva 64-bit host
                          virtual pointer to the host IVT for the virtual
                          processor
  @param OptionalHandler  64-bit non-zero host-virtual pointer
                          to an optional handler for
                          virtualization intercepts.

  @retval 0               Call completed without error
  @retval -1              Unimplemented procedure
  @retval -2              Invalid argument
  @retval -3              Call completed with error.
  @retval -9              Call requires PAL memory buffer.

**/
#define PAL_VP_REGISTER       269


/**
  PAL Procedure - PAL_VP_RESTORE.

  Restores virtual processor state for the specified vpd on the
  logical processor. It is optional by Itanium processors. The PAL procedure
  supports the Stacked Registers calling convention. It could be
  called at Virtual mode.

  @param Index      Index of PAL_VP_RESTORE within the list of PAL
                    procedures.
  @param Vpd        64-bit host virtual pointer to the Virtual
                    Processor Descriptor (VPD) host_iva 64-bit host
                    virtual pointer to the host IVT for the virtual
                    processor
  @param PalVector  Vector specifies PAL procedure
                    implementation-specific state to be
                    restored.

  @retval 0         Call completed without error
  @retval -1        Unimplemented procedure
  @retval -2        Invalid argument
  @retval -3        Call completed with error.
  @retval -9        Call requires PAL memory buffer.

**/
#define PAL_VP_RESTORE       270

/**
  PAL Procedure - PAL_VP_SAVE.

  Saves virtual processor state for the specified vpd on the
  logical processor. It is optional by Itanium processors. The PAL procedure
  supports the Stacked Registers calling convention. It could be
  called at Virtual mode.

  @param Index      Index of PAL_VP_SAVE within the list of PAL
                    procedures.
  @param Vpd        64-bit host virtual pointer to the Virtual
                    Processor Descriptor (VPD) host_iva 64-bit host
                    virtual pointer to the host IVT for the virtual
                    processor
  @param PalVector  Vector specifies PAL procedure
                    implementation-specific state to be
                    restored.

  @retval 0         Call completed without error
  @retval -1        Unimplemented procedure
  @retval -2        Invalid argument
  @retval -3        Call completed with error.
  @retval -9        Call requires PAL memory buffer.

**/
#define PAL_VP_SAVE       271


/**
  PAL Procedure - PAL_VP_TERMINATE.

  Terminates operation for the specified virtual processor. It
  is optional by Itanium processors. The PAL procedure supports the Stacked
  Registers calling convention. It could be called at Virtual
  mode.

  @param Index  Index of PAL_VP_TERMINATE within the list of PAL
                procedures.
  @param Vpd    64-bit host virtual pointer to the Virtual
                Processor Descriptor (VPD)
  @param Iva    Optional 64-bit host virtual pointer to the IVT
                when this procedure is done.

  @retval 0     Call completed without error
  @retval -1    Unimplemented procedure
  @retval -2    Invalid argument
  @retval -3    Call completed with error.
  @retval -9    Call requires PAL memory buffer.

**/
#define PAL_VP_TERMINATE       272

#endif
