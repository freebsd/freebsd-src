/******************************************************************************
 *
 * Name: acmacros.h - C macros for the entire subsystem.
 *       $Revision: 97 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#ifndef __ACMACROS_H__
#define __ACMACROS_H__


/*
 * Data manipulation macros
 */

#ifndef LOWORD
#define LOWORD(l)                       ((UINT16)(NATIVE_UINT)(l))
#endif

#ifndef HIWORD
#define HIWORD(l)                       ((UINT16)((((NATIVE_UINT)(l)) >> 16) & 0xFFFF))
#endif

#ifndef LOBYTE
#define LOBYTE(l)                       ((UINT8)(UINT16)(l))
#endif

#ifndef HIBYTE
#define HIBYTE(l)                       ((UINT8)((((UINT16)(l)) >> 8) & 0xFF))
#endif

#define BIT0(x)                         ((((x) & 0x01) > 0) ? 1 : 0)
#define BIT1(x)                         ((((x) & 0x02) > 0) ? 1 : 0)
#define BIT2(x)                         ((((x) & 0x04) > 0) ? 1 : 0)

#define BIT3(x)                         ((((x) & 0x08) > 0) ? 1 : 0)
#define BIT4(x)                         ((((x) & 0x10) > 0) ? 1 : 0)
#define BIT5(x)                         ((((x) & 0x20) > 0) ? 1 : 0)
#define BIT6(x)                         ((((x) & 0x40) > 0) ? 1 : 0)
#define BIT7(x)                         ((((x) & 0x80) > 0) ? 1 : 0)

#define LOW_BASE(w)                     ((UINT16) ((w) & 0x0000FFFF))
#define MID_BASE(b)                     ((UINT8) (((b) & 0x00FF0000) >> 16))
#define HI_BASE(b)                      ((UINT8) (((b) & 0xFF000000) >> 24))
#define LOW_LIMIT(w)                    ((UINT16) ((w) & 0x0000FFFF))
#define HI_LIMIT(b)                     ((UINT8) (((b) & 0x00FF0000) >> 16))


#ifdef _IA16
/*
 * For 16-bit addresses, we have to assume that the upper 32 bits
 * are zero.
 */
#ifndef LODWORD
#define LODWORD(l)                      (l)
#endif

#ifndef HIDWORD
#define HIDWORD(l)                      (0)
#endif

#define ACPI_GET_ADDRESS(a)             ((a).Lo)
#define ACPI_STORE_ADDRESS(a,b)         {(a).Hi=0;(a).Lo=(b);}
#define ACPI_VALID_ADDRESS(a)           ((a).Hi | (a).Lo)

#else
#ifdef ACPI_NO_INTEGER64_SUPPORT
/*
 * ACPI_INTEGER is 32-bits, no 64-bit support on this platform
 */
#ifndef LODWORD
#define LODWORD(l)                      ((UINT32)(l))
#endif

#ifndef HIDWORD
#define HIDWORD(l)                      (0)
#endif

#define ACPI_GET_ADDRESS(a)             (a)
#define ACPI_STORE_ADDRESS(a,b)         ((a)=(b))
#define ACPI_VALID_ADDRESS(a)           (a)

#else

/*
 * Full 64-bit address/integer on both 32-bit and 64-bit platforms
 */
#ifndef LODWORD
#define LODWORD(l)                      ((UINT32)(UINT64)(l))
#endif

#ifndef HIDWORD
#define HIDWORD(l)                      ((UINT32)(((*(UINT64_STRUCT *)(&l))).Hi))
#endif

#define ACPI_GET_ADDRESS(a)             (a)
#define ACPI_STORE_ADDRESS(a,b)         ((a)=(b))
#define ACPI_VALID_ADDRESS(a)           (a)
#endif
#endif

 /*
  * Extract a byte of data using a pointer.  Any more than a byte and we
  * get into potential aligment issues -- see the STORE macros below
  */
#define GET8(addr)                      (*(UINT8*)(addr))

/* Pointer arithmetic */


#define POINTER_ADD(t,a,b)              (t *) ((NATIVE_UINT)(a) + (NATIVE_UINT)(b))
#define POINTER_DIFF(a,b)               ((UINT32) ((NATIVE_UINT)(a) - (NATIVE_UINT)(b)))

/*
 * Macros for moving data around to/from buffers that are possibly unaligned.
 * If the hardware supports the transfer of unaligned data, just do the store.
 * Otherwise, we have to move one byte at a time.
 */

#ifdef _HW_ALIGNMENT_SUPPORT

/* The hardware supports unaligned transfers, just do the move */

#define MOVE_UNALIGNED16_TO_16(d,s)     *(UINT16*)(d) = *(UINT16*)(s)
#define MOVE_UNALIGNED32_TO_32(d,s)     *(UINT32*)(d) = *(UINT32*)(s)
#define MOVE_UNALIGNED16_TO_32(d,s)     *(UINT32*)(d) = *(UINT16*)(s)
#define MOVE_UNALIGNED64_TO_64(d,s)     *(UINT64*)(d) = *(UINT64*)(s)

#else
/*
 * The hardware does not support unaligned transfers.  We must move the
 * data one byte at a time.  These macros work whether the source or
 * the destination (or both) is/are unaligned.
 */

#define MOVE_UNALIGNED16_TO_16(d,s)     {((UINT8 *)(d))[0] = ((UINT8 *)(s))[0];\
                                         ((UINT8 *)(d))[1] = ((UINT8 *)(s))[1];}

#define MOVE_UNALIGNED32_TO_32(d,s)     {((UINT8 *)(d))[0] = ((UINT8 *)(s))[0];\
                                         ((UINT8 *)(d))[1] = ((UINT8 *)(s))[1];\
                                         ((UINT8 *)(d))[2] = ((UINT8 *)(s))[2];\
                                         ((UINT8 *)(d))[3] = ((UINT8 *)(s))[3];}

#define MOVE_UNALIGNED16_TO_32(d,s)     {(*(UINT32*)(d)) = 0; MOVE_UNALIGNED16_TO_16(d,s);}

#define MOVE_UNALIGNED64_TO_64(d,s)     {((UINT8 *)(d))[0] = ((UINT8 *)(s))[0];\
                                         ((UINT8 *)(d))[1] = ((UINT8 *)(s))[1];\
                                         ((UINT8 *)(d))[2] = ((UINT8 *)(s))[2];\
                                         ((UINT8 *)(d))[3] = ((UINT8 *)(s))[3];\
                                         ((UINT8 *)(d))[4] = ((UINT8 *)(s))[4];\
                                         ((UINT8 *)(d))[5] = ((UINT8 *)(s))[5];\
                                         ((UINT8 *)(d))[6] = ((UINT8 *)(s))[6];\
                                         ((UINT8 *)(d))[7] = ((UINT8 *)(s))[7];}

#endif


/*
 * Fast power-of-two math macros for non-optimized compilers
 */

#define _DIV(value,PowerOf2)            ((UINT32) ((value) >> (PowerOf2)))
#define _MUL(value,PowerOf2)            ((UINT32) ((value) << (PowerOf2)))
#define _MOD(value,Divisor)             ((UINT32) ((value) & ((Divisor) -1)))

#define DIV_2(a)                        _DIV(a,1)
#define MUL_2(a)                        _MUL(a,1)
#define MOD_2(a)                        _MOD(a,2)

#define DIV_4(a)                        _DIV(a,2)
#define MUL_4(a)                        _MUL(a,2)
#define MOD_4(a)                        _MOD(a,4)

#define DIV_8(a)                        _DIV(a,3)
#define MUL_8(a)                        _MUL(a,3)
#define MOD_8(a)                        _MOD(a,8)

#define DIV_16(a)                       _DIV(a,4)
#define MUL_16(a)                       _MUL(a,4)
#define MOD_16(a)                       _MOD(a,16)


/*
 * Rounding macros (Power of two boundaries only)
 */
#define ROUND_DOWN(value,boundary)      ((value) & (~((boundary)-1)))
#define ROUND_UP(value,boundary)        (((value) + ((boundary)-1)) & (~((boundary)-1)))

#define ROUND_DOWN_TO_32_BITS(a)        ROUND_DOWN(a,4)
#define ROUND_DOWN_TO_64_BITS(a)        ROUND_DOWN(a,8)
#define ROUND_DOWN_TO_NATIVE_WORD(a)    ROUND_DOWN(a,ALIGNED_ADDRESS_BOUNDARY)

#define ROUND_UP_TO_32BITS(a)           ROUND_UP(a,4)
#define ROUND_UP_TO_64BITS(a)           ROUND_UP(a,8)
#define ROUND_UP_TO_NATIVE_WORD(a)      ROUND_UP(a,ALIGNED_ADDRESS_BOUNDARY)

#define ROUND_PTR_UP_TO_4(a,b)          ((b *)(((NATIVE_UINT)(a) + 3) & ~3))
#define ROUND_PTR_UP_TO_8(a,b)          ((b *)(((NATIVE_UINT)(a) + 7) & ~7))

#define ROUND_BITS_UP_TO_BYTES(a)       DIV_8((a) + 7)
#define ROUND_BITS_DOWN_TO_BYTES(a)     DIV_8((a))

#define ROUND_UP_TO_1K(a)               (((a) + 1023) >> 10)

/* Generic (non-power-of-two) rounding */

#define ROUND_UP_TO(value,boundary)     (((value) + ((boundary)-1)) / (boundary))

/*
 * Bitmask creation
 * Bit positions start at zero.
 * MASK_BITS_ABOVE creates a mask starting AT the position and above
 * MASK_BITS_BELOW creates a mask starting one bit BELOW the position
 */
#define MASK_BITS_ABOVE(position)       (~(((UINT32)(-1)) << ((UINT32) (position))))
#define MASK_BITS_BELOW(position)       (((UINT32)(-1)) << ((UINT32) (position)))


/* Macros for GAS addressing */

#ifndef _IA16

#define ACPI_PCI_DEVICE_MASK            (UINT64) 0x0000FFFF00000000
#define ACPI_PCI_FUNCTION_MASK          (UINT64) 0x00000000FFFF0000
#define ACPI_PCI_REGISTER_MASK          (UINT64) 0x000000000000FFFF

#define ACPI_PCI_FUNCTION(a)            (UINT16) ((((a) & ACPI_PCI_FUNCTION_MASK) >> 16))
#define ACPI_PCI_DEVICE(a)              (UINT16) ((((a) & ACPI_PCI_DEVICE_MASK) >> 32))
#define ACPI_PCI_REGISTER(a)            (UINT16) (((a) & ACPI_PCI_REGISTER_MASK))

#else

/* No support for GAS and PCI IDs in 16-bit mode  */

#define ACPI_PCI_FUNCTION(a)            (UINT16) ((a) & 0xFFFF0000)
#define ACPI_PCI_DEVICE(a)              (UINT16) ((a) & 0x0000FFFF)
#define ACPI_PCI_REGISTER(a)            (UINT16) ((a) & 0x0000FFFF)

#endif

/*
 * An ACPI_HANDLE (which is actually an ACPI_NAMESPACE_NODE *) can appear in some contexts,
 * such as on apObjStack, where a pointer to an ACPI_OPERAND_OBJECT  can also
 * appear.  This macro is used to distinguish them.
 *
 * The DataType field is the first field in both structures.
 */
#define VALID_DESCRIPTOR_TYPE(d,t)      (((ACPI_NAMESPACE_NODE *)d)->DataType == t)


/* Macro to test the object type */

#define IS_THIS_OBJECT_TYPE(d,t)        (((ACPI_OPERAND_OBJECT  *)d)->Common.Type == (UINT8)t)

/* Macro to check the table flags for SINGLE or MULTIPLE tables are allowed */

#define IS_SINGLE_TABLE(x)              (((x) & 0x01) == ACPI_TABLE_SINGLE ? 1 : 0)

/*
 * Macro to check if a pointer is within an ACPI table.
 * Parameter (a) is the pointer to check.  Parameter (b) must be defined
 * as a pointer to an ACPI_TABLE_HEADER.  (b+1) then points past the header,
 * and ((UINT8 *)b+b->Length) points one byte past the end of the table.
 */
#ifndef _IA16
#define IS_IN_ACPI_TABLE(a,b)           (((UINT8 *)(a) >= (UINT8 *)(b + 1)) &&\
                                        ((UINT8 *)(a) < ((UINT8 *)b + b->Length)))

#else
#define IS_IN_ACPI_TABLE(a,b)           (_segment)(a) == (_segment)(b) &&\
                                        (((UINT8 *)(a) >= (UINT8 *)(b + 1)) &&\
                                        ((UINT8 *)(a) < ((UINT8 *)b + b->Length)))
#endif

/*
 * Macros for the master AML opcode table
 */
#ifdef ACPI_DEBUG
#define ACPI_OP(Name,PArgs,IArgs,Class,Type,Flags)     {PArgs,IArgs,Flags,Class,Type,Name}
#else
#define ACPI_OP(Name,PArgs,IArgs,Class,Type,Flags)     {PArgs,IArgs,Flags,Class,Type}
#endif

#define ARG_TYPE_WIDTH                  5
#define ARG_1(x)                        ((UINT32)(x))
#define ARG_2(x)                        ((UINT32)(x) << (1 * ARG_TYPE_WIDTH))
#define ARG_3(x)                        ((UINT32)(x) << (2 * ARG_TYPE_WIDTH))
#define ARG_4(x)                        ((UINT32)(x) << (3 * ARG_TYPE_WIDTH))
#define ARG_5(x)                        ((UINT32)(x) << (4 * ARG_TYPE_WIDTH))
#define ARG_6(x)                        ((UINT32)(x) << (5 * ARG_TYPE_WIDTH))

#define ARGI_LIST1(a)                   (ARG_1(a))
#define ARGI_LIST2(a,b)                 (ARG_1(b)|ARG_2(a))
#define ARGI_LIST3(a,b,c)               (ARG_1(c)|ARG_2(b)|ARG_3(a))
#define ARGI_LIST4(a,b,c,d)             (ARG_1(d)|ARG_2(c)|ARG_3(b)|ARG_4(a))
#define ARGI_LIST5(a,b,c,d,e)           (ARG_1(e)|ARG_2(d)|ARG_3(c)|ARG_4(b)|ARG_5(a))
#define ARGI_LIST6(a,b,c,d,e,f)         (ARG_1(f)|ARG_2(e)|ARG_3(d)|ARG_4(c)|ARG_5(b)|ARG_6(a))

#define ARGP_LIST1(a)                   (ARG_1(a))
#define ARGP_LIST2(a,b)                 (ARG_1(a)|ARG_2(b))
#define ARGP_LIST3(a,b,c)               (ARG_1(a)|ARG_2(b)|ARG_3(c))
#define ARGP_LIST4(a,b,c,d)             (ARG_1(a)|ARG_2(b)|ARG_3(c)|ARG_4(d))
#define ARGP_LIST5(a,b,c,d,e)           (ARG_1(a)|ARG_2(b)|ARG_3(c)|ARG_4(d)|ARG_5(e))
#define ARGP_LIST6(a,b,c,d,e,f)         (ARG_1(a)|ARG_2(b)|ARG_3(c)|ARG_4(d)|ARG_5(e)|ARG_6(f))

#define GET_CURRENT_ARG_TYPE(List)      (List & ((UINT32) 0x1F))
#define INCREMENT_ARG_LIST(List)        (List >>= ((UINT32) ARG_TYPE_WIDTH))


/*
 * Build a GAS structure from earlier ACPI table entries (V1.0 and 0.71 extensions)
 *
 * 1) Address space
 * 2) Length in bytes -- convert to length in bits
 * 3) Bit offset is zero
 * 4) Reserved field is zero
 * 5) Expand address to 64 bits
 */
#define ASL_BUILD_GAS_FROM_ENTRY(a,b,c,d)   {a.AddressSpaceId = (UINT8) d;\
                                             a.RegisterBitWidth = (UINT8) MUL_8 (b);\
                                             a.RegisterBitOffset = 0;\
                                             a.Reserved = 0;\
                                             ACPI_STORE_ADDRESS (a.Address,c);}

/* ACPI V1.0 entries -- address space is always I/O */

#define ASL_BUILD_GAS_FROM_V1_ENTRY(a,b,c)  ASL_BUILD_GAS_FROM_ENTRY(a,b,c,ACPI_ADR_SPACE_SYSTEM_IO)


/*
 * Reporting macros that are never compiled out
 */

#define PARAM_LIST(pl)                  pl

/*
 * Error reporting.  These versions add callers module and line#.  Since
 * _THIS_MODULE gets compiled out when ACPI_DEBUG isn't defined, only
 * use it in debug mode.
 */

#ifdef ACPI_DEBUG

#define REPORT_INFO(fp)                 {AcpiUtReportInfo(_THIS_MODULE,__LINE__,_COMPONENT); \
                                            AcpiOsPrintf PARAM_LIST(fp);}
#define REPORT_ERROR(fp)                {AcpiUtReportError(_THIS_MODULE,__LINE__,_COMPONENT); \
                                            AcpiOsPrintf PARAM_LIST(fp);}
#define REPORT_WARNING(fp)              {AcpiUtReportWarning(_THIS_MODULE,__LINE__,_COMPONENT); \
                                            AcpiOsPrintf PARAM_LIST(fp);}

#else

#define REPORT_INFO(fp)                 {AcpiUtReportInfo("ACPI",__LINE__,_COMPONENT); \
                                            AcpiOsPrintf PARAM_LIST(fp);}
#define REPORT_ERROR(fp)                {AcpiUtReportError("ACPI",__LINE__,_COMPONENT); \
                                            AcpiOsPrintf PARAM_LIST(fp);}
#define REPORT_WARNING(fp)              {AcpiUtReportWarning("ACPI",__LINE__,_COMPONENT); \
                                            AcpiOsPrintf PARAM_LIST(fp);}

#endif

/* Error reporting.  These versions pass thru the module and line# */

#define _REPORT_INFO(a,b,c,fp)          {AcpiUtReportInfo(a,b,c); \
                                            AcpiOsPrintf PARAM_LIST(fp);}
#define _REPORT_ERROR(a,b,c,fp)         {AcpiUtReportError(a,b,c); \
                                            AcpiOsPrintf PARAM_LIST(fp);}
#define _REPORT_WARNING(a,b,c,fp)       {AcpiUtReportWarning(a,b,c); \
                                            AcpiOsPrintf PARAM_LIST(fp);}

/*
 * Debug macros that are conditionally compiled
 */

#ifdef ACPI_DEBUG

#define MODULE_NAME(name)               static char *_THIS_MODULE = name;

/*
 * Function entry tracing.
 * The first parameter should be the procedure name as a quoted string.  This is declared
 * as a local string ("_ProcName) so that it can be also used by the function exit macros below.
 */

#define PROC_NAME(a)                    ACPI_DEBUG_PRINT_INFO _Dbg;     \
                                        _Dbg.ComponentId = _COMPONENT;  \
                                        _Dbg.ProcName    = a;           \
                                        _Dbg.ModuleName  = _THIS_MODULE;

#define FUNCTION_TRACE(a)               PROC_NAME(a)\
                                        AcpiUtTrace(__LINE__,&_Dbg)
#define FUNCTION_TRACE_PTR(a,b)         PROC_NAME(a)\
                                        AcpiUtTracePtr(__LINE__,&_Dbg,(void *)b)
#define FUNCTION_TRACE_U32(a,b)         PROC_NAME(a)\
                                        AcpiUtTraceU32(__LINE__,&_Dbg,(UINT32)b)
#define FUNCTION_TRACE_STR(a,b)         PROC_NAME(a)\
                                        AcpiUtTraceStr(__LINE__,&_Dbg,(NATIVE_CHAR *)b)

#define FUNCTION_ENTRY()                AcpiUtTrackStackPtr()

/*
 * Function exit tracing.
 * WARNING: These macros include a return statement.  This is usually considered
 * bad form, but having a separate exit macro is very ugly and difficult to maintain.
 * One of the FUNCTION_TRACE macros above must be used in conjunction with these macros
 * so that "_ProcName" is defined.
 */
#define return_VOID                     {AcpiUtExit(__LINE__,&_Dbg);return;}
#define return_ACPI_STATUS(s)           {AcpiUtStatusExit(__LINE__,&_Dbg,s);return(s);}
#define return_VALUE(s)                 {AcpiUtValueExit(__LINE__,&_Dbg,s);return(s);}
#define return_PTR(s)                   {AcpiUtPtrExit(__LINE__,&_Dbg,(UINT8 *)s);return(s);}


/* Conditional execution */

#define DEBUG_EXEC(a)                   a
#define NORMAL_EXEC(a)

#define DEBUG_DEFINE(a)                 a;
#define DEBUG_ONLY_MEMBERS(a)           a;
#define _OPCODE_NAMES
#define _VERBOSE_STRUCTURES


/* Stack and buffer dumping */

#define DUMP_STACK_ENTRY(a)             AcpiExDumpOperand(a)
#define DUMP_OPERANDS(a,b,c,d,e)        AcpiExDumpOperands(a,b,c,d,e,_THIS_MODULE,__LINE__)


#define DUMP_ENTRY(a,b)                 AcpiNsDumpEntry (a,b)
#define DUMP_TABLES(a,b)                AcpiNsDumpTables(a,b)
#define DUMP_PATHNAME(a,b,c,d)          AcpiNsDumpPathname(a,b,c,d)
#define DUMP_RESOURCE_LIST(a)           AcpiRsDumpResourceList(a)
#define DUMP_BUFFER(a,b)                AcpiUtDumpBuffer((UINT8 *)a,b,DB_BYTE_DISPLAY,_COMPONENT)
#define BREAK_MSG(a)                    AcpiOsSignal (ACPI_SIGNAL_BREAKPOINT,(a))


/*
 * Generate INT3 on ACPI_ERROR (Debug only!)
 */

#define ERROR_BREAK
#ifdef  ERROR_BREAK
#define BREAK_ON_ERROR(lvl)              if ((lvl)&ACPI_ERROR) AcpiOsSignal(ACPI_SIGNAL_BREAKPOINT,"Fatal error encountered\n")
#else
#define BREAK_ON_ERROR(lvl)
#endif

/*
 * Master debug print macros
 * Print iff:
 *    1) Debug print for the current component is enabled
 *    2) Debug error level or trace level for the print statement is enabled
 */

#define ACPI_DEBUG_PRINT(pl)            AcpiUtDebugPrint PARAM_LIST(pl)
#define ACPI_DEBUG_PRINT_RAW(pl)        AcpiUtDebugPrintRaw PARAM_LIST(pl)


#else
/*
 * This is the non-debug case -- make everything go away,
 * leaving no executable debug code!
 */

#define MODULE_NAME(name)
#define _THIS_MODULE ""

#define DEBUG_EXEC(a)
#define NORMAL_EXEC(a)                  a;

#define DEBUG_DEFINE(a)
#define DEBUG_ONLY_MEMBERS(a)
#define PROC_NAME(a)
#define FUNCTION_TRACE(a)
#define FUNCTION_TRACE_PTR(a,b)
#define FUNCTION_TRACE_U32(a,b)
#define FUNCTION_TRACE_STR(a,b)
#define FUNCTION_EXIT
#define FUNCTION_STATUS_EXIT(s)
#define FUNCTION_VALUE_EXIT(s)
#define FUNCTION_ENTRY()
#define DUMP_STACK_ENTRY(a)
#define DUMP_OPERANDS(a,b,c,d,e)
#define DUMP_ENTRY(a,b)
#define DUMP_TABLES(a,b)
#define DUMP_PATHNAME(a,b,c,d)
#define DUMP_RESOURCE_LIST(a)
#define DUMP_BUFFER(a,b)
#define ACPI_DEBUG_PRINT(pl)
#define ACPI_DEBUG_PRINT_RAW(pl)
#define BREAK_MSG(a)

#define return_VOID                     return
#define return_ACPI_STATUS(s)           return(s)
#define return_VALUE(s)                 return(s)
#define return_PTR(s)                   return(s)

#endif

/*
 * Some code only gets executed when the debugger is built in.
 * Note that this is entirely independent of whether the
 * DEBUG_PRINT stuff (set by ACPI_DEBUG) is on, or not.
 */
#ifdef ENABLE_DEBUGGER
#define DEBUGGER_EXEC(a)                a
#else
#define DEBUGGER_EXEC(a)
#endif


/*
 * For 16-bit code, we want to shrink some things even though
 * we are using ACPI_DEBUG to get the debug output
 */
#ifdef _IA16
#undef DEBUG_ONLY_MEMBERS
#undef _VERBOSE_STRUCTURES
#define DEBUG_ONLY_MEMBERS(a)
#endif


#ifdef ACPI_DEBUG
/*
 * 1) Set name to blanks
 * 2) Copy the object name
 */
#define ADD_OBJECT_NAME(a,b)            MEMSET (a->Common.Name, ' ', sizeof (a->Common.Name));\
                                        STRNCPY (a->Common.Name, AcpiGbl_NsTypeNames[b], sizeof (a->Common.Name))
#else

#define ADD_OBJECT_NAME(a,b)
#endif


/*
 * Memory allocation tracking (DEBUG ONLY)
 */

#ifndef ACPI_DBG_TRACK_ALLOCATIONS

/* Memory allocation */

#define ACPI_MEM_ALLOCATE(a)            AcpiOsAllocate(a)
#define ACPI_MEM_CALLOCATE(a)           AcpiOsCallocate(a)
#define ACPI_MEM_FREE(a)                AcpiOsFree(a)
#define ACPI_MEM_TRACKING(a)


#else

/* Memory allocation */

#define ACPI_MEM_ALLOCATE(a)            AcpiUtAllocate(a,_COMPONENT,_THIS_MODULE,__LINE__)
#define ACPI_MEM_CALLOCATE(a)           AcpiUtCallocate(a, _COMPONENT,_THIS_MODULE,__LINE__)
#define ACPI_MEM_FREE(a)                AcpiUtFree(a,_COMPONENT,_THIS_MODULE,__LINE__)
#define ACPI_MEM_TRACKING(a)            a

#endif /* ACPI_DBG_TRACK_ALLOCATIONS */


#define ACPI_GET_STACK_POINTER          _asm {mov eax, ebx}

#endif /* ACMACROS_H */
