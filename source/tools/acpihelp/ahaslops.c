/******************************************************************************
 *
 * Module Name: ahaslops - Table of all known ASL operators
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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

#include "acpihelp.h"

/*
 * ASL operators with syntax (directly from ACPI specification).
 * Note: All tokens require a space separator.
 * Long lines are automatically split during output.
 */
const AH_ASL_OPERATOR       Gbl_AslOperatorInfo[] =
{
    {"AccessAs",                "(AccessType, AccessAttribKeyword | "
                                "ExtendedAttribKeyword (AccessLength))",
                                "ChangeFieldUnitAccess"},
    {"Acquire",                 "(SyncObject, TimeoutValue) => Boolean",
                                "Acquire a mutex"},
    {"Add",                     "(Addend1, Addend2, Result) => Integer",
                                "Integer Add"},
    {"Alias",                   "(SourceObject, AliasObject)",
                                "Define a name alias"},
    {"And",                     "(Source1, Source2, Result) => Integer",
                                "Integer Bitwise And"},
    {"Arg",                     "Arg0 - Arg6",
                                "Method argument data objects"},
    {"BankField",               "(RegionName, BankName, BankValue, "
                                "AccessTypeKeyword, LockRuleKeyword, "
                                "UpdateRuleKeyword) {FieldUnitList}",
                                "Declare fields in a banked configuration object"},
    {"Break",                   "No parameters",
                                "Continue following the innermost enclosing While"},
    {"BreakPoint",              "No parameters",
                                "Used for debugging, stops execution in the debugger"},
    {"Buffer",                  "(BufferSize) {String or ByteList} => Buffer",
                                "Declare Buffer object"},
    {"Case",                    "(Value) {TermList}",
                                "Expression for conditional execution"},
    {"Concatenate",             "(Source1, Source2, Result) => ComputationalData",
                                "Concatenate two strings, integers or buffers"},
    {"ConcatenateResTemplate",  "(Source1, Source2, Result) => Buffer",
                                "Concatenate two resource templates"},
    {"CondRefOf",               "(Source, Result) => Boolean",
                                "Conditional reference to an object"},
    {"Connection",              "(ResourceMacro)",
                                "Associate connection with FieldUnits within a Field object"},
    {"Continue",                "No parameters",
                                "Continue innermost enclosing While loop"},
    {"CopyObject",              "(Source, Destination) => DataRefObject",
                                "Copy and existing object"},
    {"CreateBitField",          "(SourceBuffer, BitIndex, BitFieldName)",
                                "Declare a bit field object of a buffer object"},
    {"CreateByteField",         "(SourceBuffer, ByteIndex, ByteFieldName)",
                                "Declare a byte field object of a buffer object"},
    {"CreateDWordField",        "(SourceBuffer, ByteIndex, DWordFieldName)",
                                "Declare a DWord field object of a buffer object"},
    {"CreateField",             "(SourceBuffer, BitIndex, NumBits, FieldName)",
                                "Declare an arbitrary length bit field of a buffer object"},
    {"CreateQWordField",        "(SourceBuffer, ByteIndex, QWordFieldName)",
                                "Declare a QWord field object of a buffer object"},
    {"CreateWordField",         "(SourceBuffer, ByteIndex, WordFieldName)",
                                "Declare a Word field object of a buffer object"},
    {"DataTableRegion",         "(RegionName, SignatureString, OemIDString, OemTableIDString)",
                                "Declare a Data Table Region"},
    {"Debug",                   "No parameters",
                                "Debugger output"},
    {"Decrement",               "(Minuend) => Integer",
                                "Decrement an Integer"},
    {"Default",                 "{TermList}",
                                "Default execution path in Switch()"},
    {"DefinitionBlock",         "(AmlFileName, TableSignature, ComplianceRevision, "
                                "OemId, TableId, OemRevision) {TermList}",
                                "Declare a Definition Block"},
    {"DerefOf",                 "(Source) => Object",
                                "Dereference an object reference"},
    {"Device",                  "(DeviceName) {TermList}",
                                "Declare a bus/device object"},
    {"Divide",                  "(Dividend, Divisor, Remainder, Result) => Integer",
                                "Integer Divide"},
    {"Dma",                     "(DmaTypeKeyword, BusMasterKeyword, XferTypeKeyword, "
                                "DescriptorName) {DmaChannelList} => Buffer",
                                "DMA Resource Descriptor macro"},
    {"DWordIo",                 "(ResourceTypeKeyword, MinKeyword, MaxKeyword, "
                                "DecodeKeyword, RangeTypeKeyword, AddressGranularity, "
                                "AddressMinimum, AddressMaximum, AddressTranslation, "
                                "RangeLength, ResourceSourceIndex, "
                                "ResourceSource, DescriptorName, TypeKeyword, TranslationKeyword)",
                                "DWord I/O Resource Descriptor macro"},
    {"DWordMemory",             "(ResourceTypeKeyword, DecodeKeyword, MinKeyword, "
                                "MaxKeyword, MemTypeKeyword, ReadWriteKeyword, "
                                "AddressGranularity, AddressMinimum, AddressMaximum, AddressTranslation, "
                                "RangeLength, ResourceSourceIndex, ResourceSource, DescriptorName, AddressKeyword, "
                                "TypeKeyword)",
                                "DWord Memory Resource Descriptor macro"},
    {"DWordSpace",              "(ResourceType, ResourceTypeKeyword, DecodeKeyword, "
                                "MinKeyword, MaxKeyword, TypeSpecificFlags, "
                                "AddressGranularity, AddressMinimum, AddressMaximum, "
                                "AddressTranslation, RangeLength, "
                                "ResourceSourceIndex, ResourceSource, DescriptorName)",
                                "DWord Space Resource Descriptor macro"},
    {"EisaId",                  "(EisaIdString) => DWordConst",
                                "EISA ID String to Integer conversion macro"},
    {"Else",                    "{TermList}",
                                "Alternate conditional execution"},
    {"ElseIf",                  "(Predicate)",
                                "Conditional execution"},
    {"EndDependentFn",          "() => Buffer",
                                "End Dependent Function Resource Descriptor macro"},
    {"Event",                   "(EventName)",
                                "Declare an event synchronization object"},
    {"ExtendedIo",              "(ResourceTypeKeyword, MinKeyword, MaxKeyword, "
                                "DecodeKeyword, RangeTypeKeyword, AddressGranularity, "
                                "AddressMinimum, AddressMaximum, AddressTranslation, RangeLength, "
                                "TypeSpecificAttributes, DescriptorName, TypeKeyword, TranslationKeyword)",
                                "Extended I/O Resource Descriptor macro"},
    {"ExtendedMemory",          "(ResourceTypeKeyword, DecodeKeyword, MinKeyword, "
                                "MaxKeyword, MemTypeKeyword, ReadWriteKeyword, "
                                "AddressGranularity, AddressMinimum, AddressMaximum, AddressTranslation, "
                                "RangeLength, TypeSpecificAttributes, DescriptorName, "
                                "AddressKeyword, TypeKeyword)",
                                "Extended Memory Resource Descriptor macro"},
    {"ExtendedSpace",           "(ResourceType, ResourceTypeKeyword, DecodeKeyword, "
                                "MinKeyword, MaxKeyword, TypeSpecificFlags, "
                                "AddressGranularity, AddressMinimum, AddressMaximum, AddressTranslation, "
                                "RangeLength, TypeSpecificAttributes, DescriptorName)",
                                "Extended Space Resource Descriptor macro"},
    {"External",                "(ObjectName, ObjectTypeKeyword, ReturnType, ParameterTypes)",
                                "Declare external objects"},
    {"Fatal",                   "(Type, Code, Arg)",
                                "Fatal error check"},
    {"Field",                   "(RegionName, AccessTypeKeyword, LockRuleKeyword, "
                                "UpdateRuleKeyword) {FieldUnitList}",
                                "Declare fields of an operation region object"},
    {"FindSetLeftBit",          "(Source, Result) => Integer",
                                "Index of first least significant bit set"},
    {"FindSetRightBit",         "(Source, Result) => Integer",
                                "Index of first most significant bit set"},
    {"FixedDma",                "(DmaRequestLine, Channel, TransferWidthKeyword, DescriptorName) => Buffer",
                                "Fixed DMA Resource Descriptor macro"},
    {"FixedIo",                 "(AddressBase, RangeLength, DescriptorName) => Buffer",
                                "Fixed I/O Resource Descriptor macro"},
    {"FromBcd",                 "(BcdValue, Result) => Integer",
                                "Convert from BCD to numeric"},
    {"Function",                "(FunctionName, ReturnType, ParameterTypes) {TermList}",
                                "Declare control method"},
    {"GpioInt",                 "(InterruptTypeKeyword, InterruptLevelKeyword, "
                                "ShareTypeKeyword, PinConfigKeyword, "
                                "DebounceTimeout, ResourceSource, "
                                "ResourceSourceIndex, ResourceTypeKeyword, DescriptorName, "
                                "RawDataBuffer() {VendorData}) {Pin}",
                                "GPIO Interrupt Connection Resource Descriptor Macro"},
    {"GpioIo",                  "(ShareTypeKeyword, PinConfigKeyword, DebounceTimeout, DriveStrength, "
                                "IoRestrictionKeyword, ResourceSource, "
                                "ResourceSourceIndex, ResourceTypeKeyword, DescriptorName, "
                                "RawDataBuffer() {VendorData}) {PinList}",
                                "GPIO I/O Connection Resource Descriptor Macro"},
    {"I2cSerialBusV2",           "(SlaveAddress, SlaveModeKeyword, ConnectionSpeed, "
                                "AddressingModeKeyword, ResourceSource, "
                                "ResourceSourceIndex, ResourceTypeKeyword, DescriptorName, Shared, "
                                "RawDataBuffer() {VendorData})",
                                "I2C Serial Bus Connection Resource Descriptor Macro"},
    {"If",                      "(Predicate) {TermList}",
                                "Conditional execution"},
    {"Include",                 "(FilePathName)",
                                "Include another ASL file"},
    {"Increment",               "(Addend) => Integer",
                                "Increment a Integer"},
    {"Index",                   "(Source, Index, Destination) => ObjectReference",
                                "Indexed Reference to member object"},
    {"IndexField",              "(IndexName, DataName, AccessTypeKeyword, LockRuleKeyword, "
                                "UpdateRuleKeyword) {FieldUnitList}",
                                "Declare Index/Data Fields"},
    {"Interrupt",               "(ResourceTypeKeyword, InterruptTypeKeyword, InterruptLevelKeyword, "
                                "ShareTypeKeyword, ResourceSourceIndex, "
                                "ResourceSource, DescriptorName) {InterruptList} => Buffer",
                                "Interrupt Resource Descriptor macro"},
    {"Io",                      "(IoDecodeKeyword, AddressMin, AddressMax, AddressAlignment, "
                                "RangeLength, DescriptorName) => Buffer",
                                "I/O Resource Descriptor macro"},
    {"Irq",                     "(InterruptTypeKeyword, InterruptLevelKeyword, ShareTypeKeyword, "
                                "DescriptorName) {InterruptList} => Buffer",
                                "Interrupt Resource Descriptor macro"},
    {"IrqNoFlags",              "(DescriptorName) {InterruptList} => Buffer",
                                "Short Interrupt Resource Descriptor macro"},
    {"LAnd",                    "(Source1, Source2) => Boolean",
                                "Logical And"},
    {"LEqual",                  "(Source1, Source2) => Boolean",
                                "Logical Equal"},
    {"LGreater",                "(Source1, Source2) => Boolean",
                                "Logical Greater"},
    {"LGreaterEqual",           "(Source1, Source2) => Boolean",
                                "Logical Not less"},
    {"LLess",                   "(Source1, Source2) => Boolean",
                                "Logical Less"},
    {"LLessEqual",              "(Source1, Source2) => Boolean",
                                "Logical Not greater"},
    {"LNot",                    "(Source) => Boolean",
                                "Logical Not"},
    {"LNotEqual",               "(Source1, Source2) => Boolean",
                                "Logical Not equal"},
    {"Load",                    "(Object, DDBHandle)",
                                "Load differentiating definition block"},
    {"LoadTable",               "(SignatureString, OemIdString, OemTableIdString, RootPathString, "
                                "ParameterPathString, ParameterData) => DDBHandle",
                                "Load Table from RSDT/XSDT"},
    {"Local",                   "Local0 - Local7",
                                "Method local data objects"},
    {"LOr",                     "(Source1, Source2) => Boolean",
                                "Logical Or"},
    {"Match",                   "(SearchPackage, MatchOpKeyword, MatchObject1, MatchOpKeyword, "
                                "MatchObject2, StartIndex) => Ones | Integer",
                                "Search for match in package array"},
    {"Memory24",                "(ReadWriteKeyword, AddressMinimum, AddressMaximum, AddressAlignment, "
                                "RangeLength, DescriptorName)",
                                "Memory Resource Descriptor macro"},
    {"Memory32",                "(ReadWriteKeyword, AddressMinimum, AddressMaximum, AddressAlignment, "
                                "RangeLength, DescriptorName)",
                                "Memory Resource Descriptor macro"},
    {"Memory32Fixed",           "(ReadWriteKeyword, AddressBase, RangeLength, DescriptorName)",
                                "Memory Resource Descriptor macro"},
    {"Method",                  "(MethodName, NumArgs, SerializeRuleKeyword, "
                                "SyncLevel, ReturnType, ParameterTypes) "
                                "{TermList}",
                                "Declare a control method"},
    {"Mid",                     "(Source, Index, Length, Result) => Buffer or String",
                                "Return a portion of buffer or string"},
    {"Mod",                     "(Dividend, Divisor, Result) => Integer",
                                "Integer Modulo"},
    {"Multiply",                "(Multiplicand, Multiplier, Result) => Integer",
                                "Integer Multiply"},
    {"Mutex",                   "(MutexName, SyncLevel)",
                                "Declare a mutex synchronization object"},
    {"Name",                    "(ObjectName, Object)",
                                "Declare a Named object"},
    {"NAnd",                    "(Source1, Source2, Result) => Integer",
                                "Integer Bitwise Nand"},
    {"NoOp",                    "No parameters",
                                "No operation"},
    {"NOr",                     "(Source1, Source2, Result) => Integer",
                                "Integer Bitwise Nor"},
    {"Not",                     "(Source, Result) => Integer",
                                "Integer Bitwise Not"},
    {"Notify",                  "(Object, NotificationValue)",
                                "Notify Object of event"},
    {"ObjectType",              "(Object) => Integer",
                                "Type of object"},
    {"Offset",                  "(ByteOffset)",
                                "Change Current Field Unit Offset"},
    {"One",                     "=> Integer",
                                "Constant One Object (1)"},
    {"Ones",                    "=> Integer",
                                "Constant Ones Object (0xFFFFFFFF or 0xFFFFFFFFFFFFFFFF)"},
    {"OperationRegion",         "(RegionName, RegionSpaceKeyword, Offset, Length)",
                                "Declare an operational region"},
    {"Or",                      "(Source1, Source2, Result) => Integer",
                                "Integer Bitwise Or"},
    {"Package",                 "(NumElements) {PackageList} => Package",
                                "Declare a package object"},
    {"PowerResource",           "(ResourceName, SystemLevel, ResourceOrder) {TermList}",
                                "Declare a power resource object"},
    {"Processor",               "(ProcessorName, ProcessorID, PBlockAddress, PblockLength) {TermList}",
                                "Declare a processor package"},
    {"QWordIo",                 "(ResourceTypeKeyword, MinKeyword, MaxKeyword, DecodeKeyword, "
                                "RangeTypeKeyword, AddressGranularity, "
                                "AddressMinimum, AddressMaximum, AddressTranslation, RangeLength, "
                                "ResourceSourceIndex, ResourceSource, DescriptorName, TypeKeyword, "
                                "TranslationKeyword)",
                                "QWord I/O Resource Descriptor macro"},
    {"QWordMemory",             "(ResourceTypeKeyword, DecodeKeyword, MinKeyword, MaxKeyword, "
                                "MemTypeKeyword, ReadWriteKeyword, "
                                "AddressGranularity, AddressMinimum, AddressMaximum, AddressTranslation, "
                                "RangeLength, ResourceSourceIndex, ResourceSource, "
                                "DescriptorName, AddressKeyword, "
                                "TypeKeyword)",
                                "QWord Memory Resource Descriptor macro"},
    {"QWordSpace",              "(ResourceType, ResourceTypeKeyword, DecodeKeyword, "
                                "MinKeyword, MaxKeyword, TypeSpecificFlags, "
                                "AddressGranularity, AddressMinimum, AddressMaximum, AddressTranslation, "
                                "RangeLength, ResourceSourceIndex, ResourceSource, DescriptorName)",
                                "Qword Space Resource Descriptor macro"},
    {"RawDataBuffer",           "(BufferSize) {ByteList} => RawDataBuffer",
                                "Create a raw data buffer (does not use Buffer AML opcode)"},
    {"RefOf",                   "(Object) => ObjectReference",
                                "Create Reference to an object"},
    {"Register",                "(AddressSpaceKeyword, RegisterBitWidth, "
                                "RegisterBitOffset, RegisterAddress, "
                                "AccessSize, DescriptorName)",
                                "Generic register Resource Descriptor macro"},
    {"Release",                 "(SyncObject)",
                                "Release a synchronization object"},
    {"Reset",                   "(SyncObject)",
                                "Reset a synchronization object"},
    {"ResourceTemplate",        "() {ResourceMacroList} => Buffer",
                                "Resource to buffer conversion macro"},
    {"Return",                  "None | () | (ReturnArg)",
                                "Return from method execution"},
    {"Revision",                "=> Integer",
                                "Constant revision object"},
    {"Scope",                   "(Location) {TermList}",
                                "Open named scope "},
    {"ShiftLeft",               "(Source, ShiftCount, Result) => Integer",
                                "Integer shift value left"},
    {"ShiftRight",              "(Source, ShiftCount, Result) => Integer",
                                "Integer shift value right"},
    {"Signal",                  "(SyncObject)",
                                "Signal a synchronization object"},
    {"SizeOf",                  "(ObjectName) => Integer",
                                "Get the size of a buffer}, string}, or package"},
    {"Sleep",                   "(Milliseconds)",
                                "Sleep n milliseconds (yields the processor)"},
    {"SpiSerialBusV2",          "(DeviceSelection, PolarityKeyword, WireModeKeyword, "
                                "DataBitLength, SlaveModeKeyword, "
                                "ConnectionSpeed, ClockPolarityKeyword, ClockPhaseKeyword, "
                                "ResourceSource, ResourceSourceIndex, "
                                "ResourceTypeKeyword, DescriptorName, Shared, RawDataBuffer() {VendorData})",
                                "SPI Serial Bus Connection Resource Descriptor Macro"},
    {"Stall",                   "(Microseconds)",
                                "Delay n microseconds (does not yield the processor)"},
    {"StartDependentFn",        "(CompatibilityPriority, PerformancePriority) {ResourceList}",
                                "Start Dependent Function Resource Descriptor macro"},
    {"StartDependentFnNoPri",   "() {ResourceList}",
                                "Start Dependent Function Resource Descriptor macro"},
    {"Store",                   "(Source, Destination) => DataRefObject",
                                "Store object"},
    {"Subtract",                "(Minuend, Subtrahend, Result) => Integer",
                                "Integer Subtract"},
    {"Switch",                  "(Expression) {CaseTermList}",
                                "Select code to execute based on expression value"},
    {"ThermalZone",             "(ThermalZoneName) {TermList}",
                                "Declare a thermal zone package"},
    {"Timer",                   "=> Integer",
                                "Get 64-bit timer value"},
    {"ToBcd",                   "(Value, Result) => Integer",
                                "Convert Integer to BCD"},
    {"ToBuffer",                "(Data, Result) => Buffer",
                                "Convert data type to buffer"},
    {"ToDecimalString",         "(Data, Result) => String",
                                "Convert data type to decimal string"},
    {"ToHexString",             "(Data, Result) => String",
                                "Convert data type to hexadecimal string"},
    {"ToInteger",               "(Data, Result) => Integer",
                                "Convert data type to integer"},
    {"ToString",                "(Source, Length, Result) => String",
                                "Copy ASCII string from buffer"},
    {"ToUuid",                  "(AsciiString) => Buffer",
                                "Convert Ascii string to UUID"},
    {"UartSerialBusV2",         "(ConnectionSpeed, ByteLengthKeyword, StopBitsKeyword, "
                                "LinesInUse, EndianKeyword, ParityKeyword, "
                                "FlowControlKeyword, ReceiveBufferSize, TransmitBufferSize, ResourceSource, "
                                "ResourceSourceIndex, ResourceTypeKeyword, DescriptorName, Shared, "
                                "RawDataBuffer() {VendorData})",
                                "UART Serial Bus Connection Resource Descriptor Macro"},
    {"Unicode",                 "(String) => Buffer",
                                "String to Unicode conversion macro"},
    {"Unload",                  "(Handle)",
                                "Unload definition block"},
    {"VendorLong",              "(DescriptorName) {VendorByteList}",
                                "Vendor Resource Descriptor"},
    {"VendorShort",             "(DescriptorName) {VendorByteList}",
                                "Vendor Resource Descriptor"},
    {"Wait",                    "(SyncObject, TimeoutValue) => Boolean",
                                "Wait on an Event"},
    {"While",                   "(Predicate) {TermList}",
                                "Conditional loop"},
    {"WordBusNumber",           "(ResourceTypeKeyword, MinKeyword, MaxKeyword, DecodeKeyword, "
                                "AddressGranularity, AddressMinimum, "
                                "AddressMaximum, AddressTranslation, RangeLength, ResourceSourceIndex, "
                                "ResourceSource, DescriptorName)",
                                "Word Bus number Resource Descriptor macro"},
    {"WordIo",                  "(ResourceTypeKeyword, MinKeyword, MaxKeyword, DecodeKeyword, "
                                "RangeTypeKeyword, AddressGranularity, "
                                "AddressMinimum, AddressMaximum, AddressTranslation, RangeLength, "
                                "ResourceSourceIndex, ResourceSource, DescriptorName, TypeKeyword, "
                                "TranslationKeyword)",
                                "Word I/O Resource Descriptor macro"},
    {"WordSpace",               "(ResourceType, ResourceTypeKeyword, DecodeKeyword, MinKeyword, "
                                "MaxKeyword, TypeSpecificFlags, "
                                "AddressGranularity, AddressMinimum, AddressMaximum, AddressTranslation, "
                                "RangeLength, ResourceSourceIndex, ResourceSource, DescriptorName)",
                                "Word Space Resource Descriptor macro"},
    {"Xor",                     "(Source1, Source2, Result) => Integer",
                                "Integer Bitwise Xor"},
    {"Zero",                    "=> Integer",
                                "Constant Zero object (0)"},
    {NULL, NULL, NULL}
};
