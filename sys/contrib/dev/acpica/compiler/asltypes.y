NoEcho('
/******************************************************************************
 *
 * Module Name: asltypes.y - Bison/Yacc production types/names
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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

')

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
%type <n> SimpleTarget
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
%type <n> ObjectTypeName
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
%type <n> VarPackageLengthTerm

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
%type <n> OptionalAddressingMode
%type <n> OptionalAddressRange
%type <n> OptionalBitsPerByte
%type <n> OptionalBuffer_Last
%type <n> OptionalBufferLength
%type <n> OptionalByteConstExpr
%type <n> OptionalCount
%type <n> OptionalDecodeType
%type <n> OptionalDevicePolarity
%type <n> OptionalDWordConstExpr
%type <n> OptionalEndian
%type <n> OptionalFlowControl
%type <n> OptionalIoRestriction
%type <n> OptionalListString
%type <n> OptionalMaxType
%type <n> OptionalMemType
%type <n> OptionalMinType
%type <n> OptionalNameString
%type <n> OptionalNameString_First
%type <n> OptionalNameString_Last
%type <n> OptionalObjectTypeKeyword
%type <n> OptionalParameterTypePackage
%type <n> OptionalParameterTypesPackage
%type <n> OptionalParityType
%type <n> OptionalPredicate
%type <n> OptionalQWordConstExpr
%type <n> OptionalRangeType
%type <n> OptionalReference
%type <n> OptionalResourceType
%type <n> OptionalResourceType_First
%type <n> OptionalReturnArg
%type <n> OptionalSerializeRuleKeyword
%type <n> OptionalShareType
%type <n> OptionalShareType_First
%type <n> OptionalSlaveMode
%type <n> OptionalStopBits
%type <n> OptionalStringData
%type <n> OptionalTermArg
%type <n> OptionalTranslationType_Last
%type <n> OptionalType
%type <n> OptionalType_Last
%type <n> OptionalWireMode
%type <n> OptionalWordConst
%type <n> OptionalWordConstExpr
%type <n> OptionalXferSize

/*
 * C-style expression parser
 */
%type <n> Expression
%type <n> EqualsTerm
%type <n> IndexExpTerm
