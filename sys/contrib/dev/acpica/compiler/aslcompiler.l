
%{
/******************************************************************************
 *
 * Module Name: aslcompiler.l - Flex input file
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
 * All rights reserved.
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
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"

#include <stdlib.h>
#include <string.h>
YYSTYPE AslCompilerlval;

/*
 * Generation:  Use the following command line:
 *
 * flex.exe -PAslCompiler -i -o$(InputPath).c $(InputPath)
 *
 * -i: Scanner must be case-insensitive
 */

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslscan")
char
comment (void);
char
comment2 (void);
void
count (int type);
char
literal (void);
void
copy (void);

/*! [Begin] no source code translation */

%}


LeadNameChar                [A-Za-z_]
DigitChar                   [0-9]
HexDigitChar                [A-Fa-f0-9]
RootChar                    [\\]
Nothing                     []

NameChar                    [A-Za-z_0-9]
NameSeg1                    {LeadNameChar}{NameChar}
NameSeg2                    {LeadNameChar}{NameChar}{NameChar}
NameSeg3                    {LeadNameChar}{NameChar}{NameChar}{NameChar}
NameSeg                     {LeadNameChar}|{NameSeg1}|{NameSeg2}|{NameSeg3}

NameString                  {RootChar}|{RootChar}{NamePath}|[\^]+{NamePath}|{NonEmptyNamePath}
NamePath                    {NonEmptyNamePath}?
NonEmptyNamePath            {NameSeg}{NamePathTail}*
NamePathTail                [.]{NameSeg}

%%

[ ]                         { count (0); }
[\n]                        { count (0); } /* Handle files with both LF and CR/LF */
[\r]                        { count (0); } /* termination on both Unix and Windows */
[ \t]                       { count (0); }


"/*"                        { if (!comment ()) yyterminate (); }
"//"                        { if (!comment2 ()) yyterminate (); }

"\""                        { if (literal ()) return (PARSEOP_STRING_LITERAL); else yyterminate (); }


0[xX]{HexDigitChar}+ |
{DigitChar}+                { AslCompilerlval.i = UtDoConstant ((char *) AslCompilertext);
                                count (1); return (PARSEOP_INTEGER); }

"Include"                   { count (1); return (PARSEOP_INCLUDE); }
"#include"                  { count (1); return (PARSEOP_INCLUDE_CSTYLE); }
"#line"						{ count (1); return (PARSEOP_LINE_CSTYLE); }
"External"                  { count (1); return (PARSEOP_EXTERNAL); }


"Ones"                      { count (1); return (PARSEOP_ONES); }
"One"                       { count (1); return (PARSEOP_ONE); }
"Zero"                      { count (1); return (PARSEOP_ZERO); }
"Revision"                  { count (1); return (PARSEOP_REVISION); }

"Offset"                    { count (1); return (PARSEOP_OFFSET); }
"AccessAs"                  { count (1); return (PARSEOP_ACCESSAS); }
"BankField"                 { count (2); return (PARSEOP_BANKFIELD); }
"CreateBitField"            { count (2); return (PARSEOP_CREATEBITFIELD); }
"CreateByteField"           { count (2); return (PARSEOP_CREATEBYTEFIELD); }
"CreateDWordField"          { count (2); return (PARSEOP_CREATEDWORDFIELD); }
"CreateField"               { count (2); return (PARSEOP_CREATEFIELD); }
"CreateQWordField"          { count (2); return (PARSEOP_CREATEQWORDFIELD); }
"CreateWordField"           { count (2); return (PARSEOP_CREATEWORDFIELD); }
"DataTableRegion"           { count (2); return (PARSEOP_DATATABLEREGION); }
"Device"                    { count (2); return (PARSEOP_DEVICE); }
"Event"                     { count (2); return (PARSEOP_EVENT); }
"Field"                     { count (2); return (PARSEOP_FIELD); }
"Function"                  { count (2); return (PARSEOP_FUNCTION); }
"IndexField"                { count (2); return (PARSEOP_INDEXFIELD); }
"Method"                    { count (2); return (PARSEOP_METHOD); }
"Mutex"                     { count (2); return (PARSEOP_MUTEX); }
"OperationRegion"           { count (2); return (PARSEOP_OPERATIONREGION); }
"PowerResource"             { count (2); return (PARSEOP_POWERRESOURCE); }
"Processor"                 { count (2); return (PARSEOP_PROCESSOR); }
"ThermalZone"               { count (2); return (PARSEOP_THERMALZONE); }
"Alias"                     { count (2); return (PARSEOP_ALIAS); }
"Name"                      { count (2); return (PARSEOP_NAME); }
"Scope"                     { count (2); return (PARSEOP_SCOPE); }
"Break"                     { count (3); return (PARSEOP_BREAK); }
"BreakPoint"                { count (3); return (PARSEOP_BREAKPOINT); }
"Continue"                  { count (3); return (PARSEOP_CONTINUE); }
"Fatal"                     { count (3); return (PARSEOP_FATAL); }
"If"                        { count (3); return (PARSEOP_IF); }
"Else"                      { count (3); return (PARSEOP_ELSE); }
"ElseIf"                    { count (3); return (PARSEOP_ELSEIF); }
"Load"                      { count (3); return (PARSEOP_LOAD); }
"Noop"                      { count (3); return (PARSEOP_NOOP); }
"Notify"                    { count (3); return (PARSEOP_NOTIFY); }
"Release"                   { count (3); return (PARSEOP_RELEASE); }
"Reset"                     { count (3); return (PARSEOP_RESET); }
"Return"                    { count (3); return (PARSEOP_RETURN); }
"Signal"                    { count (3); return (PARSEOP_SIGNAL); }
"Sleep"                     { count (3); return (PARSEOP_SLEEP); }
"Stall"                     { count (3); return (PARSEOP_STALL); }
"Switch"                    { count (3); return (PARSEOP_SWITCH); }
"Case"                      { count (3); return (PARSEOP_CASE); }
"Default"                   { count (3); return (PARSEOP_DEFAULT); }
"Unload"                    { count (3); return (PARSEOP_UNLOAD); }
"While"                     { count (3); return (PARSEOP_WHILE); }

"Acquire"                   { count (3); return (PARSEOP_ACQUIRE); }
"Add"                       { count (3); return (PARSEOP_ADD); }
"And"                       { count (3); return (PARSEOP_AND); }
"Concatenate"               { count (3); return (PARSEOP_CONCATENATE); }
"ConcatenateResTemplate"    { count (3); return (PARSEOP_CONCATENATERESTEMPLATE); }
"CondRefOf"                 { count (3); return (PARSEOP_CONDREFOF); }
"CopyObject"                { count (3); return (PARSEOP_COPYOBJECT); }
"Decrement"                 { count (3); return (PARSEOP_DECREMENT); }
"DeRefOf"                   { count (3); return (PARSEOP_DEREFOF); }
"Divide"                    { count (3); return (PARSEOP_DIVIDE); }
"FindSetLeftBit"            { count (3); return (PARSEOP_FINDSETLEFTBIT); }
"FindSetRightBit"           { count (3); return (PARSEOP_FINDSETRIGHTBIT); }
"FromBCD"                   { count (3); return (PARSEOP_FROMBCD); }
"Increment"                 { count (3); return (PARSEOP_INCREMENT); }
"Index"                     { count (3); return (PARSEOP_INDEX); }
"LAnd"                      { count (3); return (PARSEOP_LAND); }
"LEqual"                    { count (3); return (PARSEOP_LEQUAL); }
"LGreater"                  { count (3); return (PARSEOP_LGREATER); }
"LGreaterEqual"             { count (3); return (PARSEOP_LGREATEREQUAL); }
"LLess"                     { count (3); return (PARSEOP_LLESS); }
"LLessEqual"                { count (3); return (PARSEOP_LLESSEQUAL); }
"LNot"                      { count (3); return (PARSEOP_LNOT); }
"LNotEqual"                 { count (3); return (PARSEOP_LNOTEQUAL); }
"LoadTable"                 { count (3); return (PARSEOP_LOADTABLE); }
"LOr"                       { count (3); return (PARSEOP_LOR); }
"Match"                     { count (3); return (PARSEOP_MATCH); }
"Mid"                       { count (3); return (PARSEOP_MID); }
"Mod"                       { count (3); return (PARSEOP_MOD); }
"Multiply"                  { count (3); return (PARSEOP_MULTIPLY); }
"NAnd"                      { count (3); return (PARSEOP_NAND); }
"NOr"                       { count (3); return (PARSEOP_NOR); }
"Not"                       { count (3); return (PARSEOP_NOT); }
"ObjectType"                { count (3); return (PARSEOP_OBJECTTYPE); }
"Or"                        { count (3); return (PARSEOP_OR); }
"RefOf"                     { count (3); return (PARSEOP_REFOF); }
"ShiftLeft"                 { count (3); return (PARSEOP_SHIFTLEFT); }
"ShiftRight"                { count (3); return (PARSEOP_SHIFTRIGHT); }
"SizeOf"                    { count (3); return (PARSEOP_SIZEOF); }
"Store"                     { count (3); return (PARSEOP_STORE); }
"Subtract"                  { count (3); return (PARSEOP_SUBTRACT); }
"Timer"                     { count (3); return (PARSEOP_TIMER); }
"ToBCD"                     { count (3); return (PARSEOP_TOBCD); }
"ToBuffer"                  { count (3); return (PARSEOP_TOBUFFER); }
"ToDecimalString"           { count (3); return (PARSEOP_TODECIMALSTRING); }
"ToHexString"               { count (3); return (PARSEOP_TOHEXSTRING); }
"ToInteger"                 { count (3); return (PARSEOP_TOINTEGER); }
"ToString"                  { count (3); return (PARSEOP_TOSTRING); }
"Wait"                      { count (3); return (PARSEOP_WAIT); }
"XOr"                       { count (3); return (PARSEOP_XOR); }

"Arg0"                      { count (1); return (PARSEOP_ARG0); }
"Arg1"                      { count (1); return (PARSEOP_ARG1); }
"Arg2"                      { count (1); return (PARSEOP_ARG2); }
"Arg3"                      { count (1); return (PARSEOP_ARG3); }
"Arg4"                      { count (1); return (PARSEOP_ARG4); }
"Arg5"                      { count (1); return (PARSEOP_ARG5); }
"Arg6"                      { count (1); return (PARSEOP_ARG6); }

"Local0"                    { count (1); return (PARSEOP_LOCAL0); }
"Local1"                    { count (1); return (PARSEOP_LOCAL1); }
"Local2"                    { count (1); return (PARSEOP_LOCAL2); }
"Local3"                    { count (1); return (PARSEOP_LOCAL3); }
"Local4"                    { count (1); return (PARSEOP_LOCAL4); }
"Local5"                    { count (1); return (PARSEOP_LOCAL5); }
"Local6"                    { count (1); return (PARSEOP_LOCAL6); }
"Local7"                    { count (1); return (PARSEOP_LOCAL7); }

"Debug"                     { count (1); return (PARSEOP_DEBUG); }

"DefinitionBlock"           { count (1); return (PARSEOP_DEFINITIONBLOCK); }
"Buffer"                    { count (1); return (PARSEOP_BUFFER); }
"Package"                   { count (1); return (PARSEOP_PACKAGE); }

"EISAID"                    { count (1); return (PARSEOP_EISAID); }
"ResourceTemplate"          { count (1); return (PARSEOP_RESOURCETEMPLATE); }
"ToUUID"                    { count (1); return (PARSEOP_TOUUID); }
"Unicode"                   { count (1); return (PARSEOP_UNICODE); }
"DMA"                       { count (1); return (PARSEOP_DMA); }
"DWordIO"                   { count (1); return (PARSEOP_DWORDIO); }
"DWordMemory"               { count (1); return (PARSEOP_DWORDMEMORY); }
"DWordSpace"                { count (1); return (PARSEOP_DWORDSPACE); }
"EndDependentFn"            { count (1); return (PARSEOP_ENDDEPENDENTFN); }
"ExtendedIO"                { count (1); return (PARSEOP_EXTENDEDIO); }
"ExtendedMemory"            { count (1); return (PARSEOP_EXTENDEDMEMORY); }
"ExtendedSpace"             { count (1); return (PARSEOP_EXTENDEDSPACE); }
"FixedIO"                   { count (1); return (PARSEOP_FIXEDIO); }
"Interrupt"                 { count (1); return (PARSEOP_INTERRUPT); }
"IO"                        { count (1); return (PARSEOP_IO); }
"IRQNoFlags"                { count (1); return (PARSEOP_IRQNOFLAGS); }
"IRQ"                       { count (1); return (PARSEOP_IRQ); }
"Memory24"                  { count (1); return (PARSEOP_MEMORY24); }
"Memory32Fixed"             { count (1); return (PARSEOP_MEMORY32FIXED); }
"Memory32"                  { count (1); return (PARSEOP_MEMORY32); }
"QWordIO"                   { count (1); return (PARSEOP_QWORDIO); }
"QWordMemory"               { count (1); return (PARSEOP_QWORDMEMORY); }
"QWordSpace"                { count (1); return (PARSEOP_QWORDSPACE); }
"Register"                  { count (1); return (PARSEOP_REGISTER); }
"StartDependentFn"          { count (1); return (PARSEOP_STARTDEPENDENTFN); }
"StartDependentFnNoPri"     { count (1); return (PARSEOP_STARTDEPENDENTFN_NOPRI); }
"VendorLong"                { count (1); return (PARSEOP_VENDORLONG); }
"VendorShort"               { count (1); return (PARSEOP_VENDORSHORT); }
"WordBusNumber"             { count (1); return (PARSEOP_WORDBUSNUMBER); }
"WordIO"                    { count (1); return (PARSEOP_WORDIO); }
"WordSpace"                 { count (1); return (PARSEOP_WORDSPACE); }

"UnknownObj"                { count (0); return (PARSEOP_OBJECTTYPE_UNK); }
"IntObj"                    { count (0); return (PARSEOP_OBJECTTYPE_INT); }
"StrObj"                    { count (0); return (PARSEOP_OBJECTTYPE_STR); }
"BuffObj"                   { count (0); return (PARSEOP_OBJECTTYPE_BUF); }
"PkgObj"                    { count (0); return (PARSEOP_OBJECTTYPE_PKG); }
"FieldUnitObj"              { count (0); return (PARSEOP_OBJECTTYPE_FLD); }
"DeviceObj"                 { count (0); return (PARSEOP_OBJECTTYPE_DEV); }
"EventObj"                  { count (0); return (PARSEOP_OBJECTTYPE_EVT); }
"MethodObj"                 { count (0); return (PARSEOP_OBJECTTYPE_MTH); }
"MutexObj"                  { count (0); return (PARSEOP_OBJECTTYPE_MTX); }
"OpRegionObj"               { count (0); return (PARSEOP_OBJECTTYPE_OPR); }
"PowerResObj"               { count (0); return (PARSEOP_OBJECTTYPE_POW); }
"ProcessorObj"              { count (0); return (PARSEOP_OBJECTTYPE_PRO); }
"ThermalZoneObj"            { count (0); return (PARSEOP_OBJECTTYPE_THZ); }
"BuffFieldObj"              { count (0); return (PARSEOP_OBJECTTYPE_BFF); }
"DDBHandleObj"              { count (0); return (PARSEOP_OBJECTTYPE_DDB); }

"AnyAcc"                    { count (0); return (PARSEOP_ACCESSTYPE_ANY); }
"ByteAcc"                   { count (0); return (PARSEOP_ACCESSTYPE_BYTE); }
"WordAcc"                   { count (0); return (PARSEOP_ACCESSTYPE_WORD); }
"DWordAcc"                  { count (0); return (PARSEOP_ACCESSTYPE_DWORD); }
"QWordAcc"                  { count (0); return (PARSEOP_ACCESSTYPE_QWORD); }
"BufferAcc"                 { count (0); return (PARSEOP_ACCESSTYPE_BUF); }

"Lock"                      { count (0); return (PARSEOP_LOCKRULE_LOCK); }
"NoLock"                    { count (0); return (PARSEOP_LOCKRULE_NOLOCK); }

"Preserve"                  { count (0); return (PARSEOP_UPDATERULE_PRESERVE); }
"WriteAsOnes"               { count (0); return (PARSEOP_UPDATERULE_ONES); }
"WriteAsZeros"              { count (0); return (PARSEOP_UPDATERULE_ZEROS); }

"Serialized"                { count (0); return (PARSEOP_SERIALIZERULE_SERIAL); }
"NotSerialized"             { count (0); return (PARSEOP_SERIALIZERULE_NOTSERIAL); }

"SystemIO"                  { count (0); return (PARSEOP_REGIONSPACE_IO); }
"SystemMemory"              { count (0); return (PARSEOP_REGIONSPACE_MEM); }
"PCI_Config"                { count (0); return (PARSEOP_REGIONSPACE_PCI); }
"EmbeddedControl"           { count (0); return (PARSEOP_REGIONSPACE_EC); }
"SMBus"                     { count (0); return (PARSEOP_REGIONSPACE_SMBUS); }
"SystemCMOS"                { count (0); return (PARSEOP_REGIONSPACE_CMOS); }
"PciBarTarget"              { count (0); return (PARSEOP_REGIONSPACE_PCIBAR); }
"IPMI"                      { count (0); return (PARSEOP_REGIONSPACE_IPMI); }

"FFixedHW"                  { count (0); return (PARSEOP_ADDRESSSPACE_FFIXEDHW); }

"SMBQuick"                  { count (0); return (PARSEOP_ACCESSATTRIB_QUICK); }
"SMBSendReceive"            { count (0); return (PARSEOP_ACCESSATTRIB_SND_RCV); }
"SMBByte"                   { count (0); return (PARSEOP_ACCESSATTRIB_BYTE); }
"SMBWord"                   { count (0); return (PARSEOP_ACCESSATTRIB_WORD); }
"SMBBlock"                  { count (0); return (PARSEOP_ACCESSATTRIB_BLOCK); }
"SMBProcessCall"            { count (0); return (PARSEOP_ACCESSATTRIB_WORD_CALL); }
"SMBBlockProcessCall"       { count (0); return (PARSEOP_ACCESSATTRIB_BLOCK_CALL); }

"MTR"                       { count (0); return (PARSEOP_MATCHTYPE_MTR); }
"MEQ"                       { count (0); return (PARSEOP_MATCHTYPE_MEQ); }
"MLE"                       { count (0); return (PARSEOP_MATCHTYPE_MLE); }
"MLT"                       { count (0); return (PARSEOP_MATCHTYPE_MLT); }
"MGE"                       { count (0); return (PARSEOP_MATCHTYPE_MGE); }
"MGT"                       { count (0); return (PARSEOP_MATCHTYPE_MGT); }

"Compatibility"             { count (0); return (PARSEOP_DMATYPE_COMPATIBILITY); }
"TypeA"                     { count (0); return (PARSEOP_DMATYPE_A); }
"TypeB"                     { count (0); return (PARSEOP_DMATYPE_B); }
"TypeF"                     { count (0); return (PARSEOP_DMATYPE_F); }

"BusMaster"                 { count (0); return (PARSEOP_BUSMASTERTYPE_MASTER); }
"NotBusMaster"              { count (0); return (PARSEOP_BUSMASTERTYPE_NOTMASTER); }

"Transfer8"                 { count (0); return (PARSEOP_XFERTYPE_8); }
"Transfer8_16"              { count (0); return (PARSEOP_XFERTYPE_8_16); }
"Transfer16"                { count (0); return (PARSEOP_XFERTYPE_16); }

"ResourceConsumer"          { count (0); return (PARSEOP_RESOURCETYPE_CONSUMER); }
"ResourceProducer"          { count (0); return (PARSEOP_RESOURCETYPE_PRODUCER); }

"MinFixed"                  { count (0); return (PARSEOP_MINTYPE_FIXED); }
"MinNotFixed"               { count (0); return (PARSEOP_MINTYPE_NOTFIXED); }

"MaxFixed"                  { count (0); return (PARSEOP_MAXTYPE_FIXED); }
"MaxNotFixed"               { count (0); return (PARSEOP_MAXTYPE_NOTFIXED); }

"PosDecode"                 { count (0); return (PARSEOP_DECODETYPE_POS); }
"SubDecode"                 { count (0); return (PARSEOP_DECODETYPE_SUB); }

"ISAOnlyRanges"             { count (0); return (PARSEOP_RANGETYPE_ISAONLY); }
"NonISAOnlyRanges"          { count (0); return (PARSEOP_RANGETYPE_NONISAONLY); }
"EntireRange"               { count (0); return (PARSEOP_RANGETYPE_ENTIRE); }

"Cacheable"                 { count (0); return (PARSEOP_MEMTYPE_CACHEABLE); }
"WriteCombining"            { count (0); return (PARSEOP_MEMTYPE_WRITECOMBINING); }
"Prefetchable"              { count (0); return (PARSEOP_MEMTYPE_PREFETCHABLE); }
"NonCacheable"              { count (0); return (PARSEOP_MEMTYPE_NONCACHEABLE); }

"ReadWrite"                 { count (0); return (PARSEOP_READWRITETYPE_BOTH); }
"ReadOnly"                  { count (0); return (PARSEOP_READWRITETYPE_READONLY); }

"Edge"                      { count (0); return (PARSEOP_INTTYPE_EDGE); }
"Level"                     { count (0); return (PARSEOP_INTTYPE_LEVEL); }

"ActiveHigh"                { count (0); return (PARSEOP_INTLEVEL_ACTIVEHIGH); }
"ActiveLow"                 { count (0); return (PARSEOP_INTLEVEL_ACTIVELOW); }

"Shared"                    { count (0); return (PARSEOP_SHARETYPE_SHARED); }
"Exclusive"                 { count (0); return (PARSEOP_SHARETYPE_EXCLUSIVE); }

"Decode10"                  { count (0); return (PARSEOP_IODECODETYPE_10); }
"Decode16"                  { count (0); return (PARSEOP_IODECODETYPE_16); }

"TypeTranslation"           { count (0); return (PARSEOP_TYPE_TRANSLATION); }
"TypeStatic"                { count (0); return (PARSEOP_TYPE_STATIC); }

"SparseTranslation"         { count (0); return (PARSEOP_TRANSLATIONTYPE_SPARSE); }
"DenseTranslation"          { count (0); return (PARSEOP_TRANSLATIONTYPE_DENSE); }

"AddressRangeMemory"        { count (0); return (PARSEOP_ADDRESSTYPE_MEMORY); }
"AddressRangeReserved"      { count (0); return (PARSEOP_ADDRESSTYPE_RESERVED); }
"AddressRangeNVS"           { count (0); return (PARSEOP_ADDRESSTYPE_NVS); }
"AddressRangeACPI"          { count (0); return (PARSEOP_ADDRESSTYPE_ACPI); }


"{"                         { count (0); return('{'); }
"}"                         { count (0); return('}'); }
","                         { count (0); return(','); }
"("                         { count (0); return('('); }
")"                         { count (0); return(')'); }


{NameSeg}                   { char *s;
                                count (0);
                                s=malloc (ACPI_NAME_SIZE + 1);
                                if (strcmp (AslCompilertext, "\\"))
                                {
                                    strcpy (s, "____");
                                    AcpiUtStrupr (AslCompilertext);
                                }
                                memcpy (s, AslCompilertext, strlen (AslCompilertext));
                                AslCompilerlval.s = s;
                                DbgPrint (ASL_PARSE_OUTPUT, "NameSeg: %s\n", s);
                                return (PARSEOP_NAMESEG); }

{NameString}                { char *s;
                                count (0);
                                s=malloc (strlen (AslCompilertext)+1);
                                AcpiUtStrupr (AslCompilertext);
                                strcpy (s, AslCompilertext);
                                s[strlen (AslCompilertext)] = 0;
                                AslCompilerlval.s = s;
                                DbgPrint (ASL_PARSE_OUTPUT, "NameString: %s\n", s);
                                return (PARSEOP_NAMESTRING); }

"*" |
"/"                         { count (1);
                                AslCompilererror ("Parse error, expecting ASL keyword or name");}

.                           { count (1);
                                sprintf (MsgBuffer,
                                    "Invalid character (0x%2.2X), expecting ASL keyword or name",
                                    *AslCompilertext);
                                AslCompilererror (MsgBuffer);}

<<EOF>>                     { if (AslPopInputFileStack ())
                                yyterminate();
                              else
                                return (PARSEOP_INCLUDE_END);};

%%

/*! [End] no source code translation !*/

typedef struct asl_file_node
{
    FILE                    *File;
    UINT32                  CurrentLineNumber;
    YY_BUFFER_STATE         State;
    char                    *Filename;
    struct asl_file_node    *Next;

} ASL_FILE_NODE;

ASL_FILE_NODE               *InputStack = NULL;


/*******************************************************************************
 *
 * FUNCTION:    AslPopInputFileStack
 *
 * PARAMETERS:  None
 *
 * RETURN:      0 if a node was popped, -1 otherwise
 *
 * DESCRIPTION: Pop the top of the input file stack and point the parser to
 *              the saved parse buffer contained in the fnode.  Also, set the
 *              global line counters to the saved values.  This function is
 *              called when an include file reaches EOF.
 *
 ******************************************************************************/

int
AslPopInputFileStack (
    void)
{
    ASL_FILE_NODE           *Fnode;
    FILE                    *InputFile = NULL;


    Fnode = InputStack;
    DbgPrint (ASL_PARSE_OUTPUT, "\nPop InputFile Stack, Fnode %p\n\n", Fnode);


    if (!Fnode)
    {
        return -1;
    }

    /* Close the current include file */

    fclose (yyin);

    /* Update the top-of-stack */

    InputStack = Fnode->Next;
    InputFile = Fnode->File;

    /* Reset global line counter and filename */

    Gbl_Files[ASL_FILE_INPUT].Filename = Fnode->Filename;
    Gbl_CurrentLineNumber = Fnode->CurrentLineNumber;

    /* Point the parser to the popped file */

    yy_delete_buffer (YY_CURRENT_BUFFER);
    yy_switch_to_buffer (Fnode->State);

    /* All done with this node */

    ACPI_FREE (Fnode);
    return 0;
}


/*******************************************************************************
 *
 * FUNCTION:    AslPushInputFileStack
 *
 * PARAMETERS:  InputFile           - Open file pointer
 *              Filename            - Name of the file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Push the InputFile onto the file stack, and point the parser
 *              to this file.  Called when an include file is successfully
 *              opened.
 *
 ******************************************************************************/

void
AslPushInputFileStack (
    FILE                    *InputFile,
    char                    *Filename)
{
    ASL_FILE_NODE           *Fnode;
    YY_BUFFER_STATE         State;


    /* Save the current state in an Fnode */

    Fnode = UtLocalCalloc (sizeof (ASL_FILE_NODE));

    Fnode->File                 = yyin;
    Fnode->Next                 = InputStack;
    Fnode->State                = YY_CURRENT_BUFFER;
    Fnode->CurrentLineNumber    = Gbl_CurrentLineNumber;
    Fnode->Filename             = Gbl_Files[ASL_FILE_INPUT].Filename;

    /* Push it on the stack */

    InputStack = Fnode;

    /* Point the parser to this file */

    State = yy_create_buffer (InputFile, YY_BUF_SIZE);
    yy_switch_to_buffer (State);

    DbgPrint (ASL_PARSE_OUTPUT, "\nPush InputFile Stack, returning %p\n\n", InputFile);

    /* Reset the global line count and filename */

    Gbl_Files[ASL_FILE_INPUT].Filename = Filename;
    Gbl_CurrentLineNumber = 1;
    yyin = InputFile;
}


/*******************************************************************************
 *
 * FUNCTION:    ResetCurrentLineBuffer
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Reset the Line Buffer to zero, increment global line numbers.
 *
 ******************************************************************************/

void
ResetCurrentLineBuffer (
    void)
{

    if (Gbl_Files[ASL_FILE_SOURCE_OUTPUT].Handle)
    {
        FlWriteFile (ASL_FILE_SOURCE_OUTPUT, Gbl_CurrentLineBuffer,
            Gbl_LineBufPtr - Gbl_CurrentLineBuffer);
    }

    Gbl_CurrentLineOffset += Gbl_CurrentColumn;
    Gbl_CurrentColumn = 0;

    Gbl_CurrentLineNumber++;
    Gbl_LogicalLineNumber++;
    Gbl_LineBufPtr = Gbl_CurrentLineBuffer;
}


/*******************************************************************************
 *
 * FUNCTION:    InsertLineBuffer
 *
 * PARAMETERS:  SourceChar      - One char from the input ASL source file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Put one character of the source file into the temp line buffer
 *
 ******************************************************************************/

#define ASL_SPACES_PER_TAB  4

void
InsertLineBuffer (
    int                     SourceChar)
{
    UINT32                  i;
    UINT32                  Count = 1;


    if (SourceChar == EOF)
    {
        return;
    }

    Gbl_InputByteCount++;

    /* Handle tabs.  Convert to spaces */

    if (SourceChar == '\t')
    {
        SourceChar = ' ';
        Count = ASL_SPACES_PER_TAB -
                    (Gbl_CurrentColumn & (ASL_SPACES_PER_TAB-1));
    }


    for (i = 0; i < Count; i++)
    {
        Gbl_CurrentColumn++;

        /* Insert the character into the line buffer */

        *Gbl_LineBufPtr = (UINT8) SourceChar;
        Gbl_LineBufPtr++;

        if (Gbl_LineBufPtr > (Gbl_CurrentLineBuffer + (ASL_LINE_BUFFER_SIZE - 1)))
        {
#if 0
            /*
             * Warning if we have split a long source line.
             * <Probably overkill>
             */
            sprintf (MsgBuffer, "Max %u", ASL_LINE_BUFFER_SIZE);
            AslCommonError (ASL_WARNING, ASL_MSG_LONG_LINE,
                            Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                            Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                            Gbl_Files[ASL_FILE_INPUT].Filename, MsgBuffer);
#endif

            ResetCurrentLineBuffer ();
        }
        else if (SourceChar == '\n')
        {
            /* End of line */

            ResetCurrentLineBuffer ();
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    count
 *
 * PARAMETERS:  yytext      - Contains the matched keyword.
 *              Type        - Keyword/Character type:
 *                             0 = anything except a keyword
 *                             1 = pseudo-keywords
 *                             2 = non-executable ASL keywords
 *                             3 = executable ASL keywords
 *
 * RETURN:      None
 *
 * DESCRIPTION: Count keywords and put them into the line buffer
 *
 ******************************************************************************/

void
count (
    int                 Type)
{
    int                 i;


    switch (Type)
    {
    case 2:
        TotalKeywords++;
        TotalNamedObjects++;
        break;

    case 3:
        TotalKeywords++;
        TotalExecutableOpcodes++;
        break;
    }

    for (i = 0; (yytext[i] != 0) && (yytext[i] != EOF); i++)
    {
        InsertLineBuffer (yytext[i]);
        *Gbl_LineBufPtr = 0;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    comment
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: Process a standard comment.
 *
 ******************************************************************************/

char
comment (void)
{
    char                c;
    char                c1 = 0;


    InsertLineBuffer ('/');
    InsertLineBuffer ('*');

loop:

    /* Eat chars until end-of-comment */

    while ((c = (char) input()) != '*' && c != EOF)
    {
        InsertLineBuffer (c);
        c1 = c;
    }

    if (c == EOF)
    {
        goto EarlyEOF;
    }

    /*
     * Check for nested comment -- can help catch cases where a previous
     * comment was accidently left unterminated
     */
    if ((c1 == '/') && (c == '*'))
    {
        AslCommonError (ASL_WARNING, ASL_MSG_NESTED_COMMENT,
                        Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                        Gbl_InputByteCount, Gbl_CurrentColumn,
                        Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
    }

    /* Comment is closed only if the NEXT character is a slash */

    InsertLineBuffer (c);

    if ((c1 = (char) input()) != '/' && c1 != EOF)
    {
        unput(c1);
        goto loop;
    }

    if (c1 == EOF)
    {
        goto EarlyEOF;
    }

    InsertLineBuffer (c1);
    return TRUE;


EarlyEOF:
    /*
     * Premature End-Of-File
     */
    AslCommonError (ASL_ERROR, ASL_MSG_EARLY_EOF,
                    Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                    Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                    Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    comment
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: Process a new "//" comment.
 *
 ******************************************************************************/

char
comment2 (void)
{
    char                c;


    InsertLineBuffer ('/');
    InsertLineBuffer ('/');

    while ((c = (char) input()) != '\n' && c != EOF)
    {
        InsertLineBuffer (c);
    }

    if (c == EOF)
    {
        /* End of file is OK, change to newline. Let parser detect EOF later */

        c = '\n';
    }

    InsertLineBuffer (c);
    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    literal
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: Process a string literal (surrounded by quotes)
 *
 ******************************************************************************/

#define ASL_NORMAL_CHAR         0
#define ASL_ESCAPE_SEQUENCE     1
#define ASL_OCTAL_CONSTANT      2
#define ASL_HEX_CONSTANT        3

char
literal (void)
{
    char                *StringBuffer = MsgBuffer;
    char                *EndBuffer = MsgBuffer + ASL_MSG_BUFFER_SIZE;
    char                *CleanString;
    char                StringChar;
    UINT32              State = ASL_NORMAL_CHAR;
    UINT32              i = 0;
    UINT8               Digit;
    char                ConvertBuffer[4];


    /*
     * Eat chars until end-of-literal.
     * NOTE:  Put back the original surrounding quotes into the
     * source line buffer.
     */
    InsertLineBuffer ('\"');
    while ((StringChar = (char) input()) != EOF)
    {
        InsertLineBuffer (StringChar);

DoCharacter:

        switch (State)
        {
        case ASL_NORMAL_CHAR:

            switch (StringChar)
            {
            case '\\':
                /*
                 * Special handling for backslash-escape sequence.  We will
                 * toss the backslash and translate the escape char(s).
                 */
                State = ASL_ESCAPE_SEQUENCE;
                continue;

            case '\"':

                /* String terminator */

                goto CompletedString;
            }
            break;


        case ASL_ESCAPE_SEQUENCE:

            State = ASL_NORMAL_CHAR;
            switch (StringChar)
            {
            case 'a':
                StringChar = 0x07;      /* BELL */
                break;

            case 'b':
                StringChar = 0x08;      /* BACKSPACE */
                break;

            case 'f':
                StringChar = 0x0C;      /* FORMFEED */
                break;

            case 'n':
                StringChar = 0x0A;      /* LINEFEED */
                break;

            case 'r':
                StringChar = 0x0D;      /* CARRIAGE RETURN*/
                break;

            case 't':
                StringChar = 0x09;      /* HORIZONTAL TAB */
                break;

            case 'v':
                StringChar = 0x0B;      /* VERTICAL TAB */
                break;

            case 'x':
                State = ASL_HEX_CONSTANT;
                i = 0;
                continue;

            case '\'':                  /* Single Quote */
            case '\"':                  /* Double Quote */
            case '\\':                  /* Backslash */
                break;

            default:

                /* Check for an octal digit (0-7) */

                if (ACPI_IS_OCTAL_DIGIT (StringChar))
                {
                    State = ASL_OCTAL_CONSTANT;
                    ConvertBuffer[0] = StringChar;
                    i = 1;
                    continue;
                }

                /* Unknown escape sequence issue warning, but use the character */

                AslCommonError (ASL_WARNING, ASL_MSG_INVALID_ESCAPE,
                                Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                                Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                                Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
				break;
            }
            break;


        case ASL_OCTAL_CONSTANT:

            /* Up to three octal digits allowed */

            if (!ACPI_IS_OCTAL_DIGIT (StringChar) ||
                (i > 2))
            {
                /*
                 * Reached end of the constant.  Convert the assembled ASCII
                 * string and resume processing of the next character
                 */
                ConvertBuffer[i] = 0;
                Digit = (UINT8) ACPI_STRTOUL (ConvertBuffer, NULL, 8);

                /* Check for NULL or non-ascii character (ignore if so) */

                if ((Digit == 0) || (Digit > ACPI_ASCII_MAX))
                {
                    AslCommonError (ASL_WARNING, ASL_MSG_INVALID_STRING,
                                    Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                                    Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                                    Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
                }
                else
                {
                    *StringBuffer = (char) Digit;
                    StringBuffer++;
                    if (StringBuffer >= EndBuffer)
                    {
                        goto BufferOverflow;
                    }
                }

                State = ASL_NORMAL_CHAR;
                goto DoCharacter;
                break;
            }

            /* Append another digit of the constant */

            ConvertBuffer[i] = StringChar;
            i++;
            continue;


        case ASL_HEX_CONSTANT:

            /* Up to two hex digits allowed */

            if (!ACPI_IS_XDIGIT (StringChar) ||
                (i > 1))
            {
                /*
                 * Reached end of the constant.  Convert the assembled ASCII
                 * string and resume processing of the next character
                 */
                ConvertBuffer[i] = 0;
                Digit = (UINT8) ACPI_STRTOUL (ConvertBuffer, NULL, 16);

                /* Check for NULL or non-ascii character (ignore if so) */

                if ((Digit == 0) || (Digit > ACPI_ASCII_MAX))
                {
                    AslCommonError (ASL_WARNING, ASL_MSG_INVALID_STRING,
                                    Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                                    Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                                    Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
                }
                else
                {
                    *StringBuffer = (char) Digit;
                    StringBuffer++;
                    if (StringBuffer >= EndBuffer)
                    {
                        goto BufferOverflow;
                    }
                }

                State = ASL_NORMAL_CHAR;
                goto DoCharacter;
                break;
            }

            /* Append another digit of the constant */

            ConvertBuffer[i] = StringChar;
            i++;
            continue;
        }

        /* Save the finished character */

        *StringBuffer = StringChar;
        StringBuffer++;
        if (StringBuffer >= EndBuffer)
        {
            goto BufferOverflow;
        }
    }

    /*
     * Premature End-Of-File
     */
    AslCommonError (ASL_ERROR, ASL_MSG_EARLY_EOF,
                    Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                    Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                    Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
    return (FALSE);


CompletedString:
    /*
     * Null terminate the input string and copy string to a new buffer
     */
    *StringBuffer = 0;

    CleanString = UtGetStringBuffer (strlen (MsgBuffer) + 1);
    if (!CleanString)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_MEMORY_ALLOCATION,
                        Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                        Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                        Gbl_Files[ASL_FILE_INPUT].Filename, NULL);
        return (FALSE);
    }

    ACPI_STRCPY (CleanString, MsgBuffer);
    AslCompilerlval.s = CleanString;
    return (TRUE);


BufferOverflow:

    /* Literal was too long */

    AslCommonError (ASL_ERROR, ASL_MSG_STRING_LENGTH,
                    Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
                    Gbl_CurrentLineOffset, Gbl_CurrentColumn,
                    Gbl_Files[ASL_FILE_INPUT].Filename, "Max length 4096");
    return (FALSE);
}


