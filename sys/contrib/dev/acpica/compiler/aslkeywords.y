NoEcho('
/******************************************************************************
 *
 * Module Name: aslkeywords.y - Rules for resource descriptor keywords
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

/*******************************************************************************
 *
 * ASL Parameter Keyword Terms
 *
 ******************************************************************************/

AccessAttribKeyword
    : PARSEOP_ACCESSATTRIB_BLOCK            {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_BLOCK);}
    | PARSEOP_ACCESSATTRIB_BLOCK_CALL       {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_BLOCK_CALL);}
    | PARSEOP_ACCESSATTRIB_BYTE             {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_BYTE);}
    | PARSEOP_ACCESSATTRIB_QUICK            {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_QUICK );}
    | PARSEOP_ACCESSATTRIB_SND_RCV          {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_SND_RCV);}
    | PARSEOP_ACCESSATTRIB_WORD             {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_WORD);}
    | PARSEOP_ACCESSATTRIB_WORD_CALL        {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_WORD_CALL);}
    | PARSEOP_ACCESSATTRIB_MULTIBYTE
        PARSEOP_OPEN_PAREN                  {$<n>$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_MULTIBYTE);}
        ByteConst
        PARSEOP_CLOSE_PAREN                 {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_ACCESSATTRIB_RAW_BYTES
        PARSEOP_OPEN_PAREN                  {$<n>$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_RAW_BYTES);}
        ByteConst
        PARSEOP_CLOSE_PAREN                 {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_ACCESSATTRIB_RAW_PROCESS
        PARSEOP_OPEN_PAREN                  {$<n>$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_RAW_PROCESS);}
        ByteConst
        PARSEOP_CLOSE_PAREN                 {$$ = TrLinkChildren ($<n>3,1,$4);}
    ;

AccessTypeKeyword
    : PARSEOP_ACCESSTYPE_ANY                {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_ANY);}
    | PARSEOP_ACCESSTYPE_BYTE               {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_BYTE);}
    | PARSEOP_ACCESSTYPE_WORD               {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_WORD);}
    | PARSEOP_ACCESSTYPE_DWORD              {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_DWORD);}
    | PARSEOP_ACCESSTYPE_QWORD              {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_QWORD);}
    | PARSEOP_ACCESSTYPE_BUF                {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_BUF);}
    ;

AddressingModeKeyword
    : PARSEOP_ADDRESSINGMODE_7BIT           {$$ = TrCreateLeafNode (PARSEOP_ADDRESSINGMODE_7BIT);}
    | PARSEOP_ADDRESSINGMODE_10BIT          {$$ = TrCreateLeafNode (PARSEOP_ADDRESSINGMODE_10BIT);}
    ;

AddressKeyword
    : PARSEOP_ADDRESSTYPE_MEMORY            {$$ = TrCreateLeafNode (PARSEOP_ADDRESSTYPE_MEMORY);}
    | PARSEOP_ADDRESSTYPE_RESERVED          {$$ = TrCreateLeafNode (PARSEOP_ADDRESSTYPE_RESERVED);}
    | PARSEOP_ADDRESSTYPE_NVS               {$$ = TrCreateLeafNode (PARSEOP_ADDRESSTYPE_NVS);}
    | PARSEOP_ADDRESSTYPE_ACPI              {$$ = TrCreateLeafNode (PARSEOP_ADDRESSTYPE_ACPI);}
    ;

AddressSpaceKeyword
    : ByteConst                             {$$ = UtCheckIntegerRange ($1, 0x0A, 0xFF);}
    | RegionSpaceKeyword                    {}
    ;

BitsPerByteKeyword
    : PARSEOP_BITSPERBYTE_FIVE              {$$ = TrCreateLeafNode (PARSEOP_BITSPERBYTE_FIVE);}
    | PARSEOP_BITSPERBYTE_SIX               {$$ = TrCreateLeafNode (PARSEOP_BITSPERBYTE_SIX);}
    | PARSEOP_BITSPERBYTE_SEVEN             {$$ = TrCreateLeafNode (PARSEOP_BITSPERBYTE_SEVEN);}
    | PARSEOP_BITSPERBYTE_EIGHT             {$$ = TrCreateLeafNode (PARSEOP_BITSPERBYTE_EIGHT);}
    | PARSEOP_BITSPERBYTE_NINE              {$$ = TrCreateLeafNode (PARSEOP_BITSPERBYTE_NINE);}
    ;

ClockPhaseKeyword
    : PARSEOP_CLOCKPHASE_FIRST              {$$ = TrCreateLeafNode (PARSEOP_CLOCKPHASE_FIRST);}
    | PARSEOP_CLOCKPHASE_SECOND             {$$ = TrCreateLeafNode (PARSEOP_CLOCKPHASE_SECOND);}
    ;

ClockPolarityKeyword
    : PARSEOP_CLOCKPOLARITY_LOW             {$$ = TrCreateLeafNode (PARSEOP_CLOCKPOLARITY_LOW);}
    | PARSEOP_CLOCKPOLARITY_HIGH            {$$ = TrCreateLeafNode (PARSEOP_CLOCKPOLARITY_HIGH);}
    ;

DecodeKeyword
    : PARSEOP_DECODETYPE_POS                {$$ = TrCreateLeafNode (PARSEOP_DECODETYPE_POS);}
    | PARSEOP_DECODETYPE_SUB                {$$ = TrCreateLeafNode (PARSEOP_DECODETYPE_SUB);}
    ;

DevicePolarityKeyword
    : PARSEOP_DEVICEPOLARITY_LOW            {$$ = TrCreateLeafNode (PARSEOP_DEVICEPOLARITY_LOW);}
    | PARSEOP_DEVICEPOLARITY_HIGH           {$$ = TrCreateLeafNode (PARSEOP_DEVICEPOLARITY_HIGH);}
    ;

DMATypeKeyword
    : PARSEOP_DMATYPE_A                     {$$ = TrCreateLeafNode (PARSEOP_DMATYPE_A);}
    | PARSEOP_DMATYPE_COMPATIBILITY         {$$ = TrCreateLeafNode (PARSEOP_DMATYPE_COMPATIBILITY);}
    | PARSEOP_DMATYPE_B                     {$$ = TrCreateLeafNode (PARSEOP_DMATYPE_B);}
    | PARSEOP_DMATYPE_F                     {$$ = TrCreateLeafNode (PARSEOP_DMATYPE_F);}
    ;

EndianKeyword
    : PARSEOP_ENDIAN_LITTLE                 {$$ = TrCreateLeafNode (PARSEOP_ENDIAN_LITTLE);}
    | PARSEOP_ENDIAN_BIG                    {$$ = TrCreateLeafNode (PARSEOP_ENDIAN_BIG);}
    ;

FlowControlKeyword
    : PARSEOP_FLOWCONTROL_HW                {$$ = TrCreateLeafNode (PARSEOP_FLOWCONTROL_HW);}
    | PARSEOP_FLOWCONTROL_NONE              {$$ = TrCreateLeafNode (PARSEOP_FLOWCONTROL_NONE);}
    | PARSEOP_FLOWCONTROL_SW                {$$ = TrCreateLeafNode (PARSEOP_FLOWCONTROL_SW);}
    ;

InterruptLevel
    : PARSEOP_INTLEVEL_ACTIVEBOTH           {$$ = TrCreateLeafNode (PARSEOP_INTLEVEL_ACTIVEBOTH);}
    | PARSEOP_INTLEVEL_ACTIVEHIGH           {$$ = TrCreateLeafNode (PARSEOP_INTLEVEL_ACTIVEHIGH);}
    | PARSEOP_INTLEVEL_ACTIVELOW            {$$ = TrCreateLeafNode (PARSEOP_INTLEVEL_ACTIVELOW);}
    ;

InterruptTypeKeyword
    : PARSEOP_INTTYPE_EDGE                  {$$ = TrCreateLeafNode (PARSEOP_INTTYPE_EDGE);}
    | PARSEOP_INTTYPE_LEVEL                 {$$ = TrCreateLeafNode (PARSEOP_INTTYPE_LEVEL);}
    ;

IODecodeKeyword
    : PARSEOP_IODECODETYPE_16               {$$ = TrCreateLeafNode (PARSEOP_IODECODETYPE_16);}
    | PARSEOP_IODECODETYPE_10               {$$ = TrCreateLeafNode (PARSEOP_IODECODETYPE_10);}
    ;

IoRestrictionKeyword
    : PARSEOP_IORESTRICT_IN                 {$$ = TrCreateLeafNode (PARSEOP_IORESTRICT_IN);}
    | PARSEOP_IORESTRICT_OUT                {$$ = TrCreateLeafNode (PARSEOP_IORESTRICT_OUT);}
    | PARSEOP_IORESTRICT_NONE               {$$ = TrCreateLeafNode (PARSEOP_IORESTRICT_NONE);}
    | PARSEOP_IORESTRICT_PRESERVE           {$$ = TrCreateLeafNode (PARSEOP_IORESTRICT_PRESERVE);}
    ;

LockRuleKeyword
    : PARSEOP_LOCKRULE_LOCK                 {$$ = TrCreateLeafNode (PARSEOP_LOCKRULE_LOCK);}
    | PARSEOP_LOCKRULE_NOLOCK               {$$ = TrCreateLeafNode (PARSEOP_LOCKRULE_NOLOCK);}
    ;

MatchOpKeyword
    : PARSEOP_MATCHTYPE_MTR                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MTR);}
    | PARSEOP_MATCHTYPE_MEQ                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MEQ);}
    | PARSEOP_MATCHTYPE_MLE                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MLE);}
    | PARSEOP_MATCHTYPE_MLT                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MLT);}
    | PARSEOP_MATCHTYPE_MGE                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MGE);}
    | PARSEOP_MATCHTYPE_MGT                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MGT);}
    ;

MaxKeyword
    : PARSEOP_MAXTYPE_FIXED                 {$$ = TrCreateLeafNode (PARSEOP_MAXTYPE_FIXED);}
    | PARSEOP_MAXTYPE_NOTFIXED              {$$ = TrCreateLeafNode (PARSEOP_MAXTYPE_NOTFIXED);}
    ;

MemTypeKeyword
    : PARSEOP_MEMTYPE_CACHEABLE             {$$ = TrCreateLeafNode (PARSEOP_MEMTYPE_CACHEABLE);}
    | PARSEOP_MEMTYPE_WRITECOMBINING        {$$ = TrCreateLeafNode (PARSEOP_MEMTYPE_WRITECOMBINING);}
    | PARSEOP_MEMTYPE_PREFETCHABLE          {$$ = TrCreateLeafNode (PARSEOP_MEMTYPE_PREFETCHABLE);}
    | PARSEOP_MEMTYPE_NONCACHEABLE          {$$ = TrCreateLeafNode (PARSEOP_MEMTYPE_NONCACHEABLE);}
    ;

MinKeyword
    : PARSEOP_MINTYPE_FIXED                 {$$ = TrCreateLeafNode (PARSEOP_MINTYPE_FIXED);}
    | PARSEOP_MINTYPE_NOTFIXED              {$$ = TrCreateLeafNode (PARSEOP_MINTYPE_NOTFIXED);}
    ;

ObjectTypeKeyword
    : PARSEOP_OBJECTTYPE_UNK                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_UNK);}
    | PARSEOP_OBJECTTYPE_INT                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_INT);}
    | PARSEOP_OBJECTTYPE_STR                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_STR);}
    | PARSEOP_OBJECTTYPE_BUF                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_BUF);}
    | PARSEOP_OBJECTTYPE_PKG                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_PKG);}
    | PARSEOP_OBJECTTYPE_FLD                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_FLD);}
    | PARSEOP_OBJECTTYPE_DEV                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_DEV);}
    | PARSEOP_OBJECTTYPE_EVT                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_EVT);}
    | PARSEOP_OBJECTTYPE_MTH                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_MTH);}
    | PARSEOP_OBJECTTYPE_MTX                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_MTX);}
    | PARSEOP_OBJECTTYPE_OPR                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_OPR);}
    | PARSEOP_OBJECTTYPE_POW                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_POW);}
    | PARSEOP_OBJECTTYPE_PRO                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_PRO);}
    | PARSEOP_OBJECTTYPE_THZ                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_THZ);}
    | PARSEOP_OBJECTTYPE_BFF                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_BFF);}
    | PARSEOP_OBJECTTYPE_DDB                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_DDB);}
    ;

ParityTypeKeyword
    : PARSEOP_PARITYTYPE_SPACE              {$$ = TrCreateLeafNode (PARSEOP_PARITYTYPE_SPACE);}
    | PARSEOP_PARITYTYPE_MARK               {$$ = TrCreateLeafNode (PARSEOP_PARITYTYPE_MARK);}
    | PARSEOP_PARITYTYPE_ODD                {$$ = TrCreateLeafNode (PARSEOP_PARITYTYPE_ODD);}
    | PARSEOP_PARITYTYPE_EVEN               {$$ = TrCreateLeafNode (PARSEOP_PARITYTYPE_EVEN);}
    | PARSEOP_PARITYTYPE_NONE               {$$ = TrCreateLeafNode (PARSEOP_PARITYTYPE_NONE);}
    ;

PinConfigByte
    : PinConfigKeyword                      {$$ = $1;}
    | ByteConstExpr                         {$$ = UtCheckIntegerRange ($1, 0x80, 0xFF);}
    ;

PinConfigKeyword
    : PARSEOP_PIN_NOPULL                    {$$ = TrCreateLeafNode (PARSEOP_PIN_NOPULL);}
    | PARSEOP_PIN_PULLDOWN                  {$$ = TrCreateLeafNode (PARSEOP_PIN_PULLDOWN);}
    | PARSEOP_PIN_PULLUP                    {$$ = TrCreateLeafNode (PARSEOP_PIN_PULLUP);}
    | PARSEOP_PIN_PULLDEFAULT               {$$ = TrCreateLeafNode (PARSEOP_PIN_PULLDEFAULT);}
    ;

PldKeyword
    : PARSEOP_PLD_REVISION                  {$$ = TrCreateLeafNode (PARSEOP_PLD_REVISION);}
    | PARSEOP_PLD_IGNORECOLOR               {$$ = TrCreateLeafNode (PARSEOP_PLD_IGNORECOLOR);}
    | PARSEOP_PLD_RED                       {$$ = TrCreateLeafNode (PARSEOP_PLD_RED);}
    | PARSEOP_PLD_GREEN                     {$$ = TrCreateLeafNode (PARSEOP_PLD_GREEN);}
    | PARSEOP_PLD_BLUE                      {$$ = TrCreateLeafNode (PARSEOP_PLD_BLUE);}
    | PARSEOP_PLD_WIDTH                     {$$ = TrCreateLeafNode (PARSEOP_PLD_WIDTH);}
    | PARSEOP_PLD_HEIGHT                    {$$ = TrCreateLeafNode (PARSEOP_PLD_HEIGHT);}
    | PARSEOP_PLD_USERVISIBLE               {$$ = TrCreateLeafNode (PARSEOP_PLD_USERVISIBLE);}
    | PARSEOP_PLD_DOCK                      {$$ = TrCreateLeafNode (PARSEOP_PLD_DOCK);}
    | PARSEOP_PLD_LID                       {$$ = TrCreateLeafNode (PARSEOP_PLD_LID);}
    | PARSEOP_PLD_PANEL                     {$$ = TrCreateLeafNode (PARSEOP_PLD_PANEL);}
    | PARSEOP_PLD_VERTICALPOSITION          {$$ = TrCreateLeafNode (PARSEOP_PLD_VERTICALPOSITION);}
    | PARSEOP_PLD_HORIZONTALPOSITION        {$$ = TrCreateLeafNode (PARSEOP_PLD_HORIZONTALPOSITION);}
    | PARSEOP_PLD_SHAPE                     {$$ = TrCreateLeafNode (PARSEOP_PLD_SHAPE);}
    | PARSEOP_PLD_GROUPORIENTATION          {$$ = TrCreateLeafNode (PARSEOP_PLD_GROUPORIENTATION);}
    | PARSEOP_PLD_GROUPTOKEN                {$$ = TrCreateLeafNode (PARSEOP_PLD_GROUPTOKEN);}
    | PARSEOP_PLD_GROUPPOSITION             {$$ = TrCreateLeafNode (PARSEOP_PLD_GROUPPOSITION);}
    | PARSEOP_PLD_BAY                       {$$ = TrCreateLeafNode (PARSEOP_PLD_BAY);}
    | PARSEOP_PLD_EJECTABLE                 {$$ = TrCreateLeafNode (PARSEOP_PLD_EJECTABLE);}
    | PARSEOP_PLD_EJECTREQUIRED             {$$ = TrCreateLeafNode (PARSEOP_PLD_EJECTREQUIRED);}
    | PARSEOP_PLD_CABINETNUMBER             {$$ = TrCreateLeafNode (PARSEOP_PLD_CABINETNUMBER);}
    | PARSEOP_PLD_CARDCAGENUMBER            {$$ = TrCreateLeafNode (PARSEOP_PLD_CARDCAGENUMBER);}
    | PARSEOP_PLD_REFERENCE                 {$$ = TrCreateLeafNode (PARSEOP_PLD_REFERENCE);}
    | PARSEOP_PLD_ROTATION                  {$$ = TrCreateLeafNode (PARSEOP_PLD_ROTATION);}
    | PARSEOP_PLD_ORDER                     {$$ = TrCreateLeafNode (PARSEOP_PLD_ORDER);}
    | PARSEOP_PLD_RESERVED                  {$$ = TrCreateLeafNode (PARSEOP_PLD_RESERVED);}
    | PARSEOP_PLD_VERTICALOFFSET            {$$ = TrCreateLeafNode (PARSEOP_PLD_VERTICALOFFSET);}
    | PARSEOP_PLD_HORIZONTALOFFSET          {$$ = TrCreateLeafNode (PARSEOP_PLD_HORIZONTALOFFSET);}
    ;

RangeTypeKeyword
    : PARSEOP_RANGETYPE_ISAONLY             {$$ = TrCreateLeafNode (PARSEOP_RANGETYPE_ISAONLY);}
    | PARSEOP_RANGETYPE_NONISAONLY          {$$ = TrCreateLeafNode (PARSEOP_RANGETYPE_NONISAONLY);}
    | PARSEOP_RANGETYPE_ENTIRE              {$$ = TrCreateLeafNode (PARSEOP_RANGETYPE_ENTIRE);}
    ;

RegionSpaceKeyword
    : PARSEOP_REGIONSPACE_IO                {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_IO);}
    | PARSEOP_REGIONSPACE_MEM               {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_MEM);}
    | PARSEOP_REGIONSPACE_PCI               {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_PCI);}
    | PARSEOP_REGIONSPACE_EC                {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_EC);}
    | PARSEOP_REGIONSPACE_SMBUS             {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_SMBUS);}
    | PARSEOP_REGIONSPACE_CMOS              {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_CMOS);}
    | PARSEOP_REGIONSPACE_PCIBAR            {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_PCIBAR);}
    | PARSEOP_REGIONSPACE_IPMI              {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_IPMI);}
    | PARSEOP_REGIONSPACE_GPIO              {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_GPIO);}
    | PARSEOP_REGIONSPACE_GSBUS             {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_GSBUS);}
    | PARSEOP_REGIONSPACE_PCC               {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_PCC);}
    | PARSEOP_REGIONSPACE_FFIXEDHW          {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_FFIXEDHW);}
    ;

ResourceTypeKeyword
    : PARSEOP_RESOURCETYPE_CONSUMER         {$$ = TrCreateLeafNode (PARSEOP_RESOURCETYPE_CONSUMER);}
    | PARSEOP_RESOURCETYPE_PRODUCER         {$$ = TrCreateLeafNode (PARSEOP_RESOURCETYPE_PRODUCER);}
    ;

SerializeRuleKeyword
    : PARSEOP_SERIALIZERULE_SERIAL          {$$ = TrCreateLeafNode (PARSEOP_SERIALIZERULE_SERIAL);}
    | PARSEOP_SERIALIZERULE_NOTSERIAL       {$$ = TrCreateLeafNode (PARSEOP_SERIALIZERULE_NOTSERIAL);}
    ;

ShareTypeKeyword
    : PARSEOP_SHARETYPE_SHARED              {$$ = TrCreateLeafNode (PARSEOP_SHARETYPE_SHARED);}
    | PARSEOP_SHARETYPE_EXCLUSIVE           {$$ = TrCreateLeafNode (PARSEOP_SHARETYPE_EXCLUSIVE);}
    | PARSEOP_SHARETYPE_SHAREDWAKE          {$$ = TrCreateLeafNode (PARSEOP_SHARETYPE_SHAREDWAKE);}
    | PARSEOP_SHARETYPE_EXCLUSIVEWAKE       {$$ = TrCreateLeafNode (PARSEOP_SHARETYPE_EXCLUSIVEWAKE);}
   ;

SlaveModeKeyword
    : PARSEOP_SLAVEMODE_CONTROLLERINIT      {$$ = TrCreateLeafNode (PARSEOP_SLAVEMODE_CONTROLLERINIT);}
    | PARSEOP_SLAVEMODE_DEVICEINIT          {$$ = TrCreateLeafNode (PARSEOP_SLAVEMODE_DEVICEINIT);}
    ;

StopBitsKeyword
    : PARSEOP_STOPBITS_TWO                  {$$ = TrCreateLeafNode (PARSEOP_STOPBITS_TWO);}
    | PARSEOP_STOPBITS_ONEPLUSHALF          {$$ = TrCreateLeafNode (PARSEOP_STOPBITS_ONEPLUSHALF);}
    | PARSEOP_STOPBITS_ONE                  {$$ = TrCreateLeafNode (PARSEOP_STOPBITS_ONE);}
    | PARSEOP_STOPBITS_ZERO                 {$$ = TrCreateLeafNode (PARSEOP_STOPBITS_ZERO);}
    ;

TranslationKeyword
    : PARSEOP_TRANSLATIONTYPE_SPARSE        {$$ = TrCreateLeafNode (PARSEOP_TRANSLATIONTYPE_SPARSE);}
    | PARSEOP_TRANSLATIONTYPE_DENSE         {$$ = TrCreateLeafNode (PARSEOP_TRANSLATIONTYPE_DENSE);}
    ;

TypeKeyword
    : PARSEOP_TYPE_TRANSLATION              {$$ = TrCreateLeafNode (PARSEOP_TYPE_TRANSLATION);}
    | PARSEOP_TYPE_STATIC                   {$$ = TrCreateLeafNode (PARSEOP_TYPE_STATIC);}
    ;

UpdateRuleKeyword
    : PARSEOP_UPDATERULE_PRESERVE           {$$ = TrCreateLeafNode (PARSEOP_UPDATERULE_PRESERVE);}
    | PARSEOP_UPDATERULE_ONES               {$$ = TrCreateLeafNode (PARSEOP_UPDATERULE_ONES);}
    | PARSEOP_UPDATERULE_ZEROS              {$$ = TrCreateLeafNode (PARSEOP_UPDATERULE_ZEROS);}
    ;

WireModeKeyword
    : PARSEOP_WIREMODE_FOUR                 {$$ = TrCreateLeafNode (PARSEOP_WIREMODE_FOUR);}
    | PARSEOP_WIREMODE_THREE                {$$ = TrCreateLeafNode (PARSEOP_WIREMODE_THREE);}
    ;

XferSizeKeyword
    : PARSEOP_XFERSIZE_8                    {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_8,   0);}
    | PARSEOP_XFERSIZE_16                   {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_16,  1);}
    | PARSEOP_XFERSIZE_32                   {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_32,  2);}
    | PARSEOP_XFERSIZE_64                   {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_64,  3);}
    | PARSEOP_XFERSIZE_128                  {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_128, 4);}
    | PARSEOP_XFERSIZE_256                  {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_256, 5);}
    ;

XferTypeKeyword
    : PARSEOP_XFERTYPE_8                    {$$ = TrCreateLeafNode (PARSEOP_XFERTYPE_8);}
    | PARSEOP_XFERTYPE_8_16                 {$$ = TrCreateLeafNode (PARSEOP_XFERTYPE_8_16);}
    | PARSEOP_XFERTYPE_16                   {$$ = TrCreateLeafNode (PARSEOP_XFERTYPE_16);}
    ;
