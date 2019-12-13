%{
/******************************************************************************
 *
 * Module Name: aslparser.y - Master Bison/Yacc input file for iASL
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2019, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include "aslcompiler.h"
#include "acpi.h"
#include "accommon.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslparse")

/*
 * Global Notes:
 *
 * October 2005: The following list terms have been optimized (from the
 * original ASL grammar in the ACPI specification) to force the immediate
 * reduction of each list item so that the parse stack use doesn't increase on
 * each list element and possibly overflow on very large lists (>4000 items).
 * This dramatically reduces use of the parse stack overall.
 *
 *      ArgList, TermList, ByteList, DWordList, PackageList,
 *      ResourceMacroList, and FieldUnitList
 */

void *
AslLocalAllocate (
    unsigned int            Size);


/* Bison/yacc configuration */

#define static
#undef malloc
#define malloc              AslLocalAllocate
#undef alloca
#define alloca              AslLocalAllocate
#define yytname             AslCompilername

#define YYINITDEPTH         600             /* State stack depth */
#define YYDEBUG             1               /* Enable debug output */
#define YYERROR_VERBOSE     1               /* Verbose error messages */
#define YYFLAG              -32768

/* Define YYMALLOC/YYFREE to prevent redefinition errors  */

#define YYMALLOC            AslLocalAllocate
#define YYFREE              ACPI_FREE
%}

/*
 * Declare the type of values in the grammar
 */
%union {
    UINT64              i;
    char                *s;
    ACPI_PARSE_OBJECT   *n;
}

/*
 * These shift/reduce conflicts are expected. There should be zero
 * reduce/reduce conflicts.
 */
%expect 124

/*! [Begin] no source code translation */

/*
 * The M4 macro processor is used to bring in the parser items,
 * in order to keep this master file smaller, and to break up
 * the various parser items.
 */


/* Token types */



/******************************************************************************
 *
 * Token types: These are returned by the lexer
 *
 * NOTE: This list MUST match the AslKeywordMapping table found
 *       in aslmap.c EXACTLY!  Double check any changes!
 *
 *****************************************************************************/

/*
 * Most tokens are defined to return <i>, which is a UINT64.
 *
 * These tokens return <s>, a pointer to the associated lexed string:
 *
 *  PARSEOP_NAMESEG
 *  PARSEOP_NAMESTRING
 *  PARSEOP_STRING_LITERAL
 *  PARSEOP_STRUCTURE_NAMESTRING
 */
%token <i> PARSEOP_ACCESSAS
%token <i> PARSEOP_ACCESSATTRIB_BLOCK
%token <i> PARSEOP_ACCESSATTRIB_BLOCK_CALL
%token <i> PARSEOP_ACCESSATTRIB_BYTE
%token <i> PARSEOP_ACCESSATTRIB_BYTES
%token <i> PARSEOP_ACCESSATTRIB_QUICK
%token <i> PARSEOP_ACCESSATTRIB_RAW_BYTES
%token <i> PARSEOP_ACCESSATTRIB_RAW_PROCESS
%token <i> PARSEOP_ACCESSATTRIB_SND_RCV
%token <i> PARSEOP_ACCESSATTRIB_WORD
%token <i> PARSEOP_ACCESSATTRIB_WORD_CALL
%token <i> PARSEOP_ACCESSTYPE_ANY
%token <i> PARSEOP_ACCESSTYPE_BUF
%token <i> PARSEOP_ACCESSTYPE_BYTE
%token <i> PARSEOP_ACCESSTYPE_DWORD
%token <i> PARSEOP_ACCESSTYPE_QWORD
%token <i> PARSEOP_ACCESSTYPE_WORD
%token <i> PARSEOP_ACQUIRE
%token <i> PARSEOP_ADD
%token <i> PARSEOP_ADDRESSINGMODE_7BIT
%token <i> PARSEOP_ADDRESSINGMODE_10BIT
%token <i> PARSEOP_ADDRESSTYPE_ACPI
%token <i> PARSEOP_ADDRESSTYPE_MEMORY
%token <i> PARSEOP_ADDRESSTYPE_NVS
%token <i> PARSEOP_ADDRESSTYPE_RESERVED
%token <i> PARSEOP_ALIAS
%token <i> PARSEOP_AND
%token <i> PARSEOP_ARG0
%token <i> PARSEOP_ARG1
%token <i> PARSEOP_ARG2
%token <i> PARSEOP_ARG3
%token <i> PARSEOP_ARG4
%token <i> PARSEOP_ARG5
%token <i> PARSEOP_ARG6
%token <i> PARSEOP_BANKFIELD
%token <i> PARSEOP_BITSPERBYTE_EIGHT
%token <i> PARSEOP_BITSPERBYTE_FIVE
%token <i> PARSEOP_BITSPERBYTE_NINE
%token <i> PARSEOP_BITSPERBYTE_SEVEN
%token <i> PARSEOP_BITSPERBYTE_SIX
%token <i> PARSEOP_BREAK
%token <i> PARSEOP_BREAKPOINT
%token <i> PARSEOP_BUFFER
%token <i> PARSEOP_BUSMASTERTYPE_MASTER
%token <i> PARSEOP_BUSMASTERTYPE_NOTMASTER
%token <i> PARSEOP_BYTECONST
%token <i> PARSEOP_CASE
%token <i> PARSEOP_CLOCKPHASE_FIRST
%token <i> PARSEOP_CLOCKPHASE_SECOND
%token <i> PARSEOP_CLOCKPOLARITY_HIGH
%token <i> PARSEOP_CLOCKPOLARITY_LOW
%token <i> PARSEOP_CONCATENATE
%token <i> PARSEOP_CONCATENATERESTEMPLATE
%token <i> PARSEOP_CONDREFOF
%token <i> PARSEOP_CONNECTION
%token <i> PARSEOP_CONTINUE
%token <i> PARSEOP_COPYOBJECT
%token <i> PARSEOP_CREATEBITFIELD
%token <i> PARSEOP_CREATEBYTEFIELD
%token <i> PARSEOP_CREATEDWORDFIELD
%token <i> PARSEOP_CREATEFIELD
%token <i> PARSEOP_CREATEQWORDFIELD
%token <i> PARSEOP_CREATEWORDFIELD
%token <i> PARSEOP_DATABUFFER
%token <i> PARSEOP_DATATABLEREGION
%token <i> PARSEOP_DEBUG
%token <i> PARSEOP_DECODETYPE_POS
%token <i> PARSEOP_DECODETYPE_SUB
%token <i> PARSEOP_DECREMENT
%token <i> PARSEOP_DEFAULT
%token <i> PARSEOP_DEFAULT_ARG
%token <i> PARSEOP_DEFINITION_BLOCK
%token <i> PARSEOP_DEREFOF
%token <i> PARSEOP_DEVICE
%token <i> PARSEOP_DEVICEPOLARITY_HIGH
%token <i> PARSEOP_DEVICEPOLARITY_LOW
%token <i> PARSEOP_DIVIDE
%token <i> PARSEOP_DMA
%token <i> PARSEOP_DMATYPE_A
%token <i> PARSEOP_DMATYPE_COMPATIBILITY
%token <i> PARSEOP_DMATYPE_B
%token <i> PARSEOP_DMATYPE_F
%token <i> PARSEOP_DWORDCONST
%token <i> PARSEOP_DWORDIO
%token <i> PARSEOP_DWORDMEMORY
%token <i> PARSEOP_DWORDSPACE
%token <i> PARSEOP_EISAID
%token <i> PARSEOP_ELSE
%token <i> PARSEOP_ELSEIF
%token <i> PARSEOP_ENDDEPENDENTFN
%token <i> PARSEOP_ENDIAN_BIG
%token <i> PARSEOP_ENDIAN_LITTLE
%token <i> PARSEOP_ENDTAG
%token <i> PARSEOP_ERRORNODE
%token <i> PARSEOP_EVENT
%token <i> PARSEOP_EXTENDEDIO
%token <i> PARSEOP_EXTENDEDMEMORY
%token <i> PARSEOP_EXTENDEDSPACE
%token <i> PARSEOP_EXTERNAL
%token <i> PARSEOP_FATAL
%token <i> PARSEOP_FIELD
%token <i> PARSEOP_FINDSETLEFTBIT
%token <i> PARSEOP_FINDSETRIGHTBIT
%token <i> PARSEOP_FIXEDDMA
%token <i> PARSEOP_FIXEDIO
%token <i> PARSEOP_FLOWCONTROL_HW
%token <i> PARSEOP_FLOWCONTROL_NONE
%token <i> PARSEOP_FLOWCONTROL_SW
%token <i> PARSEOP_FROMBCD
%token <i> PARSEOP_FUNCTION
%token <i> PARSEOP_GPIO_INT
%token <i> PARSEOP_GPIO_IO
%token <i> PARSEOP_I2C_SERIALBUS
%token <i> PARSEOP_I2C_SERIALBUS_V2
%token <i> PARSEOP_IF
%token <i> PARSEOP_INCLUDE
%token <i> PARSEOP_INCLUDE_END
%token <i> PARSEOP_INCREMENT
%token <i> PARSEOP_INDEX
%token <i> PARSEOP_INDEXFIELD
%token <i> PARSEOP_INTEGER
%token <i> PARSEOP_INTERRUPT
%token <i> PARSEOP_INTLEVEL_ACTIVEBOTH
%token <i> PARSEOP_INTLEVEL_ACTIVEHIGH
%token <i> PARSEOP_INTLEVEL_ACTIVELOW
%token <i> PARSEOP_INTTYPE_EDGE
%token <i> PARSEOP_INTTYPE_LEVEL
%token <i> PARSEOP_IO
%token <i> PARSEOP_IODECODETYPE_10
%token <i> PARSEOP_IODECODETYPE_16
%token <i> PARSEOP_IORESTRICT_IN
%token <i> PARSEOP_IORESTRICT_NONE
%token <i> PARSEOP_IORESTRICT_OUT
%token <i> PARSEOP_IORESTRICT_PRESERVE
%token <i> PARSEOP_IRQ
%token <i> PARSEOP_IRQNOFLAGS
%token <i> PARSEOP_LAND
%token <i> PARSEOP_LEQUAL
%token <i> PARSEOP_LGREATER
%token <i> PARSEOP_LGREATEREQUAL
%token <i> PARSEOP_LLESS
%token <i> PARSEOP_LLESSEQUAL
%token <i> PARSEOP_LNOT
%token <i> PARSEOP_LNOTEQUAL
%token <i> PARSEOP_LOAD
%token <i> PARSEOP_LOADTABLE
%token <i> PARSEOP_LOCAL0
%token <i> PARSEOP_LOCAL1
%token <i> PARSEOP_LOCAL2
%token <i> PARSEOP_LOCAL3
%token <i> PARSEOP_LOCAL4
%token <i> PARSEOP_LOCAL5
%token <i> PARSEOP_LOCAL6
%token <i> PARSEOP_LOCAL7
%token <i> PARSEOP_LOCKRULE_LOCK
%token <i> PARSEOP_LOCKRULE_NOLOCK
%token <i> PARSEOP_LOR
%token <i> PARSEOP_MATCH
%token <i> PARSEOP_MATCHTYPE_MEQ
%token <i> PARSEOP_MATCHTYPE_MGE
%token <i> PARSEOP_MATCHTYPE_MGT
%token <i> PARSEOP_MATCHTYPE_MLE
%token <i> PARSEOP_MATCHTYPE_MLT
%token <i> PARSEOP_MATCHTYPE_MTR
%token <i> PARSEOP_MAXTYPE_FIXED
%token <i> PARSEOP_MAXTYPE_NOTFIXED
%token <i> PARSEOP_MEMORY24
%token <i> PARSEOP_MEMORY32
%token <i> PARSEOP_MEMORY32FIXED
%token <i> PARSEOP_MEMTYPE_CACHEABLE
%token <i> PARSEOP_MEMTYPE_NONCACHEABLE
%token <i> PARSEOP_MEMTYPE_PREFETCHABLE
%token <i> PARSEOP_MEMTYPE_WRITECOMBINING
%token <i> PARSEOP_METHOD
%token <i> PARSEOP_METHODCALL
%token <i> PARSEOP_MID
%token <i> PARSEOP_MINTYPE_FIXED
%token <i> PARSEOP_MINTYPE_NOTFIXED
%token <i> PARSEOP_MOD
%token <i> PARSEOP_MULTIPLY
%token <i> PARSEOP_MUTEX
%token <i> PARSEOP_NAME
%token <s> PARSEOP_NAMESEG
%token <s> PARSEOP_NAMESTRING
%token <i> PARSEOP_NAND
%token <i> PARSEOP_NOOP
%token <i> PARSEOP_NOR
%token <i> PARSEOP_NOT
%token <i> PARSEOP_NOTIFY
%token <i> PARSEOP_OBJECTTYPE
%token <i> PARSEOP_OBJECTTYPE_BFF
%token <i> PARSEOP_OBJECTTYPE_BUF
%token <i> PARSEOP_OBJECTTYPE_DDB
%token <i> PARSEOP_OBJECTTYPE_DEV
%token <i> PARSEOP_OBJECTTYPE_EVT
%token <i> PARSEOP_OBJECTTYPE_FLD
%token <i> PARSEOP_OBJECTTYPE_INT
%token <i> PARSEOP_OBJECTTYPE_MTH
%token <i> PARSEOP_OBJECTTYPE_MTX
%token <i> PARSEOP_OBJECTTYPE_OPR
%token <i> PARSEOP_OBJECTTYPE_PKG
%token <i> PARSEOP_OBJECTTYPE_POW
%token <i> PARSEOP_OBJECTTYPE_PRO
%token <i> PARSEOP_OBJECTTYPE_STR
%token <i> PARSEOP_OBJECTTYPE_THZ
%token <i> PARSEOP_OBJECTTYPE_UNK
%token <i> PARSEOP_OFFSET
%token <i> PARSEOP_ONE
%token <i> PARSEOP_ONES
%token <i> PARSEOP_OPERATIONREGION
%token <i> PARSEOP_OR
%token <i> PARSEOP_PACKAGE
%token <i> PARSEOP_PACKAGE_LENGTH
%token <i> PARSEOP_PARITYTYPE_EVEN
%token <i> PARSEOP_PARITYTYPE_MARK
%token <i> PARSEOP_PARITYTYPE_NONE
%token <i> PARSEOP_PARITYTYPE_ODD
%token <i> PARSEOP_PARITYTYPE_SPACE
%token <i> PARSEOP_PINCONFIG
%token <i> PARSEOP_PINFUNCTION
%token <i> PARSEOP_PINGROUP
%token <i> PARSEOP_PINGROUPCONFIG
%token <i> PARSEOP_PINGROUPFUNCTION
%token <i> PARSEOP_PIN_NOPULL
%token <i> PARSEOP_PIN_PULLDEFAULT
%token <i> PARSEOP_PIN_PULLDOWN
%token <i> PARSEOP_PIN_PULLUP
%token <i> PARSEOP_POWERRESOURCE
%token <i> PARSEOP_PROCESSOR
%token <i> PARSEOP_QWORDCONST
%token <i> PARSEOP_QWORDIO
%token <i> PARSEOP_QWORDMEMORY
%token <i> PARSEOP_QWORDSPACE
%token <i> PARSEOP_RANGETYPE_ENTIRE
%token <i> PARSEOP_RANGETYPE_ISAONLY
%token <i> PARSEOP_RANGETYPE_NONISAONLY
%token <i> PARSEOP_RAW_DATA
%token <i> PARSEOP_READWRITETYPE_BOTH
%token <i> PARSEOP_READWRITETYPE_READONLY
%token <i> PARSEOP_REFOF
%token <i> PARSEOP_REGIONSPACE_CMOS
%token <i> PARSEOP_REGIONSPACE_EC
%token <i> PARSEOP_REGIONSPACE_FFIXEDHW
%token <i> PARSEOP_REGIONSPACE_GPIO
%token <i> PARSEOP_REGIONSPACE_GSBUS
%token <i> PARSEOP_REGIONSPACE_IO
%token <i> PARSEOP_REGIONSPACE_IPMI
%token <i> PARSEOP_REGIONSPACE_MEM
%token <i> PARSEOP_REGIONSPACE_PCC
%token <i> PARSEOP_REGIONSPACE_PCI
%token <i> PARSEOP_REGIONSPACE_PCIBAR
%token <i> PARSEOP_REGIONSPACE_SMBUS
%token <i> PARSEOP_REGISTER
%token <i> PARSEOP_RELEASE
%token <i> PARSEOP_RESERVED_BYTES
%token <i> PARSEOP_RESET
%token <i> PARSEOP_RESOURCETEMPLATE
%token <i> PARSEOP_RESOURCETYPE_CONSUMER
%token <i> PARSEOP_RESOURCETYPE_PRODUCER
%token <i> PARSEOP_RETURN
%token <i> PARSEOP_REVISION
%token <i> PARSEOP_SCOPE
%token <i> PARSEOP_SERIALIZERULE_NOTSERIAL
%token <i> PARSEOP_SERIALIZERULE_SERIAL
%token <i> PARSEOP_SHARETYPE_EXCLUSIVE
%token <i> PARSEOP_SHARETYPE_EXCLUSIVEWAKE
%token <i> PARSEOP_SHARETYPE_SHARED
%token <i> PARSEOP_SHARETYPE_SHAREDWAKE
%token <i> PARSEOP_SHIFTLEFT
%token <i> PARSEOP_SHIFTRIGHT
%token <i> PARSEOP_SIGNAL
%token <i> PARSEOP_SIZEOF
%token <i> PARSEOP_SLAVEMODE_CONTROLLERINIT
%token <i> PARSEOP_SLAVEMODE_DEVICEINIT
%token <i> PARSEOP_SLEEP
%token <i> PARSEOP_SPI_SERIALBUS
%token <i> PARSEOP_SPI_SERIALBUS_V2
%token <i> PARSEOP_STALL
%token <i> PARSEOP_STARTDEPENDENTFN
%token <i> PARSEOP_STARTDEPENDENTFN_NOPRI
%token <i> PARSEOP_STOPBITS_ONE
%token <i> PARSEOP_STOPBITS_ONEPLUSHALF
%token <i> PARSEOP_STOPBITS_TWO
%token <i> PARSEOP_STOPBITS_ZERO
%token <i> PARSEOP_STORE
%token <s> PARSEOP_STRING_LITERAL
%token <i> PARSEOP_SUBTRACT
%token <i> PARSEOP_SWITCH
%token <i> PARSEOP_THERMALZONE
%token <i> PARSEOP_TIMER
%token <i> PARSEOP_TOBCD
%token <i> PARSEOP_TOBUFFER
%token <i> PARSEOP_TODECIMALSTRING
%token <i> PARSEOP_TOHEXSTRING
%token <i> PARSEOP_TOINTEGER
%token <i> PARSEOP_TOSTRING
%token <i> PARSEOP_TOUUID
%token <i> PARSEOP_TRANSLATIONTYPE_DENSE
%token <i> PARSEOP_TRANSLATIONTYPE_SPARSE
%token <i> PARSEOP_TYPE_STATIC
%token <i> PARSEOP_TYPE_TRANSLATION
%token <i> PARSEOP_UART_SERIALBUS
%token <i> PARSEOP_UART_SERIALBUS_V2
%token <i> PARSEOP_UNICODE
%token <i> PARSEOP_UNLOAD
%token <i> PARSEOP_UPDATERULE_ONES
%token <i> PARSEOP_UPDATERULE_PRESERVE
%token <i> PARSEOP_UPDATERULE_ZEROS
%token <i> PARSEOP_VAR_PACKAGE
%token <i> PARSEOP_VENDORLONG
%token <i> PARSEOP_VENDORSHORT
%token <i> PARSEOP_WAIT
%token <i> PARSEOP_WHILE
%token <i> PARSEOP_WIREMODE_FOUR
%token <i> PARSEOP_WIREMODE_THREE
%token <i> PARSEOP_WORDBUSNUMBER
%token <i> PARSEOP_WORDCONST
%token <i> PARSEOP_WORDIO
%token <i> PARSEOP_WORDSPACE
%token <i> PARSEOP_XFERSIZE_8
%token <i> PARSEOP_XFERSIZE_16
%token <i> PARSEOP_XFERSIZE_32
%token <i> PARSEOP_XFERSIZE_64
%token <i> PARSEOP_XFERSIZE_128
%token <i> PARSEOP_XFERSIZE_256
%token <i> PARSEOP_XFERTYPE_8
%token <i> PARSEOP_XFERTYPE_8_16
%token <i> PARSEOP_XFERTYPE_16
%token <i> PARSEOP_XOR
%token <i> PARSEOP_ZERO

/* ToPld macro */

%token <i> PARSEOP_TOPLD
%token <i> PARSEOP_PLD_REVISION
%token <i> PARSEOP_PLD_IGNORECOLOR
%token <i> PARSEOP_PLD_RED
%token <i> PARSEOP_PLD_GREEN
%token <i> PARSEOP_PLD_BLUE
%token <i> PARSEOP_PLD_WIDTH
%token <i> PARSEOP_PLD_HEIGHT
%token <i> PARSEOP_PLD_USERVISIBLE
%token <i> PARSEOP_PLD_DOCK
%token <i> PARSEOP_PLD_LID
%token <i> PARSEOP_PLD_PANEL
%token <i> PARSEOP_PLD_VERTICALPOSITION
%token <i> PARSEOP_PLD_HORIZONTALPOSITION
%token <i> PARSEOP_PLD_SHAPE
%token <i> PARSEOP_PLD_GROUPORIENTATION
%token <i> PARSEOP_PLD_GROUPTOKEN
%token <i> PARSEOP_PLD_GROUPPOSITION
%token <i> PARSEOP_PLD_BAY
%token <i> PARSEOP_PLD_EJECTABLE
%token <i> PARSEOP_PLD_EJECTREQUIRED
%token <i> PARSEOP_PLD_CABINETNUMBER
%token <i> PARSEOP_PLD_CARDCAGENUMBER
%token <i> PARSEOP_PLD_REFERENCE
%token <i> PARSEOP_PLD_ROTATION
%token <i> PARSEOP_PLD_ORDER
%token <i> PARSEOP_PLD_RESERVED
%token <i> PARSEOP_PLD_VERTICALOFFSET
%token <i> PARSEOP_PLD_HORIZONTALOFFSET

/*
 * C-style expression parser. These must appear after all of the
 * standard ASL operators and keywords.
 *
 * Note: The order of these tokens implements the precedence rules
 * (low precedence to high). See aslrules.y for an exhaustive list.
 */
%right <i> PARSEOP_EXP_EQUALS
           PARSEOP_EXP_ADD_EQ
           PARSEOP_EXP_SUB_EQ
           PARSEOP_EXP_MUL_EQ
           PARSEOP_EXP_DIV_EQ
           PARSEOP_EXP_MOD_EQ
           PARSEOP_EXP_SHL_EQ
           PARSEOP_EXP_SHR_EQ
           PARSEOP_EXP_AND_EQ
           PARSEOP_EXP_XOR_EQ
           PARSEOP_EXP_OR_EQ

%left <i>  PARSEOP_EXP_LOGICAL_OR
%left <i>  PARSEOP_EXP_LOGICAL_AND
%left <i>  PARSEOP_EXP_OR
%left <i>  PARSEOP_EXP_XOR
%left <i>  PARSEOP_EXP_AND
%left <i>  PARSEOP_EXP_EQUAL
           PARSEOP_EXP_NOT_EQUAL
%left <i>  PARSEOP_EXP_GREATER
           PARSEOP_EXP_LESS
           PARSEOP_EXP_GREATER_EQUAL
           PARSEOP_EXP_LESS_EQUAL
%left <i>  PARSEOP_EXP_SHIFT_RIGHT
           PARSEOP_EXP_SHIFT_LEFT
%left <i>  PARSEOP_EXP_ADD
           PARSEOP_EXP_SUBTRACT
%left <i>  PARSEOP_EXP_MULTIPLY
           PARSEOP_EXP_DIVIDE
           PARSEOP_EXP_MODULO

%right <i> PARSEOP_EXP_NOT
           PARSEOP_EXP_LOGICAL_NOT

%left <i>  PARSEOP_EXP_INCREMENT
           PARSEOP_EXP_DECREMENT

%left <i>  PARSEOP_OPEN_PAREN
           PARSEOP_CLOSE_PAREN

/* Brackets for Index() support */

%left <i>  PARSEOP_EXP_INDEX_LEFT
%right <i> PARSEOP_EXP_INDEX_RIGHT

/* Macros */

%token <i> PARSEOP_PRINTF
%token <i> PARSEOP_FPRINTF
%token <i> PARSEOP_FOR

/* Structures */

%token <i> PARSEOP_STRUCTURE
%token <s> PARSEOP_STRUCTURE_NAMESTRING
%token <i> PARSEOP_STRUCTURE_TAG
%token <i> PARSEOP_STRUCTURE_ELEMENT
%token <i> PARSEOP_STRUCTURE_INSTANCE
%token <i> PARSEOP_STRUCTURE_REFERENCE
%token <i> PARSEOP_STRUCTURE_POINTER

/* Top level */

%token <i> PARSEOP_ASL_CODE


/*******************************************************************************
 *
 * Tokens below are not in the aslmap.c file
 *
 ******************************************************************************/


/* Tokens below this are not in the aslmap.c file */

/* Specific parentheses tokens are not used at this time */
           /* PARSEOP_EXP_PAREN_OPEN */
           /* PARSEOP_EXP_PAREN_CLOSE */

/* ASL+ variable creation */

%token <i> PARSEOP_INTEGER_TYPE
%token <i> PARSEOP_STRING_TYPE
%token <i> PARSEOP_BUFFER_TYPE
%token <i> PARSEOP_PACKAGE_TYPE
%token <i> PARSEOP_REFERENCE_TYPE


/*
 * Special functions. These should probably stay at the end of this
 * table.
 */
%token <i> PARSEOP___DATE__
%token <i> PARSEOP___FILE__
%token <i> PARSEOP___LINE__
%token <i> PARSEOP___PATH__
%token <i> PARSEOP___METHOD__


/* Production types/names */



/******************************************************************************
 *
 * Production names
 *
 *****************************************************************************/

%type <n> ArgList
%type <n> AslCode
%type <n> BufferData
%type <n> BufferTermData
%type <n> CompilerDirective
%type <n> DataObject
%type <n> DefinitionBlockTerm
%type <n> DefinitionBlockList
%type <n> IntegerData
%type <n> NamedObject
%type <n> NameSpaceModifier
%type <n> Object
%type <n> PackageData
%type <n> ParameterTypePackage
%type <n> ParameterTypePackageList
%type <n> ParameterTypesPackage
%type <n> ParameterTypesPackageList
%type <n> RequiredTarget
%type <n> SimpleName
%type <n> StringData
%type <n> Target
%type <n> Term
%type <n> TermArg
%type <n> TermList
%type <n> MethodInvocationTerm

/* Type4Opcode is obsolete */

%type <n> Type1Opcode
%type <n> Type2BufferOpcode
%type <n> Type2BufferOrStringOpcode
%type <n> Type2IntegerOpcode
%type <n> Type2Opcode
%type <n> Type2StringOpcode
%type <n> Type3Opcode
%type <n> Type5Opcode
%type <n> Type6Opcode

%type <n> AccessAsTerm
%type <n> ExternalTerm
%type <n> FieldUnit
%type <n> FieldUnitEntry
%type <n> FieldUnitList
%type <n> IncludeTerm
%type <n> OffsetTerm
%type <n> OptionalAccessAttribTerm

/* Named Objects */

%type <n> BankFieldTerm
%type <n> CreateBitFieldTerm
%type <n> CreateByteFieldTerm
%type <n> CreateDWordFieldTerm
%type <n> CreateFieldTerm
%type <n> CreateQWordFieldTerm
%type <n> CreateWordFieldTerm
%type <n> DataRegionTerm
%type <n> DeviceTerm
%type <n> EventTerm
%type <n> FieldTerm
%type <n> FunctionTerm
%type <n> IndexFieldTerm
%type <n> MethodTerm
%type <n> MutexTerm
%type <n> OpRegionTerm
%type <n> OpRegionSpaceIdTerm
%type <n> PowerResTerm
%type <n> ProcessorTerm
%type <n> ThermalZoneTerm

/* Namespace modifiers */

%type <n> AliasTerm
%type <n> NameTerm
%type <n> ScopeTerm

/* Type 1 opcodes */

%type <n> BreakPointTerm
%type <n> BreakTerm
%type <n> CaseDefaultTermList
%type <n> CaseTerm
%type <n> ContinueTerm
%type <n> DefaultTerm
%type <n> ElseTerm
%type <n> FatalTerm
%type <n> ElseIfTerm
%type <n> IfTerm
%type <n> LoadTerm
%type <n> NoOpTerm
%type <n> NotifyTerm
%type <n> ReleaseTerm
%type <n> ResetTerm
%type <n> ReturnTerm
%type <n> SignalTerm
%type <n> SleepTerm
%type <n> StallTerm
%type <n> SwitchTerm
%type <n> UnloadTerm
%type <n> WhileTerm
/* %type <n> CaseTermList */

/* Type 2 opcodes */

%type <n> AcquireTerm
%type <n> AddTerm
%type <n> AndTerm
%type <n> ConcatResTerm
%type <n> ConcatTerm
%type <n> CondRefOfTerm
%type <n> CopyObjectTerm
%type <n> DecTerm
%type <n> DerefOfTerm
%type <n> DivideTerm
%type <n> FindSetLeftBitTerm
%type <n> FindSetRightBitTerm
%type <n> FromBCDTerm
%type <n> IncTerm
%type <n> IndexTerm
%type <n> LAndTerm
%type <n> LEqualTerm
%type <n> LGreaterEqualTerm
%type <n> LGreaterTerm
%type <n> LLessEqualTerm
%type <n> LLessTerm
%type <n> LNotEqualTerm
%type <n> LNotTerm
%type <n> LoadTableTerm
%type <n> LOrTerm
%type <n> MatchTerm
%type <n> MidTerm
%type <n> ModTerm
%type <n> MultiplyTerm
%type <n> NAndTerm
%type <n> NOrTerm
%type <n> NotTerm
%type <n> ObjectTypeTerm
%type <n> OrTerm
%type <n> RawDataBufferTerm
%type <n> RefOfTerm
%type <n> ShiftLeftTerm
%type <n> ShiftRightTerm
%type <n> SizeOfTerm
%type <n> StoreTerm
%type <n> SubtractTerm
%type <n> TimerTerm
%type <n> ToBCDTerm
%type <n> ToBufferTerm
%type <n> ToDecimalStringTerm
%type <n> ToHexStringTerm
%type <n> ToIntegerTerm
%type <n> ToStringTerm
%type <n> WaitTerm
%type <n> XOrTerm

/* Keywords */

%type <n> AccessAttribKeyword
%type <n> AccessTypeKeyword
%type <n> AddressingModeKeyword
%type <n> AddressKeyword
%type <n> AddressSpaceKeyword
%type <n> BitsPerByteKeyword
%type <n> ClockPhaseKeyword
%type <n> ClockPolarityKeyword
%type <n> DecodeKeyword
%type <n> DevicePolarityKeyword
%type <n> DMATypeKeyword
%type <n> EndianKeyword
%type <n> FlowControlKeyword
%type <n> InterruptLevel
%type <n> InterruptTypeKeyword
%type <n> IODecodeKeyword
%type <n> IoRestrictionKeyword
%type <n> LockRuleKeyword
%type <n> MatchOpKeyword
%type <n> MaxKeyword
%type <n> MemTypeKeyword
%type <n> MinKeyword
%type <n> ObjectTypeKeyword
%type <n> OptionalBusMasterKeyword
%type <n> OptionalReadWriteKeyword
%type <n> ParityTypeKeyword
%type <n> PinConfigByte
%type <n> PinConfigKeyword
%type <n> RangeTypeKeyword
%type <n> RegionSpaceKeyword
%type <n> ResourceTypeKeyword
%type <n> SerializeRuleKeyword
%type <n> ShareTypeKeyword
%type <n> SlaveModeKeyword
%type <n> StopBitsKeyword
%type <n> TranslationKeyword
%type <n> TypeKeyword
%type <n> UpdateRuleKeyword
%type <n> WireModeKeyword
%type <n> XferSizeKeyword
%type <n> XferTypeKeyword

/* Types */

%type <n> SuperName
%type <n> ObjectTypeSource
%type <n> DerefOfSource
%type <n> RefOfSource
%type <n> CondRefOfSource
%type <n> ArgTerm
%type <n> LocalTerm
%type <n> DebugTerm

%type <n> Integer
%type <n> ByteConst
%type <n> WordConst
%type <n> DWordConst
%type <n> QWordConst
%type <n> String

%type <n> ConstTerm
%type <n> ConstExprTerm
%type <n> ByteConstExpr
%type <n> WordConstExpr
%type <n> DWordConstExpr
%type <n> QWordConstExpr

%type <n> DWordList
%type <n> BufferTerm
%type <n> ByteList

%type <n> PackageElement
%type <n> PackageList
%type <n> PackageTerm

/* Macros */

%type <n> EISAIDTerm
%type <n> ResourceMacroList
%type <n> ResourceMacroTerm
%type <n> ResourceTemplateTerm
%type <n> PldKeyword
%type <n> PldKeywordList
%type <n> ToPLDTerm
%type <n> ToUUIDTerm
%type <n> UnicodeTerm
%type <n> PrintfArgList
%type <n> PrintfTerm
%type <n> FprintfTerm
%type <n> ForTerm

/* Resource Descriptors */

%type <n> ConnectionTerm
%type <n> DMATerm
%type <n> DWordIOTerm
%type <n> DWordMemoryTerm
%type <n> DWordSpaceTerm
%type <n> EndDependentFnTerm
%type <n> ExtendedIOTerm
%type <n> ExtendedMemoryTerm
%type <n> ExtendedSpaceTerm
%type <n> FixedDmaTerm
%type <n> FixedIOTerm
%type <n> GpioIntTerm
%type <n> GpioIoTerm
%type <n> I2cSerialBusTerm
%type <n> I2cSerialBusTermV2
%type <n> InterruptTerm
%type <n> IOTerm
%type <n> IRQNoFlagsTerm
%type <n> IRQTerm
%type <n> Memory24Term
%type <n> Memory32FixedTerm
%type <n> Memory32Term
%type <n> NameSeg
%type <n> NameString
%type <n> PinConfigTerm
%type <n> PinFunctionTerm
%type <n> PinGroupTerm
%type <n> PinGroupConfigTerm
%type <n> PinGroupFunctionTerm
%type <n> QWordIOTerm
%type <n> QWordMemoryTerm
%type <n> QWordSpaceTerm
%type <n> RegisterTerm
%type <n> SpiSerialBusTerm
%type <n> SpiSerialBusTermV2
%type <n> StartDependentFnNoPriTerm
%type <n> StartDependentFnTerm
%type <n> UartSerialBusTerm
%type <n> UartSerialBusTermV2
%type <n> VendorLongTerm
%type <n> VendorShortTerm
%type <n> WordBusNumberTerm
%type <n> WordIOTerm
%type <n> WordSpaceTerm

/* Local types that help construct the AML, not in ACPI spec */

%type <n> AmlPackageLengthTerm
%type <n> IncludeEndTerm
%type <n> NameStringItem
%type <n> TermArgItem

%type <n> OptionalAccessSize
%type <n> OptionalAccessTypeKeyword
%type <n> OptionalAddressingMode
%type <n> OptionalAddressRange
%type <n> OptionalBitsPerByte
%type <n> OptionalBuffer_Last
%type <n> OptionalByteConstExpr
%type <n> OptionalCount
%type <n> OptionalDataCount
%type <n> OptionalDecodeType
%type <n> OptionalDevicePolarity
%type <n> OptionalDWordConstExpr
%type <n> OptionalEndian
%type <n> OptionalFlowControl
%type <n> OptionalIoRestriction
%type <n> OptionalListString
%type <n> OptionalLockRuleKeyword
%type <n> OptionalMaxType
%type <n> OptionalMemType
%type <n> OptionalMinType
%type <n> OptionalNameString
%type <n> OptionalNameString_First
%type <n> OptionalNameString_Last
%type <n> OptionalObjectTypeKeyword
%type <n> OptionalParameterTypePackage
%type <n> OptionalParameterTypesPackage
%type <n> OptionalParentheses
%type <n> OptionalParityType
%type <n> OptionalPredicate
%type <n> OptionalQWordConstExpr
%type <n> OptionalRangeType
%type <n> OptionalReference
%type <n> OptionalResourceType
%type <n> OptionalResourceType_First
%type <n> OptionalProducerResourceType
%type <n> OptionalReturnArg
%type <n> OptionalSerializeRuleKeyword
%type <n> OptionalShareType
%type <n> OptionalShareType_First
%type <n> OptionalSlaveMode
%type <n> OptionalStopBits
%type <n> OptionalStringData
%type <n> OptionalSyncLevel
%type <n> OptionalTermArg
%type <n> OptionalTranslationType_Last
%type <n> OptionalType
%type <n> OptionalType_Last
%type <n> OptionalUpdateRuleKeyword
%type <n> OptionalWireMode
%type <n> OptionalWordConst
%type <n> OptionalWordConstExpr
%type <n> OptionalXferSize

/*
 * ASL+ (C-style) parser
 */

/* Expressions and symbolic operators */

%type <n> Expression
%type <n> EqualsTerm
%type <n> IndexExpTerm

/* ASL+ Named object declaration support */
/*
%type <n> NameTermAslPlus

%type <n> BufferBegin
%type <n> BufferEnd
%type <n> PackageBegin
%type <n> PackageEnd
%type <n> OptionalLength
*/
/* ASL+ Structure declarations */
/*
%type <n> StructureTerm
%type <n> StructureTermBegin
%type <n> StructureType
%type <n> StructureTag
%type <n> StructureElementList
%type <n> StructureElement
%type <n> StructureElementType
%type <n> OptionalStructureElementType
%type <n> StructureId
*/
/* Structure instantiantion */
/*
%type <n> StructureInstanceTerm
%type <n> StructureTagReference
%type <n> StructureInstanceEnd
*/
/* Pseudo-instantiantion for method Args/Locals */
/*
%type <n> MethodStructureTerm
%type <n> LocalStructureName
*/
/* Direct structure references via the Index operator */
/*
%type <n> StructureReference
%type <n> StructureIndexTerm
%type <n> StructurePointerTerm
%type <n> StructurePointerReference
%type <n> OptionalDefinePointer
*/

%%

/* Production rules */



/*******************************************************************************
 *
 * ASL Root and Secondary Terms
 *
 ******************************************************************************/

/*
 * Root term. Allow multiple #line directives before the definition block
 * to handle output from preprocessors
 */
AslCode
    : DefinitionBlockList           {$<n>$ = TrLinkOpChildren (
                                        TrCreateLeafOp (PARSEOP_ASL_CODE),1, $1);}
    | error                         {YYABORT; $$ = NULL;}
    ;


/*
 * Note concerning support for "module-level code".
 *
 * ACPI 1.0 allowed Type1 and Type2 executable opcodes outside of control
 * methods (the so-called module-level code.) This support was explicitly
 * removed in ACPI 2.0, but this type of code continues to be created by
 * BIOS vendors. In order to support the disassembly and recompilation of
 * such code (and the porting of ASL code to iASL), iASL supports this
 * code in violation of the current ACPI specification.
 *
 * The grammar change to support module-level code is to revert the
 * {ObjectList} portion of the DefinitionBlockTerm in ACPI 2.0 to the
 * original use of {TermList} instead (see below.) This allows the use
 * of Type1 and Type2 opcodes at module level.
 *
 * 04/2016: The module-level code is now allowed in the following terms:
 * DeviceTerm, PowerResTerm, ProcessorTerm, ScopeTerm, ThermalZoneTerm.
 * The ObjectList term is obsolete and has been removed.
 */
DefinitionBlockTerm
    : PARSEOP_DEFINITION_BLOCK
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DEFINITION_BLOCK); COMMENT_CAPTURE_OFF;}
        String ','
        String ','
        ByteConst ','
        String ','
        String ','
        DWordConst
        PARSEOP_CLOSE_PAREN         {TrSetOpIntegerWidth ($6,$8);
                                        TrSetOpEndLineNumber ($<n>3); COMMENT_CAPTURE_ON;}
            '{' TermList '}'        {$$ = TrLinkOpChildren ($<n>3,7,
                                        $4,$6,$8,$10,$12,$14,$18);}
    ;

DefinitionBlockList
    : DefinitionBlockTerm
    | DefinitionBlockTerm
        DefinitionBlockList         {$$ = TrLinkPeerOps (2, $1,$2);}
    ;


/******* Basic ASCII identifiers **************************************************/

/* Allow IO, DMA, IRQ Resource macro and FOR macro names to also be used as identifiers */

NameString
    : NameSeg                       {}
    | PARSEOP_NAMESTRING            {$$ = TrCreateValuedLeafOp (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) $1);}
    | PARSEOP_IO                    {$$ = TrCreateValuedLeafOp (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "IO");}
    | PARSEOP_DMA                   {$$ = TrCreateValuedLeafOp (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "DMA");}
    | PARSEOP_IRQ                   {$$ = TrCreateValuedLeafOp (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "IRQ");}
    | PARSEOP_FOR                   {$$ = TrCreateValuedLeafOp (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "FOR");}
    ;
/*
NameSeg
    : PARSEOP_NAMESEG               {$$ = TrCreateValuedLeafOp (PARSEOP_NAMESEG, (ACPI_NATIVE_INT)
                                        TrNormalizeNameSeg ($1));}
    ;
*/

NameSeg
    : PARSEOP_NAMESEG               {$$ = TrCreateValuedLeafOp (PARSEOP_NAMESEG,
                                        (ACPI_NATIVE_INT) AslCompilerlval.s);}
    ;


/******* Fundamental argument/statement types ***********************************/

Term
    : Object                        {}
    | Type1Opcode                   {}
    | Type2Opcode                   {}
    | Type2IntegerOpcode            {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);}
    | Type2StringOpcode             {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);}
    | Type2BufferOpcode             {}
    | Type2BufferOrStringOpcode     {}
    | error                         {$$ = AslDoError(); yyclearin;}
    ;

SuperName
    : SimpleName                    {}
    | DebugTerm                     {}
    | Type6Opcode                   {}
    ;

Target
    :                               {$$ = TrCreateNullTargetOp ();} /* Placeholder is a ZeroOp object */
    | ','                           {$$ = TrCreateNullTargetOp ();} /* Placeholder is a ZeroOp object */
    | ',' SuperName                 {$$ = TrSetOpFlags ($2, OP_IS_TARGET);}
    ;

RequiredTarget
    : ',' SuperName                 {$$ = TrSetOpFlags ($2, OP_IS_TARGET);}
    ;

TermArg
    : SimpleName                    {$$ = TrSetOpFlags ($1, OP_IS_TERM_ARG);}
    | Type2Opcode                   {$$ = TrSetOpFlags ($1, OP_IS_TERM_ARG);}
    | DataObject                    {$$ = TrSetOpFlags ($1, OP_IS_TERM_ARG);}
    | PARSEOP_OPEN_PAREN
        TermArg
        PARSEOP_CLOSE_PAREN         {$$ = TrSetOpFlags ($2, OP_IS_TERM_ARG);}
    ;

/*
 NOTE: Removed from TermArg due to reduce/reduce conflicts:
    | Type2IntegerOpcode            {$$ = TrSetOpFlags ($1, OP_IS_TERM_ARG);}
    | Type2StringOpcode             {$$ = TrSetOpFlags ($1, OP_IS_TERM_ARG);}
    | Type2BufferOpcode             {$$ = TrSetOpFlags ($1, OP_IS_TERM_ARG);}
    | Type2BufferOrStringOpcode     {$$ = TrSetOpFlags ($1, OP_IS_TERM_ARG);}

*/

MethodInvocationTerm
    : NameString
        PARSEOP_OPEN_PAREN          {TrSetOpIntegerValue (PARSEOP_METHODCALL, $1); COMMENT_CAPTURE_OFF;}
        ArgList
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildOp ($1,$4); COMMENT_CAPTURE_ON;}
    ;

/* OptionalCount must appear before ByteList or an incorrect reduction will result */

OptionalCount
    :                               {$$ = TrCreateLeafOp (PARSEOP_ONES);}       /* Placeholder is a OnesOp object */
    | ','                           {$$ = TrCreateLeafOp (PARSEOP_ONES);}       /* Placeholder is a OnesOp object */
    | ',' TermArg                   {$$ = $2;}
    ;

/*
 * Data count for buffers and packages (byte count for buffers,
 * element count for packages).
 */
OptionalDataCount

        /* Legacy ASL */
    :                               {$$ = NULL;}
    | PARSEOP_OPEN_PAREN
        TermArg
        PARSEOP_CLOSE_PAREN         {$$ = $2;}
    | PARSEOP_OPEN_PAREN
        PARSEOP_CLOSE_PAREN         {$$ = NULL;}

        /* C-style (ASL+) -- adds equals term */

    |  PARSEOP_EXP_EQUALS           {$$ = NULL;}

    | PARSEOP_OPEN_PAREN
        TermArg
        PARSEOP_CLOSE_PAREN
        PARSEOP_EXP_EQUALS          {$$ = $2;}

    | PARSEOP_OPEN_PAREN
        PARSEOP_CLOSE_PAREN
        String
        PARSEOP_EXP_EQUALS          {$$ = NULL;}
    ;


/******* List Terms **************************************************/

    /* ACPI 3.0 -- allow semicolons between terms */

TermList
    :                               {$$ = NULL;}
    | TermList Term                 {$$ = TrLinkPeerOp (
                                        TrSetOpFlags ($1, OP_RESULT_NOT_USED),$2);}
    | TermList Term ';'             {$$ = TrLinkPeerOp (
                                        TrSetOpFlags ($1, OP_RESULT_NOT_USED),$2);}
    | TermList ';' Term             {$$ = TrLinkPeerOp (
                                        TrSetOpFlags ($1, OP_RESULT_NOT_USED),$3);}
    | TermList ';' Term ';'         {$$ = TrLinkPeerOp (
                                        TrSetOpFlags ($1, OP_RESULT_NOT_USED),$3);}
    ;

ArgList
    :                               {$$ = NULL;}
    | TermArg
    | ArgList ','                   /* Allows a trailing comma at list end */
    | ArgList ','
        TermArg                     {$$ = TrLinkPeerOp ($1,$3);}
    ;

ByteList
    :                               {$$ = NULL;}
    | ByteConstExpr
    | ByteList ','                  /* Allows a trailing comma at list end */
    | ByteList ','
        ByteConstExpr               {$$ = TrLinkPeerOp ($1,$3);}
    ;

DWordList
    :                               {$$ = NULL;}
    | DWordConstExpr
    | DWordList ','                 /* Allows a trailing comma at list end */
    | DWordList ','
        DWordConstExpr              {$$ = TrLinkPeerOp ($1,$3);}
    ;

FieldUnitList
    :                               {$$ = NULL;}
    | FieldUnit
    | FieldUnitList ','             /* Allows a trailing comma at list end */
    | FieldUnitList ','
        FieldUnit                   {$$ = TrLinkPeerOp ($1,$3);}
    ;

FieldUnit
    : FieldUnitEntry                {}
    | OffsetTerm                    {}
    | AccessAsTerm                  {}
    | ConnectionTerm                {}
    ;

FieldUnitEntry
    : ',' AmlPackageLengthTerm      {$$ = TrCreateOp (PARSEOP_RESERVED_BYTES,1,$2);}
    | NameSeg ','
        AmlPackageLengthTerm        {$$ = TrLinkChildOp ($1,$3);}
    ;

Object
    : CompilerDirective             {}
    | NamedObject                   {}
    | NameSpaceModifier             {}
/*    | StructureTerm                 {} */
    ;

PackageList
    :                               {$$ = NULL;}
    | PackageElement
    | PackageList ','               /* Allows a trailing comma at list end */
    | PackageList ','
        PackageElement              {$$ = TrLinkPeerOp ($1,$3);}
    ;

PackageElement
    : DataObject                    {}
    | NameString                    {}
    ;

    /* Rules for specifying the type of one method argument or return value */

ParameterTypePackage
    :                               {$$ = NULL;}
    | ObjectTypeKeyword             {$$ = $1;}
    | ParameterTypePackage ','
        ObjectTypeKeyword           {$$ = TrLinkPeerOps (2,$1,$3);}
    ;

ParameterTypePackageList
    :                               {$$ = NULL;}
    | ObjectTypeKeyword             {$$ = $1;}
    | '{' ParameterTypePackage '}'  {$$ = $2;}
    ;

OptionalParameterTypePackage
    :                               {$$ = TrCreateLeafOp (PARSEOP_DEFAULT_ARG);}
    | ',' ParameterTypePackageList  {$$ = TrLinkOpChildren (
                                        TrCreateLeafOp (PARSEOP_DEFAULT_ARG),1,$2);}
    ;

    /* Rules for specifying the types for method arguments */

ParameterTypesPackage
    : ParameterTypePackageList      {$$ = $1;}
    | ParameterTypesPackage ','
        ParameterTypePackageList    {$$ = TrLinkPeerOps (2,$1,$3);}
    ;

ParameterTypesPackageList
    :                               {$$ = NULL;}
    | ObjectTypeKeyword             {$$ = $1;}
    | '{' ParameterTypesPackage '}' {$$ = $2;}
    ;

OptionalParameterTypesPackage
    :                               {$$ = TrCreateLeafOp (PARSEOP_DEFAULT_ARG);}
    | ',' ParameterTypesPackageList {$$ = TrLinkOpChildren (
                                        TrCreateLeafOp (PARSEOP_DEFAULT_ARG),1,$2);}
    ;

/*
 * Case-Default list; allow only one Default term and unlimited Case terms
 */
CaseDefaultTermList
    :                               {$$ = NULL;}
    | CaseTerm                      {}
    | DefaultTerm                   {}
    | CaseDefaultTermList
        CaseTerm                    {$$ = TrLinkPeerOp ($1,$2);}
    | CaseDefaultTermList
        DefaultTerm                 {$$ = TrLinkPeerOp ($1,$2);}

/* Original - attempts to force zero or one default term within the switch */

/*
CaseDefaultTermList
    :                               {$$ = NULL;}
    | CaseTermList
        DefaultTerm
        CaseTermList                {$$ = TrLinkPeerOp ($1,TrLinkPeerOp ($2, $3));}
    | CaseTermList
        CaseTerm                    {$$ = TrLinkPeerOp ($1,$2);}
    ;

CaseTermList
    :                               {$$ = NULL;}
    | CaseTerm                      {}
    | CaseTermList
        CaseTerm                    {$$ = TrLinkPeerOp ($1,$2);}
    ;
*/


/*******************************************************************************
 *
 * ASL Data and Constant Terms
 *
 ******************************************************************************/

DataObject
    : BufferData                    {}
    | PackageData                   {}
    | IntegerData                   {}
    | StringData                    {}
    ;

BufferData
    : Type5Opcode                   {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);}
    | Type2BufferOrStringOpcode     {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);}
    | Type2BufferOpcode             {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);}
    | BufferTerm                    {}
    ;

PackageData
    : PackageTerm                   {}
    ;

IntegerData
    : Type2IntegerOpcode            {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);}
    | Type3Opcode                   {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);}
    | Integer                       {}
    | ConstTerm                     {}
    ;

StringData
    : Type2StringOpcode             {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);}
    | String                        {}
    ;

ByteConst
    : Integer                       {$$ = TrSetOpIntegerValue (PARSEOP_BYTECONST, $1);}
    ;

WordConst
    : Integer                       {$$ = TrSetOpIntegerValue (PARSEOP_WORDCONST, $1);}
    ;

DWordConst
    : Integer                       {$$ = TrSetOpIntegerValue (PARSEOP_DWORDCONST, $1);}
    ;

QWordConst
    : Integer                       {$$ = TrSetOpIntegerValue (PARSEOP_QWORDCONST, $1);}
    ;

/*
 * The OP_COMPILE_TIME_CONST flag in the following constant expressions
 * enables compile-time constant folding to reduce the Type3Opcodes/Type2IntegerOpcodes
 * to simple integers. It is an error if these types of expressions cannot be
 * reduced, since the AML grammar for ****ConstExpr requires a simple constant.
 * Note: The required byte length of the constant is passed through to the
 * constant folding code in the node AmlLength field.
 */
ByteConstExpr
    : Type3Opcode                   {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);
                                        TrSetOpAmlLength ($1, 1);}
    | Type2IntegerOpcode            {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);
                                        TrSetOpAmlLength ($1, 1);}
    | ConstExprTerm                 {$$ = TrSetOpIntegerValue (PARSEOP_BYTECONST, $1);}
    | ByteConst                     {}
    ;

WordConstExpr
    : Type3Opcode                   {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);
                                        TrSetOpAmlLength ($1, 2);}
    | Type2IntegerOpcode            {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);
                                        TrSetOpAmlLength ($1, 2);}
    | ConstExprTerm                 {$$ = TrSetOpIntegerValue (PARSEOP_WORDCONST, $1);}
    | WordConst                     {}
    ;

DWordConstExpr
    : Type3Opcode                   {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);
                                        TrSetOpAmlLength ($1, 4);}
    | Type2IntegerOpcode            {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);
                                        TrSetOpAmlLength ($1, 4);}
    | ConstExprTerm                 {$$ = TrSetOpIntegerValue (PARSEOP_DWORDCONST, $1);}
    | DWordConst                    {}
    ;

QWordConstExpr
    : Type3Opcode                   {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);
                                        TrSetOpAmlLength ($1, 8);}
    | Type2IntegerOpcode            {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);
                                        TrSetOpAmlLength ($1, 8);}
    | ConstExprTerm                 {$$ = TrSetOpIntegerValue (PARSEOP_QWORDCONST, $1);}
    | QWordConst                    {}
    ;

ConstTerm
    : ConstExprTerm                 {}
    | PARSEOP_REVISION              {$$ = TrCreateLeafOp (PARSEOP_REVISION);}
    ;

ConstExprTerm
    : PARSEOP_ZERO                  {$$ = TrCreateValuedLeafOp (PARSEOP_ZERO, 0);}
    | PARSEOP_ONE                   {$$ = TrCreateValuedLeafOp (PARSEOP_ONE, 1);}
    | PARSEOP_ONES                  {$$ = TrCreateValuedLeafOp (PARSEOP_ONES, ACPI_UINT64_MAX);}
    | PARSEOP___DATE__              {$$ = TrCreateConstantLeafOp (PARSEOP___DATE__);}
    | PARSEOP___FILE__              {$$ = TrCreateConstantLeafOp (PARSEOP___FILE__);}
    | PARSEOP___LINE__              {$$ = TrCreateConstantLeafOp (PARSEOP___LINE__);}
    | PARSEOP___PATH__              {$$ = TrCreateConstantLeafOp (PARSEOP___PATH__);}
    | PARSEOP___METHOD__            {$$ = TrCreateConstantLeafOp (PARSEOP___METHOD__);}
    ;

Integer
    : PARSEOP_INTEGER               {$$ = TrCreateValuedLeafOp (PARSEOP_INTEGER,
                                        AslCompilerlval.i);}
    ;

String
    : PARSEOP_STRING_LITERAL        {$$ = TrCreateValuedLeafOp (PARSEOP_STRING_LITERAL,
                                        (ACPI_NATIVE_INT) AslCompilerlval.s);}
    ;


/*******************************************************************************
 *
 * ASL Opcode Terms
 *
 ******************************************************************************/

CompilerDirective
    : IncludeTerm                   {}
    | IncludeEndTerm                {}
    | ExternalTerm                  {}
    ;

NamedObject
    : BankFieldTerm                 {}
    | CreateBitFieldTerm            {}
    | CreateByteFieldTerm           {}
    | CreateDWordFieldTerm          {}
    | CreateFieldTerm               {}
    | CreateQWordFieldTerm          {}
    | CreateWordFieldTerm           {}
    | DataRegionTerm                {}
    | DeviceTerm                    {}
    | EventTerm                     {}
    | FieldTerm                     {}
    | FunctionTerm                  {}
    | IndexFieldTerm                {}
    | MethodTerm                    {}
    | MutexTerm                     {}
    | OpRegionTerm                  {}
    | PowerResTerm                  {}
    | ProcessorTerm                 {}
    | ThermalZoneTerm               {}
    ;

NameSpaceModifier
    : AliasTerm                     {}
    | NameTerm                      {}
/*    | NameTermAslPlus               {} */
    | ScopeTerm                     {}
    ;

SimpleName
    : NameString                    {}
    | LocalTerm                     {}
    | ArgTerm                       {}
    ;

/* For ObjectType(), SuperName except for MethodInvocationTerm */

ObjectTypeSource
    : SimpleName                    {}
    | DebugTerm                     {}
    | RefOfTerm                     {}
    | DerefOfTerm                   {}
    | IndexTerm                     {}
    | IndexExpTerm                  {}
    ;

/* For DeRefOf(), SuperName except for DerefOf and Debug */

DerefOfSource
    : SimpleName                    {}
    | RefOfTerm                     {}
    | DerefOfTerm                   {}
    | IndexTerm                     {}
    | IndexExpTerm                  {}
    | StoreTerm                     {}
    | EqualsTerm                    {}
    | MethodInvocationTerm          {}
    ;

/* For RefOf(), SuperName except for RefOf and MethodInvocationTerm */

RefOfSource
    : SimpleName                    {}
    | DebugTerm                     {}
    | DerefOfTerm                   {}
    | IndexTerm                     {}
    | IndexExpTerm                  {}
    ;

/* For CondRefOf(), SuperName except for RefOf and MethodInvocationTerm */

CondRefOfSource
    : SimpleName                    {}
    | DebugTerm                     {}
    | DerefOfTerm                   {}
    | IndexTerm                     {}
    | IndexExpTerm                  {}
    ;

/*
 * Opcode types, as defined in the ACPI specification
 */
Type1Opcode
    : BreakTerm                     {}
    | BreakPointTerm                {}
    | ContinueTerm                  {}
    | FatalTerm                     {}
    | ForTerm                       {}
    | ElseIfTerm                    {}
    | LoadTerm                      {}
    | NoOpTerm                      {}
    | NotifyTerm                    {}
    | ReleaseTerm                   {}
    | ResetTerm                     {}
    | ReturnTerm                    {}
    | SignalTerm                    {}
    | SleepTerm                     {}
    | StallTerm                     {}
    | SwitchTerm                    {}
    | UnloadTerm                    {}
    | WhileTerm                     {}
    ;

Type2Opcode
    : AcquireTerm                   {}
    | CondRefOfTerm                 {}
    | CopyObjectTerm                {}
    | DerefOfTerm                   {}
    | ObjectTypeTerm                {}
    | RefOfTerm                     {}
    | SizeOfTerm                    {}
    | StoreTerm                     {}
    | EqualsTerm                    {}
    | TimerTerm                     {}
    | WaitTerm                      {}
    | MethodInvocationTerm          {}
    ;

/*
 * Type 3/4/5 opcodes
 */
Type2IntegerOpcode                  /* "Type3" opcodes */
    : Expression                    {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);}
    | AddTerm                       {}
    | AndTerm                       {}
    | DecTerm                       {}
    | DivideTerm                    {}
    | FindSetLeftBitTerm            {}
    | FindSetRightBitTerm           {}
    | FromBCDTerm                   {}
    | IncTerm                       {}
    | IndexTerm                     {}
/*    | StructureIndexTerm            {} */
/*    | StructurePointerTerm          {} */
    | LAndTerm                      {}
    | LEqualTerm                    {}
    | LGreaterTerm                  {}
    | LGreaterEqualTerm             {}
    | LLessTerm                     {}
    | LLessEqualTerm                {}
    | LNotTerm                      {}
    | LNotEqualTerm                 {}
    | LoadTableTerm                 {}
    | LOrTerm                       {}
    | MatchTerm                     {}
    | ModTerm                       {}
    | MultiplyTerm                  {}
    | NAndTerm                      {}
    | NOrTerm                       {}
    | NotTerm                       {}
    | OrTerm                        {}
    | ShiftLeftTerm                 {}
    | ShiftRightTerm                {}
    | SubtractTerm                  {}
    | ToBCDTerm                     {}
    | ToIntegerTerm                 {}
    | XOrTerm                       {}
    ;

Type2StringOpcode                   /* "Type4" Opcodes */
    : ToDecimalStringTerm           {}
    | ToHexStringTerm               {}
    | ToStringTerm                  {}
    ;

Type2BufferOpcode                   /* "Type5" Opcodes */
    : ToBufferTerm                  {}
    | ConcatResTerm                 {}
    ;

Type2BufferOrStringOpcode
    : ConcatTerm                    {$$ = TrSetOpFlags ($1, OP_COMPILE_TIME_CONST);}
    | PrintfTerm                    {}
    | FprintfTerm                   {}
    | MidTerm                       {}
    ;

/*
 * A type 3 opcode evaluates to an Integer and cannot have a destination operand
 */
Type3Opcode
    : EISAIDTerm                    {}
    ;

/* Obsolete
Type4Opcode
    : ConcatTerm                    {}
    | ToDecimalStringTerm           {}
    | ToHexStringTerm               {}
    | MidTerm                       {}
    | ToStringTerm                  {}
    ;
*/

/* Type 5 opcodes are a subset of Type2 opcodes, and return a constant */

Type5Opcode
    : ResourceTemplateTerm          {}
    | UnicodeTerm                   {}
    | ToPLDTerm                     {}
    | ToUUIDTerm                    {}
    ;

Type6Opcode
    : RefOfTerm                     {}
    | DerefOfTerm                   {}
    | IndexTerm                     {}
    | IndexExpTerm                  {}
/*    | StructureIndexTerm            {} */
/*    | StructurePointerTerm          {} */
    | MethodInvocationTerm          {}
    ;


/*******************************************************************************
 *
 * ASL Helper Terms
 *
 ******************************************************************************/

AmlPackageLengthTerm
    : Integer                       {$$ = TrSetOpIntegerValue (PARSEOP_PACKAGE_LENGTH,
                                        (ACPI_PARSE_OBJECT *) $1);}
    ;

NameStringItem
    : ',' NameString                {$$ = $2;}
    | ',' error                     {$$ = AslDoError (); yyclearin;}
    ;

TermArgItem
    : ',' TermArg                   {$$ = $2;}
    | ',' error                     {$$ = AslDoError (); yyclearin;}
    ;

OptionalReference
    :                               {$$ = TrCreateLeafOp (PARSEOP_ZERO);}       /* Placeholder is a ZeroOp object */
    | ','                           {$$ = TrCreateLeafOp (PARSEOP_ZERO);}       /* Placeholder is a ZeroOp object */
    | ',' TermArg                   {$$ = $2;}
    ;

OptionalReturnArg
    :                               {$$ = TrSetOpFlags (TrCreateLeafOp (PARSEOP_ZERO),
                                            OP_IS_NULL_RETURN);}       /* Placeholder is a ZeroOp object */
    | TermArg                       {$$ = $1;}
    ;

OptionalSerializeRuleKeyword
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' SerializeRuleKeyword      {$$ = $2;}
    ;

OptionalTermArg
    :                               {$$ = TrCreateLeafOp (PARSEOP_DEFAULT_ARG);}
    | TermArg                       {$$ = $1;}
    ;

OptionalWordConst
    :                               {$$ = NULL;}
    | WordConst                     {$$ = $1;}
    ;




/*******************************************************************************
 *
 * ASL Primary Terms
 *
 ******************************************************************************/

AccessAsTerm
    : PARSEOP_ACCESSAS
        PARSEOP_OPEN_PAREN
        AccessTypeKeyword
        OptionalAccessAttribTerm
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateOp (PARSEOP_ACCESSAS,2,$3,$4);}
    | PARSEOP_ACCESSAS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

AcquireTerm
    : PARSEOP_ACQUIRE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp(PARSEOP_ACQUIRE);}
        SuperName
        ',' WordConstExpr
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$6);}
    | PARSEOP_ACQUIRE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

AddTerm
    : PARSEOP_ADD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_ADD);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_ADD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

AliasTerm
    : PARSEOP_ALIAS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_ALIAS);}
        NameString
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,
                                        TrSetOpFlags ($5, OP_IS_NAME_DECLARATION));}
    | PARSEOP_ALIAS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

AndTerm
    : PARSEOP_AND
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_AND);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_AND
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ArgTerm
    : PARSEOP_ARG0                  {$$ = TrCreateLeafOp (PARSEOP_ARG0);}
    | PARSEOP_ARG1                  {$$ = TrCreateLeafOp (PARSEOP_ARG1);}
    | PARSEOP_ARG2                  {$$ = TrCreateLeafOp (PARSEOP_ARG2);}
    | PARSEOP_ARG3                  {$$ = TrCreateLeafOp (PARSEOP_ARG3);}
    | PARSEOP_ARG4                  {$$ = TrCreateLeafOp (PARSEOP_ARG4);}
    | PARSEOP_ARG5                  {$$ = TrCreateLeafOp (PARSEOP_ARG5);}
    | PARSEOP_ARG6                  {$$ = TrCreateLeafOp (PARSEOP_ARG6);}
    ;

BankFieldTerm
    : PARSEOP_BANKFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_BANKFIELD);}
        NameString
        NameStringItem
        TermArgItem
        OptionalAccessTypeKeyword
        OptionalLockRuleKeyword
        OptionalUpdateRuleKeyword
        PARSEOP_CLOSE_PAREN '{'
            FieldUnitList '}'       {$$ = TrLinkOpChildren ($<n>3,7,
                                        $4,$5,$6,$7,$8,$9,$12);}
    | PARSEOP_BANKFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN
        '{' error '}'               {$$ = AslDoError(); yyclearin;}
    ;

BreakTerm
    : PARSEOP_BREAK                 {$$ = TrCreateOp (PARSEOP_BREAK, 0);}
    ;

BreakPointTerm
    : PARSEOP_BREAKPOINT            {$$ = TrCreateOp (PARSEOP_BREAKPOINT, 0);}
    ;

BufferTerm
    : PARSEOP_BUFFER                {$<n>$ = TrCreateLeafOp (PARSEOP_BUFFER); COMMENT_CAPTURE_OFF; }
        OptionalDataCount
        '{' BufferTermData '}'      {$$ = TrLinkOpChildren ($<n>2,2,$3,$5); COMMENT_CAPTURE_ON;}
    ;

BufferTermData
    : ByteList                      {}
    | StringData                    {}
    ;

CaseTerm
    : PARSEOP_CASE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_CASE);}
        DataObject
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}
    | PARSEOP_CASE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ConcatTerm
    : PARSEOP_CONCATENATE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_CONCATENATE);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_CONCATENATE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ConcatResTerm
    : PARSEOP_CONCATENATERESTEMPLATE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (
                                        PARSEOP_CONCATENATERESTEMPLATE);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_CONCATENATERESTEMPLATE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CondRefOfTerm
    : PARSEOP_CONDREFOF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_CONDREFOF);}
        CondRefOfSource
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_CONDREFOF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ConnectionTerm
    : PARSEOP_CONNECTION
        PARSEOP_OPEN_PAREN
        NameString
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateOp (PARSEOP_CONNECTION,1,$3);}
    | PARSEOP_CONNECTION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_CONNECTION);}
        ResourceMacroTerm
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3, 1,
                                        TrLinkOpChildren (
                                            TrCreateLeafOp (PARSEOP_RESOURCETEMPLATE), 3,
                                            TrCreateLeafOp (PARSEOP_DEFAULT_ARG),
                                            TrCreateLeafOp (PARSEOP_DEFAULT_ARG),
                                            $4));}
    | PARSEOP_CONNECTION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ContinueTerm
    : PARSEOP_CONTINUE              {$$ = TrCreateOp (PARSEOP_CONTINUE, 0);}
    ;

CopyObjectTerm
    : PARSEOP_COPYOBJECT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_COPYOBJECT);}
        TermArg
        ',' SimpleName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,
                                        TrSetOpFlags ($6, OP_IS_TARGET));}
    | PARSEOP_COPYOBJECT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateBitFieldTerm
    : PARSEOP_CREATEBITFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_CREATEBITFIELD);}
        TermArg
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,
                                        TrSetOpFlags ($6, OP_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEBITFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateByteFieldTerm
    : PARSEOP_CREATEBYTEFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_CREATEBYTEFIELD);}
        TermArg
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,
                                        TrSetOpFlags ($6, OP_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEBYTEFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateDWordFieldTerm
    : PARSEOP_CREATEDWORDFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_CREATEDWORDFIELD);}
        TermArg
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,
                                        TrSetOpFlags ($6, OP_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEDWORDFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateFieldTerm
    : PARSEOP_CREATEFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_CREATEFIELD);}
        TermArg
        TermArgItem
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,4,$4,$5,$6,
                                        TrSetOpFlags ($7, OP_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateQWordFieldTerm
    : PARSEOP_CREATEQWORDFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_CREATEQWORDFIELD);}
        TermArg
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,
                                        TrSetOpFlags ($6, OP_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEQWORDFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateWordFieldTerm
    : PARSEOP_CREATEWORDFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_CREATEWORDFIELD);}
        TermArg
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,
                                        TrSetOpFlags ($6, OP_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEWORDFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DataRegionTerm
    : PARSEOP_DATATABLEREGION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DATATABLEREGION);}
        NameString
        TermArgItem
        TermArgItem
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,4,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),$5,$6,$7);}
    | PARSEOP_DATATABLEREGION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DebugTerm
    : PARSEOP_DEBUG                 {$$ = TrCreateLeafOp (PARSEOP_DEBUG);}
    ;

DecTerm
    : PARSEOP_DECREMENT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DECREMENT);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_DECREMENT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DefaultTerm
    : PARSEOP_DEFAULT '{'           {$<n>$ = TrCreateLeafOp (PARSEOP_DEFAULT);}
        TermList '}'                {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_DEFAULT '{'
        error '}'                   {$$ = AslDoError(); yyclearin;}
    ;

DerefOfTerm
    : PARSEOP_DEREFOF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DEREFOF);}
        DerefOfSource
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_DEREFOF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DeviceTerm
    : PARSEOP_DEVICE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DEVICE);}
        NameString
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkOpChildren ($<n>3,2,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),$7);}
    | PARSEOP_DEVICE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DivideTerm
    : PARSEOP_DIVIDE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DIVIDE);}
        TermArg
        TermArgItem
        Target
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,4,$4,$5,$6,$7);}
    | PARSEOP_DIVIDE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

EISAIDTerm
    : PARSEOP_EISAID
        PARSEOP_OPEN_PAREN
        StringData
        PARSEOP_CLOSE_PAREN         {$$ = TrSetOpIntegerValue (PARSEOP_EISAID, $3);}
    | PARSEOP_EISAID
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ElseIfTerm
    : IfTerm ElseTerm               {$$ = TrLinkPeerOp ($1,$2);}
    ;

ElseTerm
    :                               {$$ = NULL;}
    | PARSEOP_ELSE '{'
        TermList           {$<n>$ = TrCreateLeafOp (PARSEOP_ELSE);}
        '}'                {$$ = TrLinkOpChildren ($<n>4,1,$3);}

    | PARSEOP_ELSE '{'
        error '}'                   {$$ = AslDoError(); yyclearin;}

    | PARSEOP_ELSE
        error                       {$$ = AslDoError(); yyclearin;}

    | PARSEOP_ELSEIF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_ELSE);}
        TermArg                     {$<n>$ = TrCreateLeafOp (PARSEOP_IF);}
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {TrLinkOpChildren ($<n>5,2,$4,$8);}
        ElseTerm                    {TrLinkPeerOp ($<n>5,$11);}
                                    {$$ = TrLinkOpChildren ($<n>3,1,$<n>5);}

    | PARSEOP_ELSEIF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}

    | PARSEOP_ELSEIF
        error                       {$$ = AslDoError(); yyclearin;}
    ;

EventTerm
    : PARSEOP_EVENT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_EVENT);}
        NameString
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION));}
    | PARSEOP_EVENT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ExternalTerm
    : PARSEOP_EXTERNAL
        PARSEOP_OPEN_PAREN
        NameString
        OptionalObjectTypeKeyword
        OptionalParameterTypePackage
        OptionalParameterTypesPackage
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateOp (PARSEOP_EXTERNAL,4,$3,$4,$5,$6);}
    | PARSEOP_EXTERNAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

FatalTerm
    : PARSEOP_FATAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_FATAL);}
        ByteConstExpr
        ',' DWordConstExpr
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$6,$7);}
    | PARSEOP_FATAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

FieldTerm
    : PARSEOP_FIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_FIELD);}
        NameString
        OptionalAccessTypeKeyword
        OptionalLockRuleKeyword
        OptionalUpdateRuleKeyword
        PARSEOP_CLOSE_PAREN '{'
            FieldUnitList '}'       {$$ = TrLinkOpChildren ($<n>3,5,$4,$5,$6,$7,$10);}
    | PARSEOP_FIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN
        '{' error '}'               {$$ = AslDoError(); yyclearin;}
    ;

FindSetLeftBitTerm
    : PARSEOP_FINDSETLEFTBIT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_FINDSETLEFTBIT);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_FINDSETLEFTBIT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

FindSetRightBitTerm
    : PARSEOP_FINDSETRIGHTBIT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_FINDSETRIGHTBIT);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_FINDSETRIGHTBIT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

    /* Convert a For() loop to a While() loop */
ForTerm
    : PARSEOP_FOR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_WHILE);}
        OptionalTermArg ','         {}
        OptionalPredicate ','
        OptionalTermArg             {$<n>$ = TrLinkPeerOp ($4,$<n>3);
                                            TrSetOpParent ($9,$<n>3);}                /* New parent is WHILE */
        PARSEOP_CLOSE_PAREN
        '{' TermList '}'            {$<n>$ = TrLinkOpChildren ($<n>3,2,$7,$13);}
                                    {$<n>$ = TrLinkPeerOp ($13,$9);
                                        $$ = $<n>10;}
    ;

OptionalPredicate
    :                               {$$ = TrCreateValuedLeafOp (PARSEOP_INTEGER, 1);}
    | TermArg                       {$$ = $1;}
    ;

FprintfTerm
    : PARSEOP_FPRINTF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_FPRINTF);}
        TermArg ','
        StringData
        PrintfArgList
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$6,$7);}
    | PARSEOP_FPRINTF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

FromBCDTerm
    : PARSEOP_FROMBCD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_FROMBCD);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_FROMBCD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

FunctionTerm
    : PARSEOP_FUNCTION
        PARSEOP_OPEN_PAREN          {COMMENT_CAPTURE_OFF; $<n>$ = TrCreateLeafOp (PARSEOP_METHOD); }
        NameString
        OptionalParameterTypePackage
        OptionalParameterTypesPackage
        PARSEOP_CLOSE_PAREN '{'     {COMMENT_CAPTURE_ON; }
            TermList '}'            {$$ = TrLinkOpChildren ($<n>3,7,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),
                                        TrCreateValuedLeafOp (PARSEOP_BYTECONST, 0),
                                        TrCreateLeafOp (PARSEOP_SERIALIZERULE_NOTSERIAL),
                                        TrCreateValuedLeafOp (PARSEOP_BYTECONST, 0),$5,$6,$10);}
    | PARSEOP_FUNCTION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IfTerm
    : PARSEOP_IF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_IF);}
        TermArg
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}

    | PARSEOP_IF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IncludeTerm
    : PARSEOP_INCLUDE
        PARSEOP_OPEN_PAREN
        String
        PARSEOP_CLOSE_PAREN         {$$ = TrSetOpIntegerValue (PARSEOP_INCLUDE, $3);
                                        FlOpenIncludeFile ($3);}
    ;

IncludeEndTerm
    : PARSEOP_INCLUDE_END           {$<n>$ = TrCreateLeafOp (PARSEOP_INCLUDE_END);
                                        TrSetOpCurrentFilename ($$);}
    ;

IncTerm
    : PARSEOP_INCREMENT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_INCREMENT);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_INCREMENT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IndexFieldTerm
    : PARSEOP_INDEXFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_INDEXFIELD);}
        NameString
        NameStringItem
        OptionalAccessTypeKeyword
        OptionalLockRuleKeyword
        OptionalUpdateRuleKeyword
        PARSEOP_CLOSE_PAREN '{'
            FieldUnitList '}'       {$$ = TrLinkOpChildren ($<n>3,6,$4,$5,$6,$7,$8,$11);}
    | PARSEOP_INDEXFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN
        '{' error '}'               {$$ = AslDoError(); yyclearin;}
    ;

IndexTerm
    : PARSEOP_INDEX
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_INDEX);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_INDEX
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LAndTerm
    : PARSEOP_LAND
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LAND);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LAND
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LEqualTerm
    : PARSEOP_LEQUAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LEQUAL);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LEQUAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LGreaterEqualTerm
    : PARSEOP_LGREATEREQUAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LLESS);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateOp (PARSEOP_LNOT, 1,
                                        TrLinkOpChildren ($<n>3,2,$4,$5));}
    | PARSEOP_LGREATEREQUAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LGreaterTerm
    : PARSEOP_LGREATER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LGREATER);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LGREATER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LLessEqualTerm
    : PARSEOP_LLESSEQUAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LGREATER);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateOp (PARSEOP_LNOT, 1,
                                        TrLinkOpChildren ($<n>3,2,$4,$5));}
    | PARSEOP_LLESSEQUAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LLessTerm
    : PARSEOP_LLESS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LLESS);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LLESS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LNotEqualTerm
    : PARSEOP_LNOTEQUAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LEQUAL);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateOp (PARSEOP_LNOT, 1,
                                        TrLinkOpChildren ($<n>3,2,$4,$5));}
    | PARSEOP_LNOTEQUAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LNotTerm
    : PARSEOP_LNOT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LNOT);}
        TermArg
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_LNOT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LoadTableTerm
    : PARSEOP_LOADTABLE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LOADTABLE);}
        TermArg
        TermArgItem
        TermArgItem
        OptionalListString
        OptionalListString
        OptionalReference
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,6,$4,$5,$6,$7,$8,$9);}
    | PARSEOP_LOADTABLE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LoadTerm
    : PARSEOP_LOAD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LOAD);}
        NameString
        RequiredTarget
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LOAD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LocalTerm
    : PARSEOP_LOCAL0                {$$ = TrCreateLeafOp (PARSEOP_LOCAL0);}
    | PARSEOP_LOCAL1                {$$ = TrCreateLeafOp (PARSEOP_LOCAL1);}
    | PARSEOP_LOCAL2                {$$ = TrCreateLeafOp (PARSEOP_LOCAL2);}
    | PARSEOP_LOCAL3                {$$ = TrCreateLeafOp (PARSEOP_LOCAL3);}
    | PARSEOP_LOCAL4                {$$ = TrCreateLeafOp (PARSEOP_LOCAL4);}
    | PARSEOP_LOCAL5                {$$ = TrCreateLeafOp (PARSEOP_LOCAL5);}
    | PARSEOP_LOCAL6                {$$ = TrCreateLeafOp (PARSEOP_LOCAL6);}
    | PARSEOP_LOCAL7                {$$ = TrCreateLeafOp (PARSEOP_LOCAL7);}
    ;

LOrTerm
    : PARSEOP_LOR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_LOR);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LOR
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

MatchTerm
    : PARSEOP_MATCH
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MATCH);}
        TermArg
        ',' MatchOpKeyword
        TermArgItem
        ',' MatchOpKeyword
        TermArgItem
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,6,$4,$6,$7,$9,$10,$11);}
    | PARSEOP_MATCH
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

MethodTerm
    : PARSEOP_METHOD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_METHOD); COMMENT_CAPTURE_OFF;}
        NameString
        OptionalByteConstExpr       {UtCheckIntegerRange ($5, 0, 7);}
        OptionalSerializeRuleKeyword
        OptionalByteConstExpr
        OptionalParameterTypePackage
        OptionalParameterTypesPackage
        PARSEOP_CLOSE_PAREN '{'     {COMMENT_CAPTURE_ON;}
            TermList '}'            {$$ = TrLinkOpChildren ($<n>3,7,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),
                                        $5,$7,$8,$9,$10,$14);}
    | PARSEOP_METHOD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

MidTerm
    : PARSEOP_MID
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MID);}
        TermArg
        TermArgItem
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,4,$4,$5,$6,$7);}
    | PARSEOP_MID
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ModTerm
    : PARSEOP_MOD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MOD);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_MOD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

MultiplyTerm
    : PARSEOP_MULTIPLY
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MULTIPLY);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_MULTIPLY
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

MutexTerm
    : PARSEOP_MUTEX
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MUTEX);}
        NameString
        OptionalSyncLevel
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),$5);}
    | PARSEOP_MUTEX
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

NameTerm
    : PARSEOP_NAME
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_NAME);}
        NameString
        ',' DataObject
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),$6);}
    | PARSEOP_NAME
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

NAndTerm
    : PARSEOP_NAND
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_NAND);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_NAND
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

NoOpTerm
    : PARSEOP_NOOP                  {$$ = TrCreateOp (PARSEOP_NOOP, 0);}
    ;

NOrTerm
    : PARSEOP_NOR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_NOR);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_NOR
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

NotifyTerm
    : PARSEOP_NOTIFY
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_NOTIFY);}
        SuperName
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_NOTIFY
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

NotTerm
    : PARSEOP_NOT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_NOT);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_NOT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ObjectTypeTerm
    : PARSEOP_OBJECTTYPE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE);}
        ObjectTypeSource
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_OBJECTTYPE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

OffsetTerm
    : PARSEOP_OFFSET
        PARSEOP_OPEN_PAREN
        AmlPackageLengthTerm
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateOp (PARSEOP_OFFSET,1,$3);}
    | PARSEOP_OFFSET
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

OpRegionTerm
    : PARSEOP_OPERATIONREGION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_OPERATIONREGION);}
        NameString
        ',' OpRegionSpaceIdTerm
        TermArgItem
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,4,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),
                                        $6,$7,$8);}
    | PARSEOP_OPERATIONREGION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

OpRegionSpaceIdTerm
    : RegionSpaceKeyword            {}
    | ByteConst                     {$$ = UtCheckIntegerRange ($1, 0x80, 0xFF);}
    ;

OrTerm
    : PARSEOP_OR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_OR);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_OR
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PackageTerm
    : PARSEOP_PACKAGE               {$<n>$ = TrCreateLeafOp (PARSEOP_VAR_PACKAGE);}
        OptionalDataCount
        '{' PackageList '}'         {$$ = TrLinkOpChildren ($<n>2,2,$3,$5);}

PowerResTerm
    : PARSEOP_POWERRESOURCE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_POWERRESOURCE);}
        NameString
        ',' ByteConstExpr
        ',' WordConstExpr
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkOpChildren ($<n>3,4,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),
                                        $6,$8,$11);}
    | PARSEOP_POWERRESOURCE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PrintfTerm
    : PARSEOP_PRINTF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PRINTF);}
        StringData
        PrintfArgList
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_PRINTF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PrintfArgList
    :                               {$$ = NULL;}
    | TermArg                       {$$ = $1;}
    | PrintfArgList ','
       TermArg                      {$$ = TrLinkPeerOp ($1, $3);}
    ;

ProcessorTerm
    : PARSEOP_PROCESSOR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PROCESSOR);}
        NameString
        ',' ByteConstExpr
        OptionalDWordConstExpr
        OptionalByteConstExpr
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkOpChildren ($<n>3,5,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),
                                        $6,$7,$8,$11);}
    | PARSEOP_PROCESSOR
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

RawDataBufferTerm
    : PARSEOP_DATABUFFER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DATABUFFER);}
        OptionalWordConst
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}
    | PARSEOP_DATABUFFER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

/*
 * In RefOf, the node isn't really a target, but we can't keep track of it after
 * we've taken a pointer to it. (hard to tell if a local becomes initialized this way.)
 */
RefOfTerm
    : PARSEOP_REFOF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_REFOF);}
        RefOfSource
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,
                                        TrSetOpFlags ($4, OP_IS_TARGET));}
    | PARSEOP_REFOF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ReleaseTerm
    : PARSEOP_RELEASE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_RELEASE);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_RELEASE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ResetTerm
    : PARSEOP_RESET
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_RESET);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_RESET
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ReturnTerm
    : PARSEOP_RETURN
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_RETURN);}
        OptionalReturnArg
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_RETURN                {$$ = TrLinkOpChildren (
                                        TrCreateLeafOp (PARSEOP_RETURN),1,
                                        TrSetOpFlags (TrCreateLeafOp (PARSEOP_ZERO),
                                            OP_IS_NULL_RETURN));}
    | PARSEOP_RETURN
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ScopeTerm
    : PARSEOP_SCOPE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SCOPE);}
        NameString
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkOpChildren ($<n>3,2,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),$7);}
    | PARSEOP_SCOPE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ShiftLeftTerm
    : PARSEOP_SHIFTLEFT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SHIFTLEFT);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_SHIFTLEFT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ShiftRightTerm
    : PARSEOP_SHIFTRIGHT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SHIFTRIGHT);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_SHIFTRIGHT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SignalTerm
    : PARSEOP_SIGNAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SIGNAL);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_SIGNAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SizeOfTerm
    : PARSEOP_SIZEOF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SIZEOF);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_SIZEOF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SleepTerm
    : PARSEOP_SLEEP
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SLEEP);}
        TermArg
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_SLEEP
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

StallTerm
    : PARSEOP_STALL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_STALL);}
        TermArg
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_STALL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

StoreTerm
    : PARSEOP_STORE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_STORE);}
        TermArg
        ',' SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,
                                            TrSetOpFlags ($6, OP_IS_TARGET));}
    | PARSEOP_STORE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SubtractTerm
    : PARSEOP_SUBTRACT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SUBTRACT);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_SUBTRACT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SwitchTerm
    : PARSEOP_SWITCH
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SWITCH);}
        TermArg
        PARSEOP_CLOSE_PAREN '{'
            CaseDefaultTermList '}' {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}
    | PARSEOP_SWITCH
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ThermalZoneTerm
    : PARSEOP_THERMALZONE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_THERMALZONE);}
        NameString
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkOpChildren ($<n>3,2,
                                        TrSetOpFlags ($4, OP_IS_NAME_DECLARATION),$7);}
    | PARSEOP_THERMALZONE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

TimerTerm
    : PARSEOP_TIMER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_TIMER);}
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,0);}
    | PARSEOP_TIMER                 {$$ = TrLinkOpChildren (
                                        TrCreateLeafOp (PARSEOP_TIMER),0);}
    | PARSEOP_TIMER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToBCDTerm
    : PARSEOP_TOBCD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_TOBCD);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_TOBCD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToBufferTerm
    : PARSEOP_TOBUFFER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_TOBUFFER);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_TOBUFFER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToDecimalStringTerm
    : PARSEOP_TODECIMALSTRING
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_TODECIMALSTRING);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_TODECIMALSTRING
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToHexStringTerm
    : PARSEOP_TOHEXSTRING
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_TOHEXSTRING);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_TOHEXSTRING
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToIntegerTerm
    : PARSEOP_TOINTEGER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_TOINTEGER);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_TOINTEGER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToPLDTerm
    : PARSEOP_TOPLD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_TOPLD);}
        PldKeywordList
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_TOPLD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PldKeywordList
    :                               {$$ = NULL;}
    | PldKeyword
        PARSEOP_EXP_EQUALS Integer  {$$ = TrLinkOpChildren ($1,1,$3);}
    | PldKeyword
        PARSEOP_EXP_EQUALS String   {$$ = TrLinkOpChildren ($1,1,$3);}
    | PldKeywordList ','            /* Allows a trailing comma at list end */
    | PldKeywordList ','
        PldKeyword
        PARSEOP_EXP_EQUALS Integer  {$$ = TrLinkPeerOp ($1,TrLinkOpChildren ($3,1,$5));}
    | PldKeywordList ','
        PldKeyword
        PARSEOP_EXP_EQUALS String   {$$ = TrLinkPeerOp ($1,TrLinkOpChildren ($3,1,$5));}
    ;


ToStringTerm
    : PARSEOP_TOSTRING
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_TOSTRING);}
        TermArg
        OptionalCount
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_TOSTRING
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToUUIDTerm
    : PARSEOP_TOUUID
        PARSEOP_OPEN_PAREN
        StringData
        PARSEOP_CLOSE_PAREN         {$$ = TrSetOpIntegerValue (PARSEOP_TOUUID, $3);}
    | PARSEOP_TOUUID
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

UnicodeTerm
    : PARSEOP_UNICODE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_UNICODE);}
        StringData
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,0,$4);}
    | PARSEOP_UNICODE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

UnloadTerm
    : PARSEOP_UNLOAD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_UNLOAD);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_UNLOAD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

WaitTerm
    : PARSEOP_WAIT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_WAIT);}
        SuperName
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,2,$4,$5);}
    | PARSEOP_WAIT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

XOrTerm
    : PARSEOP_XOR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_XOR);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_XOR
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

WhileTerm
    : PARSEOP_WHILE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_WHILE);}
        TermArg
        PARSEOP_CLOSE_PAREN
            '{' TermList '}'        {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}
    | PARSEOP_WHILE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;



/*******************************************************************************
 *
 * Production rules for the symbolic (c-style) operators
 *
 ******************************************************************************/

/*
 * ASL Extensions: C-style math/logical operators and expressions.
 * The implementation transforms these operators into the standard
 * AML opcodes and syntax.
 *
 * Supported operators and precedence rules (high-to-low)
 *
 * NOTE: The operator precedence and associativity rules are
 * implemented by the tokens in asltokens.y
 *
 * (left-to-right):
 *  1)      ( ) expr++ expr--
 *
 * (right-to-left):
 *  2)      ! ~
 *
 * (left-to-right):
 *  3)      *   /   %
 *  4)      +   -
 *  5)      >>  <<
 *  6)      <   >   <=  >=
 *  7)      ==  !=
 *  8)      &
 *  9)      ^
 *  10)     |
 *  11)     &&
 *  12)     ||
 *
 * (right-to-left):
 *  13)     = += -= *= /= %= <<= >>= &= ^= |=
 */


/*******************************************************************************
 *
 * Basic operations for math and logical expressions.
 *
 ******************************************************************************/

Expression

    /* Unary operators */

    : PARSEOP_EXP_LOGICAL_NOT           {$<n>$ = TrCreateLeafOp (PARSEOP_LNOT);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>2,1,$3);}
    | PARSEOP_EXP_NOT                   {$<n>$ = TrCreateLeafOp (PARSEOP_NOT);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>2,2,$3,TrCreateNullTargetOp ());}

    | SuperName PARSEOP_EXP_INCREMENT   {$<n>$ = TrCreateLeafOp (PARSEOP_INCREMENT);}
                                        {$$ = TrLinkOpChildren ($<n>3,1,$1);}
    | SuperName PARSEOP_EXP_DECREMENT   {$<n>$ = TrCreateLeafOp (PARSEOP_DECREMENT);}
                                        {$$ = TrLinkOpChildren ($<n>3,1,$1);}

    /* Binary operators: math and logical */

    | TermArg PARSEOP_EXP_ADD           {$<n>$ = TrCreateLeafOp (PARSEOP_ADD);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,TrCreateNullTargetOp ());}
    | TermArg PARSEOP_EXP_DIVIDE        {$<n>$ = TrCreateLeafOp (PARSEOP_DIVIDE);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,4,$1,$4,TrCreateNullTargetOp (),
                                            TrCreateNullTargetOp ());}
    | TermArg PARSEOP_EXP_MODULO        {$<n>$ = TrCreateLeafOp (PARSEOP_MOD);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,TrCreateNullTargetOp ());}
    | TermArg PARSEOP_EXP_MULTIPLY      {$<n>$ = TrCreateLeafOp (PARSEOP_MULTIPLY);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,TrCreateNullTargetOp ());}
    | TermArg PARSEOP_EXP_SHIFT_LEFT    {$<n>$ = TrCreateLeafOp (PARSEOP_SHIFTLEFT);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,TrCreateNullTargetOp ());}
    | TermArg PARSEOP_EXP_SHIFT_RIGHT   {$<n>$ = TrCreateLeafOp (PARSEOP_SHIFTRIGHT);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,TrCreateNullTargetOp ());}
    | TermArg PARSEOP_EXP_SUBTRACT      {$<n>$ = TrCreateLeafOp (PARSEOP_SUBTRACT);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,TrCreateNullTargetOp ());}

    | TermArg PARSEOP_EXP_AND           {$<n>$ = TrCreateLeafOp (PARSEOP_AND);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,TrCreateNullTargetOp ());}
    | TermArg PARSEOP_EXP_OR            {$<n>$ = TrCreateLeafOp (PARSEOP_OR);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,TrCreateNullTargetOp ());}
    | TermArg PARSEOP_EXP_XOR           {$<n>$ = TrCreateLeafOp (PARSEOP_XOR);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,TrCreateNullTargetOp ());}

    | TermArg PARSEOP_EXP_GREATER       {$<n>$ = TrCreateLeafOp (PARSEOP_LGREATER);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,2,$1,$4);}
    | TermArg PARSEOP_EXP_GREATER_EQUAL {$<n>$ = TrCreateLeafOp (PARSEOP_LGREATEREQUAL);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,2,$1,$4);}
    | TermArg PARSEOP_EXP_LESS          {$<n>$ = TrCreateLeafOp (PARSEOP_LLESS);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,2,$1,$4);}
    | TermArg PARSEOP_EXP_LESS_EQUAL    {$<n>$ = TrCreateLeafOp (PARSEOP_LLESSEQUAL);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,2,$1,$4);}

    | TermArg PARSEOP_EXP_EQUAL         {$<n>$ = TrCreateLeafOp (PARSEOP_LEQUAL);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,2,$1,$4);}
    | TermArg PARSEOP_EXP_NOT_EQUAL     {$<n>$ = TrCreateLeafOp (PARSEOP_LNOTEQUAL);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,2,$1,$4);}

    | TermArg PARSEOP_EXP_LOGICAL_AND   {$<n>$ = TrCreateLeafOp (PARSEOP_LAND);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,2,$1,$4);}
    | TermArg PARSEOP_EXP_LOGICAL_OR    {$<n>$ = TrCreateLeafOp (PARSEOP_LOR);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,2,$1,$4);}

    /* Parentheses */

    | PARSEOP_OPEN_PAREN
        Expression
        PARSEOP_CLOSE_PAREN             {$$ = $2;}

    /* Index term -- "= BUF1[5]" on right-hand side of an equals (source) */

    | IndexExpTerm
    ;

    /*
     * Index term -- "BUF1[5] = " or " = BUF1[5] on either the left side
     * of an equals (target) or the right side (source)
     * Currently used in these terms:
     *      Expression
     *      ObjectTypeSource
     *      DerefOfSource
     *      Type6Opcode
     */
IndexExpTerm

    : SuperName
        PARSEOP_EXP_INDEX_LEFT
        TermArg
        PARSEOP_EXP_INDEX_RIGHT         {$$ = TrCreateLeafOp (PARSEOP_INDEX);
                                        TrLinkOpChildren ($$,3,$1,$3,TrCreateNullTargetOp ());}
    ;


/*******************************************************************************
 *
 * All assignment-type operations -- math and logical. Includes simple
 * assignment and compound assignments.
 *
 ******************************************************************************/

EqualsTerm

    /* Allow parens anywhere */

    : PARSEOP_OPEN_PAREN
        EqualsTerm
        PARSEOP_CLOSE_PAREN             {$$ = $2;}

    /* Simple Store() operation */

    | SuperName
        PARSEOP_EXP_EQUALS
        TermArg                         {$$ = TrCreateAssignmentOp ($1, $3);}

    /* Chained equals: (a=RefOf)=b, a=b=c=d etc. */

    | PARSEOP_OPEN_PAREN
        EqualsTerm
        PARSEOP_CLOSE_PAREN
        PARSEOP_EXP_EQUALS
        TermArg                         {$$ = TrCreateAssignmentOp ($2, $5);}

    /* Compound assignments -- Add (operand, operand, target) */

    | TermArg PARSEOP_EXP_ADD_EQ        {$<n>$ = TrCreateLeafOp (PARSEOP_ADD);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,
                                            TrSetOpFlags (TrCreateTargetOp ($1, NULL), OP_IS_TARGET));}

    | TermArg PARSEOP_EXP_DIV_EQ        {$<n>$ = TrCreateLeafOp (PARSEOP_DIVIDE);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,4,$1,$4,TrCreateNullTargetOp (),
                                            TrSetOpFlags (TrCreateTargetOp ($1, NULL), OP_IS_TARGET));}

    | TermArg PARSEOP_EXP_MOD_EQ        {$<n>$ = TrCreateLeafOp (PARSEOP_MOD);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,
                                            TrSetOpFlags (TrCreateTargetOp ($1, NULL), OP_IS_TARGET));}

    | TermArg PARSEOP_EXP_MUL_EQ        {$<n>$ = TrCreateLeafOp (PARSEOP_MULTIPLY);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,
                                            TrSetOpFlags (TrCreateTargetOp ($1, NULL), OP_IS_TARGET));}

    | TermArg PARSEOP_EXP_SHL_EQ        {$<n>$ = TrCreateLeafOp (PARSEOP_SHIFTLEFT);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,
                                            TrSetOpFlags (TrCreateTargetOp ($1, NULL), OP_IS_TARGET));}

    | TermArg PARSEOP_EXP_SHR_EQ        {$<n>$ = TrCreateLeafOp (PARSEOP_SHIFTRIGHT);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,
                                            TrSetOpFlags (TrCreateTargetOp ($1, NULL), OP_IS_TARGET));}

    | TermArg PARSEOP_EXP_SUB_EQ        {$<n>$ = TrCreateLeafOp (PARSEOP_SUBTRACT);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,
                                            TrSetOpFlags (TrCreateTargetOp ($1, NULL), OP_IS_TARGET));}

    | TermArg PARSEOP_EXP_AND_EQ        {$<n>$ = TrCreateLeafOp (PARSEOP_AND);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,
                                            TrSetOpFlags (TrCreateTargetOp ($1, NULL), OP_IS_TARGET));}

    | TermArg PARSEOP_EXP_OR_EQ         {$<n>$ = TrCreateLeafOp (PARSEOP_OR);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,
                                            TrSetOpFlags (TrCreateTargetOp ($1, NULL), OP_IS_TARGET));}

    | TermArg PARSEOP_EXP_XOR_EQ        {$<n>$ = TrCreateLeafOp (PARSEOP_XOR);}
        TermArg                         {$$ = TrLinkOpChildren ($<n>3,3,$1,$4,
                                            TrSetOpFlags (TrCreateTargetOp ($1, NULL), OP_IS_TARGET));}
    ;



/*******************************************************************************
 *
 * ASL Parameter Keyword Terms
 *
 ******************************************************************************/

AccessAttribKeyword
    : PARSEOP_ACCESSATTRIB_BLOCK            {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_BLOCK);}
    | PARSEOP_ACCESSATTRIB_BLOCK_CALL       {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_BLOCK_CALL);}
    | PARSEOP_ACCESSATTRIB_BYTE             {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_BYTE);}
    | PARSEOP_ACCESSATTRIB_QUICK            {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_QUICK );}
    | PARSEOP_ACCESSATTRIB_SND_RCV          {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_SND_RCV);}
    | PARSEOP_ACCESSATTRIB_WORD             {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_WORD);}
    | PARSEOP_ACCESSATTRIB_WORD_CALL        {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_WORD_CALL);}
    | PARSEOP_ACCESSATTRIB_BYTES
        PARSEOP_OPEN_PAREN                  {$<n>$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_BYTES);}
        ByteConst
        PARSEOP_CLOSE_PAREN                 {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_ACCESSATTRIB_RAW_BYTES
        PARSEOP_OPEN_PAREN                  {$<n>$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_RAW_BYTES);}
        ByteConst
        PARSEOP_CLOSE_PAREN                 {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_ACCESSATTRIB_RAW_PROCESS
        PARSEOP_OPEN_PAREN                  {$<n>$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_RAW_PROCESS);}
        ByteConst
        PARSEOP_CLOSE_PAREN                 {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    ;

AccessTypeKeyword
    : PARSEOP_ACCESSTYPE_ANY                {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_ANY);}
    | PARSEOP_ACCESSTYPE_BYTE               {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_BYTE);}
    | PARSEOP_ACCESSTYPE_WORD               {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_WORD);}
    | PARSEOP_ACCESSTYPE_DWORD              {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_DWORD);}
    | PARSEOP_ACCESSTYPE_QWORD              {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_QWORD);}
    | PARSEOP_ACCESSTYPE_BUF                {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_BUF);}
    ;

AddressingModeKeyword
    : PARSEOP_ADDRESSINGMODE_7BIT           {$$ = TrCreateLeafOp (PARSEOP_ADDRESSINGMODE_7BIT);}
    | PARSEOP_ADDRESSINGMODE_10BIT          {$$ = TrCreateLeafOp (PARSEOP_ADDRESSINGMODE_10BIT);}
    ;

AddressKeyword
    : PARSEOP_ADDRESSTYPE_MEMORY            {$$ = TrCreateLeafOp (PARSEOP_ADDRESSTYPE_MEMORY);}
    | PARSEOP_ADDRESSTYPE_RESERVED          {$$ = TrCreateLeafOp (PARSEOP_ADDRESSTYPE_RESERVED);}
    | PARSEOP_ADDRESSTYPE_NVS               {$$ = TrCreateLeafOp (PARSEOP_ADDRESSTYPE_NVS);}
    | PARSEOP_ADDRESSTYPE_ACPI              {$$ = TrCreateLeafOp (PARSEOP_ADDRESSTYPE_ACPI);}
    ;

AddressSpaceKeyword
    : ByteConst                             {$$ = UtCheckIntegerRange ($1, ACPI_NUM_PREDEFINED_REGIONS, 0xFF);}
    | RegionSpaceKeyword                    {}
    ;

BitsPerByteKeyword
    : PARSEOP_BITSPERBYTE_FIVE              {$$ = TrCreateLeafOp (PARSEOP_BITSPERBYTE_FIVE);}
    | PARSEOP_BITSPERBYTE_SIX               {$$ = TrCreateLeafOp (PARSEOP_BITSPERBYTE_SIX);}
    | PARSEOP_BITSPERBYTE_SEVEN             {$$ = TrCreateLeafOp (PARSEOP_BITSPERBYTE_SEVEN);}
    | PARSEOP_BITSPERBYTE_EIGHT             {$$ = TrCreateLeafOp (PARSEOP_BITSPERBYTE_EIGHT);}
    | PARSEOP_BITSPERBYTE_NINE              {$$ = TrCreateLeafOp (PARSEOP_BITSPERBYTE_NINE);}
    ;

ClockPhaseKeyword
    : PARSEOP_CLOCKPHASE_FIRST              {$$ = TrCreateLeafOp (PARSEOP_CLOCKPHASE_FIRST);}
    | PARSEOP_CLOCKPHASE_SECOND             {$$ = TrCreateLeafOp (PARSEOP_CLOCKPHASE_SECOND);}
    ;

ClockPolarityKeyword
    : PARSEOP_CLOCKPOLARITY_LOW             {$$ = TrCreateLeafOp (PARSEOP_CLOCKPOLARITY_LOW);}
    | PARSEOP_CLOCKPOLARITY_HIGH            {$$ = TrCreateLeafOp (PARSEOP_CLOCKPOLARITY_HIGH);}
    ;

DecodeKeyword
    : PARSEOP_DECODETYPE_POS                {$$ = TrCreateLeafOp (PARSEOP_DECODETYPE_POS);}
    | PARSEOP_DECODETYPE_SUB                {$$ = TrCreateLeafOp (PARSEOP_DECODETYPE_SUB);}
    ;

DevicePolarityKeyword
    : PARSEOP_DEVICEPOLARITY_LOW            {$$ = TrCreateLeafOp (PARSEOP_DEVICEPOLARITY_LOW);}
    | PARSEOP_DEVICEPOLARITY_HIGH           {$$ = TrCreateLeafOp (PARSEOP_DEVICEPOLARITY_HIGH);}
    ;

DMATypeKeyword
    : PARSEOP_DMATYPE_A                     {$$ = TrCreateLeafOp (PARSEOP_DMATYPE_A);}
    | PARSEOP_DMATYPE_COMPATIBILITY         {$$ = TrCreateLeafOp (PARSEOP_DMATYPE_COMPATIBILITY);}
    | PARSEOP_DMATYPE_B                     {$$ = TrCreateLeafOp (PARSEOP_DMATYPE_B);}
    | PARSEOP_DMATYPE_F                     {$$ = TrCreateLeafOp (PARSEOP_DMATYPE_F);}
    ;

EndianKeyword
    : PARSEOP_ENDIAN_LITTLE                 {$$ = TrCreateLeafOp (PARSEOP_ENDIAN_LITTLE);}
    | PARSEOP_ENDIAN_BIG                    {$$ = TrCreateLeafOp (PARSEOP_ENDIAN_BIG);}
    ;

FlowControlKeyword
    : PARSEOP_FLOWCONTROL_HW                {$$ = TrCreateLeafOp (PARSEOP_FLOWCONTROL_HW);}
    | PARSEOP_FLOWCONTROL_NONE              {$$ = TrCreateLeafOp (PARSEOP_FLOWCONTROL_NONE);}
    | PARSEOP_FLOWCONTROL_SW                {$$ = TrCreateLeafOp (PARSEOP_FLOWCONTROL_SW);}
    ;

InterruptLevel
    : PARSEOP_INTLEVEL_ACTIVEBOTH           {$$ = TrCreateLeafOp (PARSEOP_INTLEVEL_ACTIVEBOTH);}
    | PARSEOP_INTLEVEL_ACTIVEHIGH           {$$ = TrCreateLeafOp (PARSEOP_INTLEVEL_ACTIVEHIGH);}
    | PARSEOP_INTLEVEL_ACTIVELOW            {$$ = TrCreateLeafOp (PARSEOP_INTLEVEL_ACTIVELOW);}
    ;

InterruptTypeKeyword
    : PARSEOP_INTTYPE_EDGE                  {$$ = TrCreateLeafOp (PARSEOP_INTTYPE_EDGE);}
    | PARSEOP_INTTYPE_LEVEL                 {$$ = TrCreateLeafOp (PARSEOP_INTTYPE_LEVEL);}
    ;

IODecodeKeyword
    : PARSEOP_IODECODETYPE_16               {$$ = TrCreateLeafOp (PARSEOP_IODECODETYPE_16);}
    | PARSEOP_IODECODETYPE_10               {$$ = TrCreateLeafOp (PARSEOP_IODECODETYPE_10);}
    ;

IoRestrictionKeyword
    : PARSEOP_IORESTRICT_IN                 {$$ = TrCreateLeafOp (PARSEOP_IORESTRICT_IN);}
    | PARSEOP_IORESTRICT_OUT                {$$ = TrCreateLeafOp (PARSEOP_IORESTRICT_OUT);}
    | PARSEOP_IORESTRICT_NONE               {$$ = TrCreateLeafOp (PARSEOP_IORESTRICT_NONE);}
    | PARSEOP_IORESTRICT_PRESERVE           {$$ = TrCreateLeafOp (PARSEOP_IORESTRICT_PRESERVE);}
    ;

LockRuleKeyword
    : PARSEOP_LOCKRULE_LOCK                 {$$ = TrCreateLeafOp (PARSEOP_LOCKRULE_LOCK);}
    | PARSEOP_LOCKRULE_NOLOCK               {$$ = TrCreateLeafOp (PARSEOP_LOCKRULE_NOLOCK);}
    ;

MatchOpKeyword
    : PARSEOP_MATCHTYPE_MTR                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MTR);}
    | PARSEOP_MATCHTYPE_MEQ                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MEQ);}
    | PARSEOP_MATCHTYPE_MLE                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MLE);}
    | PARSEOP_MATCHTYPE_MLT                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MLT);}
    | PARSEOP_MATCHTYPE_MGE                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MGE);}
    | PARSEOP_MATCHTYPE_MGT                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MGT);}
    ;

MaxKeyword
    : PARSEOP_MAXTYPE_FIXED                 {$$ = TrCreateLeafOp (PARSEOP_MAXTYPE_FIXED);}
    | PARSEOP_MAXTYPE_NOTFIXED              {$$ = TrCreateLeafOp (PARSEOP_MAXTYPE_NOTFIXED);}
    ;

MemTypeKeyword
    : PARSEOP_MEMTYPE_CACHEABLE             {$$ = TrCreateLeafOp (PARSEOP_MEMTYPE_CACHEABLE);}
    | PARSEOP_MEMTYPE_WRITECOMBINING        {$$ = TrCreateLeafOp (PARSEOP_MEMTYPE_WRITECOMBINING);}
    | PARSEOP_MEMTYPE_PREFETCHABLE          {$$ = TrCreateLeafOp (PARSEOP_MEMTYPE_PREFETCHABLE);}
    | PARSEOP_MEMTYPE_NONCACHEABLE          {$$ = TrCreateLeafOp (PARSEOP_MEMTYPE_NONCACHEABLE);}
    ;

MinKeyword
    : PARSEOP_MINTYPE_FIXED                 {$$ = TrCreateLeafOp (PARSEOP_MINTYPE_FIXED);}
    | PARSEOP_MINTYPE_NOTFIXED              {$$ = TrCreateLeafOp (PARSEOP_MINTYPE_NOTFIXED);}
    ;

ObjectTypeKeyword
    : PARSEOP_OBJECTTYPE_UNK                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_UNK);}
    | PARSEOP_OBJECTTYPE_INT                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_INT);}
    | PARSEOP_OBJECTTYPE_STR                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_STR);}
    | PARSEOP_OBJECTTYPE_BUF                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_BUF);}
    | PARSEOP_OBJECTTYPE_PKG                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_PKG);}
    | PARSEOP_OBJECTTYPE_FLD                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_FLD);}
    | PARSEOP_OBJECTTYPE_DEV                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_DEV);}
    | PARSEOP_OBJECTTYPE_EVT                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_EVT);}
    | PARSEOP_OBJECTTYPE_MTH                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_MTH);}
    | PARSEOP_OBJECTTYPE_MTX                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_MTX);}
    | PARSEOP_OBJECTTYPE_OPR                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_OPR);}
    | PARSEOP_OBJECTTYPE_POW                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_POW);}
    | PARSEOP_OBJECTTYPE_PRO                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_PRO);}
    | PARSEOP_OBJECTTYPE_THZ                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_THZ);}
    | PARSEOP_OBJECTTYPE_BFF                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_BFF);}
    | PARSEOP_OBJECTTYPE_DDB                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_DDB);}
    ;

ParityTypeKeyword
    : PARSEOP_PARITYTYPE_SPACE              {$$ = TrCreateLeafOp (PARSEOP_PARITYTYPE_SPACE);}
    | PARSEOP_PARITYTYPE_MARK               {$$ = TrCreateLeafOp (PARSEOP_PARITYTYPE_MARK);}
    | PARSEOP_PARITYTYPE_ODD                {$$ = TrCreateLeafOp (PARSEOP_PARITYTYPE_ODD);}
    | PARSEOP_PARITYTYPE_EVEN               {$$ = TrCreateLeafOp (PARSEOP_PARITYTYPE_EVEN);}
    | PARSEOP_PARITYTYPE_NONE               {$$ = TrCreateLeafOp (PARSEOP_PARITYTYPE_NONE);}
    ;

PinConfigByte
    : PinConfigKeyword                      {$$ = $1;}
    | ByteConstExpr                         {$$ = UtCheckIntegerRange ($1, 0x80, 0xFF);}
    ;

PinConfigKeyword
    : PARSEOP_PIN_NOPULL                    {$$ = TrCreateLeafOp (PARSEOP_PIN_NOPULL);}
    | PARSEOP_PIN_PULLDOWN                  {$$ = TrCreateLeafOp (PARSEOP_PIN_PULLDOWN);}
    | PARSEOP_PIN_PULLUP                    {$$ = TrCreateLeafOp (PARSEOP_PIN_PULLUP);}
    | PARSEOP_PIN_PULLDEFAULT               {$$ = TrCreateLeafOp (PARSEOP_PIN_PULLDEFAULT);}
    ;

PldKeyword
    : PARSEOP_PLD_REVISION                  {$$ = TrCreateLeafOp (PARSEOP_PLD_REVISION);}
    | PARSEOP_PLD_IGNORECOLOR               {$$ = TrCreateLeafOp (PARSEOP_PLD_IGNORECOLOR);}
    | PARSEOP_PLD_RED                       {$$ = TrCreateLeafOp (PARSEOP_PLD_RED);}
    | PARSEOP_PLD_GREEN                     {$$ = TrCreateLeafOp (PARSEOP_PLD_GREEN);}
    | PARSEOP_PLD_BLUE                      {$$ = TrCreateLeafOp (PARSEOP_PLD_BLUE);}
    | PARSEOP_PLD_WIDTH                     {$$ = TrCreateLeafOp (PARSEOP_PLD_WIDTH);}
    | PARSEOP_PLD_HEIGHT                    {$$ = TrCreateLeafOp (PARSEOP_PLD_HEIGHT);}
    | PARSEOP_PLD_USERVISIBLE               {$$ = TrCreateLeafOp (PARSEOP_PLD_USERVISIBLE);}
    | PARSEOP_PLD_DOCK                      {$$ = TrCreateLeafOp (PARSEOP_PLD_DOCK);}
    | PARSEOP_PLD_LID                       {$$ = TrCreateLeafOp (PARSEOP_PLD_LID);}
    | PARSEOP_PLD_PANEL                     {$$ = TrCreateLeafOp (PARSEOP_PLD_PANEL);}
    | PARSEOP_PLD_VERTICALPOSITION          {$$ = TrCreateLeafOp (PARSEOP_PLD_VERTICALPOSITION);}
    | PARSEOP_PLD_HORIZONTALPOSITION        {$$ = TrCreateLeafOp (PARSEOP_PLD_HORIZONTALPOSITION);}
    | PARSEOP_PLD_SHAPE                     {$$ = TrCreateLeafOp (PARSEOP_PLD_SHAPE);}
    | PARSEOP_PLD_GROUPORIENTATION          {$$ = TrCreateLeafOp (PARSEOP_PLD_GROUPORIENTATION);}
    | PARSEOP_PLD_GROUPTOKEN                {$$ = TrCreateLeafOp (PARSEOP_PLD_GROUPTOKEN);}
    | PARSEOP_PLD_GROUPPOSITION             {$$ = TrCreateLeafOp (PARSEOP_PLD_GROUPPOSITION);}
    | PARSEOP_PLD_BAY                       {$$ = TrCreateLeafOp (PARSEOP_PLD_BAY);}
    | PARSEOP_PLD_EJECTABLE                 {$$ = TrCreateLeafOp (PARSEOP_PLD_EJECTABLE);}
    | PARSEOP_PLD_EJECTREQUIRED             {$$ = TrCreateLeafOp (PARSEOP_PLD_EJECTREQUIRED);}
    | PARSEOP_PLD_CABINETNUMBER             {$$ = TrCreateLeafOp (PARSEOP_PLD_CABINETNUMBER);}
    | PARSEOP_PLD_CARDCAGENUMBER            {$$ = TrCreateLeafOp (PARSEOP_PLD_CARDCAGENUMBER);}
    | PARSEOP_PLD_REFERENCE                 {$$ = TrCreateLeafOp (PARSEOP_PLD_REFERENCE);}
    | PARSEOP_PLD_ROTATION                  {$$ = TrCreateLeafOp (PARSEOP_PLD_ROTATION);}
    | PARSEOP_PLD_ORDER                     {$$ = TrCreateLeafOp (PARSEOP_PLD_ORDER);}
    | PARSEOP_PLD_RESERVED                  {$$ = TrCreateLeafOp (PARSEOP_PLD_RESERVED);}
    | PARSEOP_PLD_VERTICALOFFSET            {$$ = TrCreateLeafOp (PARSEOP_PLD_VERTICALOFFSET);}
    | PARSEOP_PLD_HORIZONTALOFFSET          {$$ = TrCreateLeafOp (PARSEOP_PLD_HORIZONTALOFFSET);}
    ;

RangeTypeKeyword
    : PARSEOP_RANGETYPE_ISAONLY             {$$ = TrCreateLeafOp (PARSEOP_RANGETYPE_ISAONLY);}
    | PARSEOP_RANGETYPE_NONISAONLY          {$$ = TrCreateLeafOp (PARSEOP_RANGETYPE_NONISAONLY);}
    | PARSEOP_RANGETYPE_ENTIRE              {$$ = TrCreateLeafOp (PARSEOP_RANGETYPE_ENTIRE);}
    ;

RegionSpaceKeyword
    : PARSEOP_REGIONSPACE_IO                {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_IO);}
    | PARSEOP_REGIONSPACE_MEM               {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_MEM);}
    | PARSEOP_REGIONSPACE_PCI               {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_PCI);}
    | PARSEOP_REGIONSPACE_EC                {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_EC);}
    | PARSEOP_REGIONSPACE_SMBUS             {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_SMBUS);}
    | PARSEOP_REGIONSPACE_CMOS              {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_CMOS);}
    | PARSEOP_REGIONSPACE_PCIBAR            {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_PCIBAR);}
    | PARSEOP_REGIONSPACE_IPMI              {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_IPMI);}
    | PARSEOP_REGIONSPACE_GPIO              {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_GPIO);}
    | PARSEOP_REGIONSPACE_GSBUS             {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_GSBUS);}
    | PARSEOP_REGIONSPACE_PCC               {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_PCC);}
    | PARSEOP_REGIONSPACE_FFIXEDHW          {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_FFIXEDHW);}
    ;

ResourceTypeKeyword
    : PARSEOP_RESOURCETYPE_CONSUMER         {$$ = TrCreateLeafOp (PARSEOP_RESOURCETYPE_CONSUMER);}
    | PARSEOP_RESOURCETYPE_PRODUCER         {$$ = TrCreateLeafOp (PARSEOP_RESOURCETYPE_PRODUCER);}
    ;

SerializeRuleKeyword
    : PARSEOP_SERIALIZERULE_SERIAL          {$$ = TrCreateLeafOp (PARSEOP_SERIALIZERULE_SERIAL);}
    | PARSEOP_SERIALIZERULE_NOTSERIAL       {$$ = TrCreateLeafOp (PARSEOP_SERIALIZERULE_NOTSERIAL);}
    ;

ShareTypeKeyword
    : PARSEOP_SHARETYPE_SHARED              {$$ = TrCreateLeafOp (PARSEOP_SHARETYPE_SHARED);}
    | PARSEOP_SHARETYPE_EXCLUSIVE           {$$ = TrCreateLeafOp (PARSEOP_SHARETYPE_EXCLUSIVE);}
    | PARSEOP_SHARETYPE_SHAREDWAKE          {$$ = TrCreateLeafOp (PARSEOP_SHARETYPE_SHAREDWAKE);}
    | PARSEOP_SHARETYPE_EXCLUSIVEWAKE       {$$ = TrCreateLeafOp (PARSEOP_SHARETYPE_EXCLUSIVEWAKE);}
   ;

SlaveModeKeyword
    : PARSEOP_SLAVEMODE_CONTROLLERINIT      {$$ = TrCreateLeafOp (PARSEOP_SLAVEMODE_CONTROLLERINIT);}
    | PARSEOP_SLAVEMODE_DEVICEINIT          {$$ = TrCreateLeafOp (PARSEOP_SLAVEMODE_DEVICEINIT);}
    ;

StopBitsKeyword
    : PARSEOP_STOPBITS_TWO                  {$$ = TrCreateLeafOp (PARSEOP_STOPBITS_TWO);}
    | PARSEOP_STOPBITS_ONEPLUSHALF          {$$ = TrCreateLeafOp (PARSEOP_STOPBITS_ONEPLUSHALF);}
    | PARSEOP_STOPBITS_ONE                  {$$ = TrCreateLeafOp (PARSEOP_STOPBITS_ONE);}
    | PARSEOP_STOPBITS_ZERO                 {$$ = TrCreateLeafOp (PARSEOP_STOPBITS_ZERO);}
    ;

TranslationKeyword
    : PARSEOP_TRANSLATIONTYPE_SPARSE        {$$ = TrCreateLeafOp (PARSEOP_TRANSLATIONTYPE_SPARSE);}
    | PARSEOP_TRANSLATIONTYPE_DENSE         {$$ = TrCreateLeafOp (PARSEOP_TRANSLATIONTYPE_DENSE);}
    ;

TypeKeyword
    : PARSEOP_TYPE_TRANSLATION              {$$ = TrCreateLeafOp (PARSEOP_TYPE_TRANSLATION);}
    | PARSEOP_TYPE_STATIC                   {$$ = TrCreateLeafOp (PARSEOP_TYPE_STATIC);}
    ;

UpdateRuleKeyword
    : PARSEOP_UPDATERULE_PRESERVE           {$$ = TrCreateLeafOp (PARSEOP_UPDATERULE_PRESERVE);}
    | PARSEOP_UPDATERULE_ONES               {$$ = TrCreateLeafOp (PARSEOP_UPDATERULE_ONES);}
    | PARSEOP_UPDATERULE_ZEROS              {$$ = TrCreateLeafOp (PARSEOP_UPDATERULE_ZEROS);}
    ;

WireModeKeyword
    : PARSEOP_WIREMODE_FOUR                 {$$ = TrCreateLeafOp (PARSEOP_WIREMODE_FOUR);}
    | PARSEOP_WIREMODE_THREE                {$$ = TrCreateLeafOp (PARSEOP_WIREMODE_THREE);}
    ;

XferSizeKeyword
    : PARSEOP_XFERSIZE_8                    {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_8,   0);}
    | PARSEOP_XFERSIZE_16                   {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_16,  1);}
    | PARSEOP_XFERSIZE_32                   {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_32,  2);}
    | PARSEOP_XFERSIZE_64                   {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_64,  3);}
    | PARSEOP_XFERSIZE_128                  {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_128, 4);}
    | PARSEOP_XFERSIZE_256                  {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_256, 5);}
    ;

XferTypeKeyword
    : PARSEOP_XFERTYPE_8                    {$$ = TrCreateLeafOp (PARSEOP_XFERTYPE_8);}
    | PARSEOP_XFERTYPE_8_16                 {$$ = TrCreateLeafOp (PARSEOP_XFERTYPE_8_16);}
    | PARSEOP_XFERTYPE_16                   {$$ = TrCreateLeafOp (PARSEOP_XFERTYPE_16);}
    ;




/*******************************************************************************
 *
 * ASL Resource Template Terms
 *
 ******************************************************************************/

/*
 * Note: Create two default nodes to allow conversion to a Buffer AML opcode
 * Also, insert the EndTag at the end of the template.
 */
ResourceTemplateTerm
    : PARSEOP_RESOURCETEMPLATE      {COMMENT_CAPTURE_OFF;}
        OptionalParentheses
        '{'
        ResourceMacroList '}'       {$$ = TrCreateOp (PARSEOP_RESOURCETEMPLATE,4,
                                          TrCreateLeafOp (PARSEOP_DEFAULT_ARG),
                                          TrCreateLeafOp (PARSEOP_DEFAULT_ARG),
                                          $5,
                                          TrCreateLeafOp (PARSEOP_ENDTAG));
                                     COMMENT_CAPTURE_ON;}
    ;

OptionalParentheses
    :                               {$$ = NULL;}
    | PARSEOP_OPEN_PAREN
        PARSEOP_CLOSE_PAREN         {$$ = NULL;}
    ;

ResourceMacroList
    :                               {$$ = NULL;}
    | ResourceMacroList
        ResourceMacroTerm           {$$ = TrLinkPeerOp ($1,$2);}
    ;

ResourceMacroTerm
    : DMATerm                       {}
    | DWordIOTerm                   {}
    | DWordMemoryTerm               {}
    | DWordSpaceTerm                {}
    | EndDependentFnTerm            {}
    | ExtendedIOTerm                {}
    | ExtendedMemoryTerm            {}
    | ExtendedSpaceTerm             {}
    | FixedDmaTerm                  {}
    | FixedIOTerm                   {}
    | GpioIntTerm                   {}
    | GpioIoTerm                    {}
    | I2cSerialBusTerm              {}
    | I2cSerialBusTermV2            {}
    | InterruptTerm                 {}
    | IOTerm                        {}
    | IRQNoFlagsTerm                {}
    | IRQTerm                       {}
    | Memory24Term                  {}
    | Memory32FixedTerm             {}
    | Memory32Term                  {}
    | PinConfigTerm                 {}
    | PinFunctionTerm               {}
    | PinGroupTerm                  {}
    | PinGroupConfigTerm            {}
    | PinGroupFunctionTerm          {}
    | QWordIOTerm                   {}
    | QWordMemoryTerm               {}
    | QWordSpaceTerm                {}
    | RegisterTerm                  {}
    | SpiSerialBusTerm              {}
    | SpiSerialBusTermV2            {}
    | StartDependentFnNoPriTerm     {}
    | StartDependentFnTerm          {}
    | UartSerialBusTerm             {}
    | UartSerialBusTermV2           {}
    | VendorLongTerm                {}
    | VendorShortTerm               {}
    | WordBusNumberTerm             {}
    | WordIOTerm                    {}
    | WordSpaceTerm                 {}
    ;

DMATerm
    : PARSEOP_DMA
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DMA);}
        DMATypeKeyword
        OptionalBusMasterKeyword
        ',' XferTypeKeyword
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,5,$4,$5,$7,$8,$11);}
    | PARSEOP_DMA
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DWordIOTerm
    : PARSEOP_DWORDIO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DWORDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22,$23);}
    | PARSEOP_DWORDIO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DWordMemoryTerm
    : PARSEOP_DWORDMEMORY
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DWORDMEMORY);}
        OptionalResourceType_First
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        OptionalMemType
        ',' OptionalReadWriteKeyword
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalAddressRange
        OptionalType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,16,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$20,$21,$22,$23,$24,$25);}
    | PARSEOP_DWORDMEMORY
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DWordSpaceTerm
    : PARSEOP_DWORDSPACE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DWORDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23,$24);}
    | PARSEOP_DWORDSPACE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

EndDependentFnTerm
    : PARSEOP_ENDDEPENDENTFN
        PARSEOP_OPEN_PAREN
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateLeafOp (PARSEOP_ENDDEPENDENTFN);}
    | PARSEOP_ENDDEPENDENTFN
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ExtendedIOTerm
    : PARSEOP_EXTENDEDIO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_EXTENDEDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalQWordConstExpr
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22);}
    | PARSEOP_EXTENDEDIO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ExtendedMemoryTerm
    : PARSEOP_EXTENDEDMEMORY
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_EXTENDEDMEMORY);}
        OptionalResourceType_First
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        OptionalMemType
        ',' OptionalReadWriteKeyword
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalQWordConstExpr
        OptionalNameString
        OptionalAddressRange
        OptionalType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$20,$21,$22,$23,$24);}
    | PARSEOP_EXTENDEDMEMORY
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ExtendedSpaceTerm
    : PARSEOP_EXTENDEDSPACE PARSEOP_OPEN_PAREN     {$<n>$ = TrCreateLeafOp (PARSEOP_EXTENDEDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalQWordConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN                         {$$ = TrLinkOpChildren ($<n>3,13,
                                        $4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23);}
    | PARSEOP_EXTENDEDSPACE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN                   {$$ = AslDoError(); yyclearin;}
    ;

FixedDmaTerm
    : PARSEOP_FIXEDDMA
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_FIXEDDMA);}
        WordConstExpr               /* 04: DMA RequestLines */
        ',' WordConstExpr           /* 06: DMA Channels */
        OptionalXferSize            /* 07: DMA TransferSize */
        OptionalNameString          /* 08: DescriptorName */
        PARSEOP_CLOSE_PAREN                         {$$ = TrLinkOpChildren ($<n>3,4,$4,$6,$7,$8);}
    | PARSEOP_FIXEDDMA
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN                   {$$ = AslDoError(); yyclearin;}
    ;

FixedIOTerm
    : PARSEOP_FIXEDIO
        PARSEOP_OPEN_PAREN           {$<n>$ = TrCreateLeafOp (PARSEOP_FIXEDIO);}
        WordConstExpr
        ',' ByteConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN                         {$$ = TrLinkOpChildren ($<n>3,3,$4,$6,$7);}
    | PARSEOP_FIXEDIO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN                   {$$ = AslDoError(); yyclearin;}
    ;

GpioIntTerm
    : PARSEOP_GPIO_INT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_GPIO_INT);}
        InterruptTypeKeyword        /* 04: InterruptType */
        ',' InterruptLevel          /* 06: InterruptLevel */
        OptionalShareType           /* 07: SharedType */
        ',' PinConfigByte           /* 09: PinConfig */
        OptionalWordConstExpr       /* 10: DebounceTimeout */
        ',' StringData              /* 12: ResourceSource */
        OptionalByteConstExpr       /* 13: ResourceSourceIndex */
        OptionalResourceType        /* 14: ResourceType */
        OptionalNameString          /* 15: DescriptorName */
        OptionalBuffer_Last         /* 16: VendorData */
        PARSEOP_CLOSE_PAREN '{'
            DWordConstExpr '}'      {$$ = TrLinkOpChildren ($<n>3,11,
                                        $4,$6,$7,$9,$10,$12,$13,$14,$15,$16,$19);}
    | PARSEOP_GPIO_INT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

GpioIoTerm
    : PARSEOP_GPIO_IO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_GPIO_IO);}
        OptionalShareType_First     /* 04: SharedType */
        ',' PinConfigByte           /* 06: PinConfig */
        OptionalWordConstExpr       /* 07: DebounceTimeout */
        OptionalWordConstExpr       /* 08: DriveStrength */
        OptionalIoRestriction       /* 09: IoRestriction */
        ',' StringData              /* 11: ResourceSource */
        OptionalByteConstExpr       /* 12: ResourceSourceIndex */
        OptionalResourceType        /* 13: ResourceType */
        OptionalNameString          /* 14: DescriptorName */
        OptionalBuffer_Last         /* 15: VendorData */
        PARSEOP_CLOSE_PAREN '{'
            DWordList '}'           {$$ = TrLinkOpChildren ($<n>3,11,
                                        $4,$6,$7,$8,$9,$11,$12,$13,$14,$15,$18);}
    | PARSEOP_GPIO_IO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN                   {$$ = AslDoError(); yyclearin;}
    ;

I2cSerialBusTerm
    : PARSEOP_I2C_SERIALBUS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_I2C_SERIALBUS);}
        WordConstExpr               /* 04: SlaveAddress */
        OptionalSlaveMode           /* 05: SlaveMode */
        ',' DWordConstExpr          /* 07: ConnectionSpeed */
        OptionalAddressingMode      /* 08: AddressingMode */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalBuffer_Last         /* 14: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,10,
                                        $4,$5,$7,$8,$10,$11,$12,$13,
                                        TrCreateLeafOp (PARSEOP_DEFAULT_ARG),$14);}
    | PARSEOP_I2C_SERIALBUS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

I2cSerialBusTermV2
    : PARSEOP_I2C_SERIALBUS_V2
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_I2C_SERIALBUS_V2);}
        WordConstExpr               /* 04: SlaveAddress */
        OptionalSlaveMode           /* 05: SlaveMode */
        ',' DWordConstExpr          /* 07: ConnectionSpeed */
        OptionalAddressingMode      /* 08: AddressingMode */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalShareType           /* 14: Share */
        OptionalBuffer_Last         /* 15: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,10,
                                        $4,$5,$7,$8,$10,$11,$12,$13,$14,$15);}
    | PARSEOP_I2C_SERIALBUS_V2
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

InterruptTerm
    : PARSEOP_INTERRUPT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_INTERRUPT);}
        OptionalResourceType_First
        ',' InterruptTypeKeyword
        ',' InterruptLevel
        OptionalShareType
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN '{'
            DWordList '}'           {$$ = TrLinkOpChildren ($<n>3,8,
                                        $4,$6,$8,$9,$10,$11,$12,$15);}
    | PARSEOP_INTERRUPT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IOTerm
    : PARSEOP_IO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_IO);}
        IODecodeKeyword
        ',' WordConstExpr
        ',' WordConstExpr
        ',' ByteConstExpr
        ',' ByteConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,6,$4,$6,$8,$10,$12,$13);}
    | PARSEOP_IO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IRQNoFlagsTerm
    : PARSEOP_IRQNOFLAGS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_IRQNOFLAGS);}
        OptionalNameString_First
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}
    | PARSEOP_IRQNOFLAGS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IRQTerm
    : PARSEOP_IRQ
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_IRQ);}
        InterruptTypeKeyword
        ',' InterruptLevel
        OptionalShareType
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,5,$4,$6,$7,$8,$11);}
    | PARSEOP_IRQ
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

Memory24Term
    : PARSEOP_MEMORY24
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MEMORY24);}
        OptionalReadWriteKeyword
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,6,$4,$6,$8,$10,$12,$13);}
    | PARSEOP_MEMORY24
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

Memory32FixedTerm
    : PARSEOP_MEMORY32FIXED
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MEMORY32FIXED);}
        OptionalReadWriteKeyword
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,4,$4,$6,$8,$9);}
    | PARSEOP_MEMORY32FIXED
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

Memory32Term
    : PARSEOP_MEMORY32
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MEMORY32);}
        OptionalReadWriteKeyword
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,6,$4,$6,$8,$10,$12,$13);}
    | PARSEOP_MEMORY32
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PinConfigTerm
    : PARSEOP_PINCONFIG
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PINCONFIG);}
        OptionalShareType_First     /* 04: SharedType */
        ',' ByteConstExpr           /* 06: PinConfigType */
        ',' DWordConstExpr          /* 08: PinConfigValue */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalBuffer_Last         /* 14: VendorData */
        PARSEOP_CLOSE_PAREN '{'
            DWordList '}'           {$$ = TrLinkOpChildren ($<n>3,9,
                                        $4,$6,$8,$10,$11,$12,$13,$14,$17);}
    | PARSEOP_PINCONFIG
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PinFunctionTerm
    : PARSEOP_PINFUNCTION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PINFUNCTION);}
        OptionalShareType_First     /* 04: SharedType */
        ',' PinConfigByte           /* 06: PinConfig */
        ',' WordConstExpr           /* 08: FunctionNumber */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalBuffer_Last         /* 14: VendorData */
        PARSEOP_CLOSE_PAREN '{'
            DWordList '}'           {$$ = TrLinkOpChildren ($<n>3,9,
                                        $4,$6,$8,$10,$11,$12,$13,$14,$17);}
    | PARSEOP_PINFUNCTION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PinGroupTerm
    : PARSEOP_PINGROUP
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PINGROUP);}
        StringData                  /* 04: ResourceLabel */
        OptionalProducerResourceType /* 05: ResourceType */
        OptionalNameString          /* 06: DescriptorName */
        OptionalBuffer_Last         /* 07: VendorData */
        PARSEOP_CLOSE_PAREN '{'
            DWordList '}'           {$$ = TrLinkOpChildren ($<n>3,5,$4,$5,$6,$7,$10);}
    | PARSEOP_PINGROUP
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PinGroupConfigTerm
    : PARSEOP_PINGROUPCONFIG
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PINGROUPCONFIG);}
        OptionalShareType_First     /* 04: SharedType */
        ',' ByteConstExpr           /* 06: PinConfigType */
        ',' DWordConstExpr          /* 08: PinConfigValue */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        ',' StringData              /* 13: ResourceSourceLabel */
        OptionalResourceType        /* 14: ResourceType */
        OptionalNameString          /* 15: DescriptorName */
        OptionalBuffer_Last         /* 16: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,9,
                                        $4,$6,$8,$10,$11,$13,$14,$15,$16);}
    | PARSEOP_PINGROUPCONFIG
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PinGroupFunctionTerm
    : PARSEOP_PINGROUPFUNCTION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PINGROUPFUNCTION);}
        OptionalShareType_First     /* 04: SharedType */
        ',' WordConstExpr           /* 06: FunctionNumber */
        ',' StringData              /* 08: ResourceSource */
        OptionalByteConstExpr       /* 09: ResourceSourceIndex */
        ',' StringData              /* 11: ResourceSourceLabel */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalBuffer_Last         /* 14: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,8,
                                        $4,$6,$8,$9,$11,$12,$13,$14);}
    | PARSEOP_PINGROUPFUNCTION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

QWordIOTerm
    : PARSEOP_QWORDIO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_QWORDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22,$23);}
    | PARSEOP_QWORDIO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

QWordMemoryTerm
    : PARSEOP_QWORDMEMORY
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_QWORDMEMORY);}
        OptionalResourceType_First
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        OptionalMemType
        ',' OptionalReadWriteKeyword
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalAddressRange
        OptionalType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,16,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$20,$21,$22,$23,$24,$25);}
    | PARSEOP_QWORDMEMORY
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

QWordSpaceTerm
    : PARSEOP_QWORDSPACE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_QWORDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23,$24);}
    | PARSEOP_QWORDSPACE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

RegisterTerm
    : PARSEOP_REGISTER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_REGISTER);}
        AddressSpaceKeyword
        ',' ByteConstExpr
        ',' ByteConstExpr
        ',' QWordConstExpr
        OptionalAccessSize
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,6,$4,$6,$8,$10,$11,$12);}
    | PARSEOP_REGISTER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SpiSerialBusTerm
    : PARSEOP_SPI_SERIALBUS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SPI_SERIALBUS);}
        WordConstExpr               /* 04: DeviceSelection */
        OptionalDevicePolarity      /* 05: DevicePolarity */
        OptionalWireMode            /* 06: WireMode */
        ',' ByteConstExpr           /* 08: DataBitLength */
        OptionalSlaveMode           /* 09: SlaveMode */
        ',' DWordConstExpr          /* 11: ConnectionSpeed */
        ',' ClockPolarityKeyword    /* 13: ClockPolarity */
        ',' ClockPhaseKeyword       /* 15: ClockPhase */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalBuffer_Last         /* 21: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$5,$6,$8,$9,$11,$13,$15,$17,$18,$19,$20,
                                        TrCreateLeafOp (PARSEOP_DEFAULT_ARG),$21);}
    | PARSEOP_SPI_SERIALBUS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SpiSerialBusTermV2
    : PARSEOP_SPI_SERIALBUS_V2
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SPI_SERIALBUS_V2);}
        WordConstExpr               /* 04: DeviceSelection */
        OptionalDevicePolarity      /* 05: DevicePolarity */
        OptionalWireMode            /* 06: WireMode */
        ',' ByteConstExpr           /* 08: DataBitLength */
        OptionalSlaveMode           /* 09: SlaveMode */
        ',' DWordConstExpr          /* 11: ConnectionSpeed */
        ',' ClockPolarityKeyword    /* 13: ClockPolarity */
        ',' ClockPhaseKeyword       /* 15: ClockPhase */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalShareType           /* 21: Share */
        OptionalBuffer_Last         /* 22: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$5,$6,$8,$9,$11,$13,$15,$17,$18,$19,$20,$21,$22);}
    | PARSEOP_SPI_SERIALBUS_V2
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

StartDependentFnNoPriTerm
    : PARSEOP_STARTDEPENDENTFN_NOPRI
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_STARTDEPENDENTFN_NOPRI);}
        PARSEOP_CLOSE_PAREN '{'
        ResourceMacroList '}'       {$$ = TrLinkOpChildren ($<n>3,1,$6);}
    | PARSEOP_STARTDEPENDENTFN_NOPRI
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

StartDependentFnTerm
    : PARSEOP_STARTDEPENDENTFN
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_STARTDEPENDENTFN);}
        ByteConstExpr
        ',' ByteConstExpr
        PARSEOP_CLOSE_PAREN '{'
        ResourceMacroList '}'       {$$ = TrLinkOpChildren ($<n>3,3,$4,$6,$9);}
    | PARSEOP_STARTDEPENDENTFN
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

UartSerialBusTerm
    : PARSEOP_UART_SERIALBUS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_UART_SERIALBUS);}
        DWordConstExpr              /* 04: ConnectionSpeed */
        OptionalBitsPerByte         /* 05: BitsPerByte */
        OptionalStopBits            /* 06: StopBits */
        ',' ByteConstExpr           /* 08: LinesInUse */
        OptionalEndian              /* 09: Endianness */
        OptionalParityType          /* 10: Parity */
        OptionalFlowControl         /* 11: FlowControl */
        ',' WordConstExpr           /* 13: Rx BufferSize */
        ',' WordConstExpr           /* 15: Tx BufferSize */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalBuffer_Last         /* 21: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$8,$9,$10,$11,$13,$15,$17,$18,$19,$20,
                                        TrCreateLeafOp (PARSEOP_DEFAULT_ARG),$21);}
    | PARSEOP_UART_SERIALBUS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

UartSerialBusTermV2
    : PARSEOP_UART_SERIALBUS_V2
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_UART_SERIALBUS_V2);}
        DWordConstExpr              /* 04: ConnectionSpeed */
        OptionalBitsPerByte         /* 05: BitsPerByte */
        OptionalStopBits            /* 06: StopBits */
        ',' ByteConstExpr           /* 08: LinesInUse */
        OptionalEndian              /* 09: Endianness */
        OptionalParityType          /* 10: Parity */
        OptionalFlowControl         /* 11: FlowControl */
        ',' WordConstExpr           /* 13: Rx BufferSize */
        ',' WordConstExpr           /* 15: Tx BufferSize */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalShareType           /* 21: Share */
        OptionalBuffer_Last         /* 22: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$8,$9,$10,$11,$13,$15,$17,$18,$19,$20,$21,$22);}
    | PARSEOP_UART_SERIALBUS_V2
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

VendorLongTerm
    : PARSEOP_VENDORLONG
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_VENDORLONG);}
        OptionalNameString_First
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}
    | PARSEOP_VENDORLONG
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

VendorShortTerm
    : PARSEOP_VENDORSHORT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_VENDORSHORT);}
        OptionalNameString_First
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}
    | PARSEOP_VENDORSHORT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

WordBusNumberTerm
    : PARSEOP_WORDBUSNUMBER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_WORDBUSNUMBER);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,12,
                                        $4,$5,$6,$7,$9,$11,$13,$15,$17,$18,$19,$20);}
    | PARSEOP_WORDBUSNUMBER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

WordIOTerm
    : PARSEOP_WORDIO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_WORDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22,$23);}
    | PARSEOP_WORDIO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

WordSpaceTerm
    : PARSEOP_WORDSPACE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_WORDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23,$24);}
    | PARSEOP_WORDSPACE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;




/*******************************************************************************
 *
 * ASL Helper Terms
 *
 ******************************************************************************/

OptionalBusMasterKeyword
    : ','                                   {$$ = TrCreateLeafOp (
                                                PARSEOP_BUSMASTERTYPE_MASTER);}
    | ',' PARSEOP_BUSMASTERTYPE_MASTER      {$$ = TrCreateLeafOp (
                                                PARSEOP_BUSMASTERTYPE_MASTER);}
    | ',' PARSEOP_BUSMASTERTYPE_NOTMASTER   {$$ = TrCreateLeafOp (
                                                PARSEOP_BUSMASTERTYPE_NOTMASTER);}
    ;

OptionalAccessAttribTerm
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ByteConstExpr             {$$ = $2;}
    | ',' AccessAttribKeyword       {$$ = $2;}
    ;

OptionalAccessSize
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalAccessTypeKeyword   /* Default: AnyAcc */
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_ACCESSTYPE_ANY);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_ACCESSTYPE_ANY);}
    | ',' AccessTypeKeyword         {$$ = $2;}
    ;

OptionalAddressingMode
    : ','                           {$$ = NULL;}
    | ',' AddressingModeKeyword     {$$ = $2;}
    ;

OptionalAddressRange
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' AddressKeyword            {$$ = $2;}
    ;

OptionalBitsPerByte
    : ','                           {$$ = NULL;}
    | ',' BitsPerByteKeyword        {$$ = $2;}
    ;

OptionalBuffer_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' RawDataBufferTerm         {$$ = $2;}
    ;

OptionalByteConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalDecodeType
    : ','                           {$$ = NULL;}
    | ',' DecodeKeyword             {$$ = $2;}
    ;

OptionalDevicePolarity
    : ','                           {$$ = NULL;}
    | ',' DevicePolarityKeyword     {$$ = $2;}
    ;

OptionalDWordConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' DWordConstExpr            {$$ = $2;}
    ;

OptionalEndian
    : ','                           {$$ = NULL;}
    | ',' EndianKeyword             {$$ = $2;}
    ;

OptionalFlowControl
    : ','                           {$$ = NULL;}
    | ',' FlowControlKeyword        {$$ = $2;}
    ;

OptionalIoRestriction
    : ','                           {$$ = NULL;}
    | ',' IoRestrictionKeyword      {$$ = $2;}
    ;

OptionalListString
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_STRING_LITERAL,
                                        ACPI_TO_INTEGER (""));}   /* Placeholder is a NULL string */
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_STRING_LITERAL,
                                        ACPI_TO_INTEGER (""));}   /* Placeholder is a NULL string */
    | ',' TermArg                   {$$ = $2;}
    ;

OptionalLockRuleKeyword     /* Default: NoLock */
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_LOCKRULE_NOLOCK);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_LOCKRULE_NOLOCK);}
    | ',' LockRuleKeyword           {$$ = $2;}
    ;

OptionalMaxType
    : ','                           {$$ = NULL;}
    | ',' MaxKeyword                {$$ = $2;}
    ;

OptionalMemType
    : ','                           {$$ = NULL;}
    | ',' MemTypeKeyword            {$$ = $2;}
    ;

OptionalMinType
    : ','                           {$$ = NULL;}
    | ',' MinKeyword                {$$ = $2;}
    ;

OptionalNameString
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' NameString                {$$ = $2;}
    ;

OptionalNameString_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' NameString                {$$ = $2;}
    ;

OptionalNameString_First
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_ZERO);}
    | NameString                    {$$ = $1;}
    ;

OptionalObjectTypeKeyword
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_OBJECTTYPE_UNK);}
    | ',' ObjectTypeKeyword         {$$ = $2;}
    ;

OptionalParityType
    : ','                           {$$ = NULL;}
    | ',' ParityTypeKeyword         {$$ = $2;}
    ;

OptionalQWordConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' QWordConstExpr            {$$ = $2;}
    ;

OptionalRangeType
    : ','                           {$$ = NULL;}
    | ',' RangeTypeKeyword          {$$ = $2;}
    ;

OptionalReadWriteKeyword
    :                                   {$$ = TrCreateLeafOp (
                                            PARSEOP_READWRITETYPE_BOTH);}
    | PARSEOP_READWRITETYPE_BOTH        {$$ = TrCreateLeafOp (
                                            PARSEOP_READWRITETYPE_BOTH);}
    | PARSEOP_READWRITETYPE_READONLY    {$$ = TrCreateLeafOp (
                                            PARSEOP_READWRITETYPE_READONLY);}
    ;

OptionalResourceType_First
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_CONSUMER);}
    | ResourceTypeKeyword           {$$ = $1;}
    ;

OptionalResourceType
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_CONSUMER);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_CONSUMER);}
    | ',' ResourceTypeKeyword       {$$ = $2;}
    ;

/* Same as above except default is producer */
OptionalProducerResourceType
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_PRODUCER);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_PRODUCER);}
    | ',' ResourceTypeKeyword       {$$ = $2;}
    ;

OptionalSlaveMode
    : ','                           {$$ = NULL;}
    | ',' SlaveModeKeyword          {$$ = $2;}
    ;

OptionalShareType
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ShareTypeKeyword          {$$ = $2;}
    ;

OptionalShareType_First
    :                               {$$ = NULL;}
    | ShareTypeKeyword              {$$ = $1;}
    ;

OptionalStopBits
    : ','                           {$$ = NULL;}
    | ',' StopBitsKeyword           {$$ = $2;}
    ;

OptionalStringData
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' StringData                {$$ = $2;}
    ;

OptionalSyncLevel           /* Default: 0 */
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalTranslationType_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TranslationKeyword        {$$ = $2;}
    ;

OptionalType
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TypeKeyword               {$$ = $2;}
    ;

OptionalType_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TypeKeyword               {$$ = $2;}
    ;

OptionalUpdateRuleKeyword   /* Default: Preserve */
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_UPDATERULE_PRESERVE);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_UPDATERULE_PRESERVE);}
    | ',' UpdateRuleKeyword         {$$ = $2;}
    ;

OptionalWireMode
    : ','                           {$$ = NULL;}
    | ',' WireModeKeyword           {$$ = $2;}
    ;

OptionalWordConstExpr
    : ','                           {$$ = NULL;}
    | ',' WordConstExpr             {$$ = $2;}
    ;

OptionalXferSize
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_XFERSIZE_32, 2);}
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_XFERSIZE_32, 2);}
    | ',' XferSizeKeyword           {$$ = $2;}
    ;

%%

/*! [End] no source code translation !*/

/* Local support functions in C */


/******************************************************************************
 *
 * Local support functions
 *
 *****************************************************************************/

/*! [Begin] no source code translation */
int
AslCompilerwrap(void)
{
  return (1);
}
/*! [End] no source code translation !*/


void *
AslLocalAllocate (
    unsigned int        Size)
{
    void                *Mem;


    DbgPrint (ASL_PARSE_OUTPUT,
        "\nAslLocalAllocate: Expanding Stack to %u\n\n", Size);

    Mem = ACPI_ALLOCATE_ZEROED (Size);
    if (!Mem)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_MEMORY_ALLOCATION,
            AslGbl_CurrentLineNumber, AslGbl_LogicalLineNumber,
            AslGbl_InputByteCount, AslGbl_CurrentColumn,
            AslGbl_Files[ASL_FILE_INPUT].Filename, NULL);
        exit (1);
    }

    return (Mem);
}

ACPI_PARSE_OBJECT *
AslDoError (
    void)
{

    return (TrCreateLeafOp (PARSEOP_ERRORNODE));
}


/*******************************************************************************
 *
 * FUNCTION:    UtGetOpName
 *
 * PARAMETERS:  ParseOpcode         - Parser keyword ID
 *
 * RETURN:      Pointer to the opcode name
 *
 * DESCRIPTION: Get the ascii name of the parse opcode
 *
 ******************************************************************************/

char *
UtGetOpName (
    UINT32                  ParseOpcode)
{
#ifdef ASL_YYTNAME_START
    /*
     * First entries (ASL_YYTNAME_START) in yytname are special reserved names.
     * Ignore first 8 characters of the name
     */
    return ((char *) yytname
        [(ParseOpcode - ASL_FIRST_PARSE_OPCODE) + ASL_YYTNAME_START] + 8);
#else
    return ("[Unknown parser generator]");
#endif
}
