/*
 * Some or all of this work - Copyright (c) 2006 - 2020, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
//
// Grammar.asl - Minimally exercises most ASL constructs
//
// NOTE -- use: iasl -f -of grammar.asl to compile
//
//         This 1) Ignores errors (checks compiler error handling)
//              2) Disables constant folding
//
//

/*******************************************************************************
Compilation should look like this:

C:\acpica\tests\misc>iasl -f -of grammar.asl

Intel ACPI Component Architecture
ASL Optimizing Compiler version 20090422 [Apr 22 2009]
Copyright (C) 2000 - 2009 Intel Corporation
Supports ACPI Specification Revision 3.0a

grammar.asl   187:     Name (_NPK, Package (8)
Warning  1098 -                 ^ Unknown reserved name (_NPK)

grammar.asl   510:     NAME (ESC1, "abcdefg\x00hijklmn")
Warning  1042 -                                ^ Invalid Hex/Octal Escape - Non-ASCII or NULL

grammar.asl   511:     NAME (ESC2, "abcdefg\000hijklmn")
Warning  1042 -                                ^ Invalid Hex/Octal Escape - Non-ASCII or NULL

grammar.asl   601:     Method (RCIV, 1)
Warning  1087 -                   ^ Not all control paths return a value (RCIV)

grammar.asl   608:         RCIV (Subtract (Arg0, 1))
Remark   5073 -               ^ Recursive method call (RCIV)

grammar.asl   937:     Method (_ERR, 2)
Warning  1077 -                   ^ Reserved method has too few arguments (_ERR requires 3)

grammar.asl  1377:         Store (0x1234567887654321, QWD2)
Warning  1032 -                                    ^ 64-bit integer in 32-bit table, truncating

grammar.asl  1379:         if (LNotEqual (Local0, 0x1234567887654321))
Warning  1032 -         64-bit integer in 32-bit table, truncating ^

grammar.asl  1459:         SizeOf (BUFO)
Warning  1105 -                       ^ Result is not used, operator has no effect

grammar.asl  1485:         Acquire (MTX2, 1)
Warning  1104 -                           ^ Possible operator timeout is ignored

grammar.asl  1633:         Add (Local0, Local1)
Warning  1105 -                      ^ Result is not used, operator has no effect

grammar.asl  1804:     Method (COND)
Warning  1087 -                   ^ Not all control paths return a value (COND)

grammar.asl  6010:             Name (_HID, "*PNP0A06")
Error    4001 -                                     ^ String must be entirely alphanumeric (*PNP0A06)

grammar.asl  6461:             Name (_CRS, Buffer(26)  {"\_SB_.PCI2._CRS..........."})
Warning  1038 -        Invalid or unknown escape sequence ^

grammar.asl  6800:                 And (Local0, 1, Local0) //  Local0 &= 1
Error    4050 -                              ^ Method local variable is not initialized (Local0)

grammar.asl  6886:             Name (_HID, "*PNP0C0A")     //  Control Method Battey ID
Error    4001 -                                     ^ String must be entirely alphanumeric (*PNP0C0A)

ASL Input:  grammar.asl - 10254 lines, 322162 bytes, 4810 keywords
AML Output: grammar.aml - 43392 bytes, 669 named objects, 4141 executable opcodes

Compilation complete. 3 Errors, 12 Warnings, 1 Remarks, 1101 Optimizations

***************************************************************************************************/

DefinitionBlock (
    "grammar.aml",      //Output filename
    "DSDT",             //Signature
    0x01,               //DSDT Revision ---> 32-bit table
    "Intel",            //OEMID
    "GRMTEST",          //TABLE ID
    0x20090511          //OEM Revision
    )
{

    External (\ABCD, UnknownObj)


    /* Device with _STA and _INI */

    Device (A1)
    {
        Method (_STA)
        {
            Return (0x0F)
        }

        Method (_INI)
        {
            Return
        }
    }

    /* Device with no _STA, has _INI */

    Device (A2)
    {
        Method (_INI)
        {
            Return
        }
    }

    /* Device with _STA, no _INI */

    Device (A3)
    {
        Method (_STA)
        {
            Return (0x0F)
        }
    }

    /* Device with _STA and _INI, but not present */

    Device (A4)
    {
        Method (_STA)
        {
            Return (Zero)
        }

        Method (_INI)
        {
            Return
        }
    }


    /* Resource descriptors */

    Device (IRES)
    {
        Name (PRT0, ResourceTemplate ()
        {
            IRQ (Edge, ActiveHigh, Exclusive) {3,4,5,6,7,9,10,11,14,15}

            StartDependentFn (1,1)
            {
                IRQNoFlags () {0,1,2}
            }
            EndDependentFn ()
        })

        Method (_CRS, 0, NotSerialized)
        {
            Store ("_CRS:", Debug)
            Store (PRT0, Debug)
            Return (PRT0)
        }

        Method (_SRS, 1, Serialized)
        {
            Store ("_SRS:", Debug)
            Store (Arg0, Debug)
            Return (Zero)
        }
    }

    Name (_NPK, Package ()
    {
        0x1111,
        0x2222,
        0x3333,
        0x4444
    })


    Device (RES)
    {
        Name (_PRT, Package (0x04)
        {
            Package (0x04)
            {
                0x0002FFFF,
                Zero,
                Zero,
                Zero
            },

            Package (0x04)
            {
                0x0002FFFF,
                One,
                Zero,
                Zero
            },

            Package (0x04)
            {
                0x000AFFFF,
                Zero,
                Zero,
                Zero
            },

            Package (0x04)
            {
                0x000BFFFF,
                Zero,
                Zero,
                Zero
            }
        })

        Method (_CRS, 0, Serialized)
        {
            Name (PRT0, ResourceTemplate ()
            {
                WordBusNumber (ResourceConsumer, MinFixed, MaxFixed, SubDecode,
                    0x0000, // Address Space Granularity
                    0xFFF2, // Address Range Minimum
                    0xFFF3, // Address Range Maximum
                    0x0032, // Address Translation Offset
                    0x0002,,,)
                WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,
                    0x0000, // Address Space Granularity
                    0x0000, // Address Range Minimum
                    0x00FF, // Address Range Maximum
                    0x0000, // Address Translation Offset
                    0x0100,,,)
                WordSpace (0xC3, ResourceConsumer, PosDecode, MinFixed, MaxFixed, 0xA5,
                    0x0000, // Address Space Granularity
                    0xA000, // Address Range Minimum
                    0xBFFF, // Address Range Maximum
                    0x0000, // Address Translation Offset
                    0x2000,,,)
                IO (Decode16, 0x0CF8, 0x0CFF, 0x01, 0x08)
                WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x0000, // Address Space Granularity
                    0x0000, // Address Range Minimum
                    0x0CF7, // Address Range Maximum
                    0x0000, // Address Translation Offset
                    0x0CF8,,,
                    , TypeStatic)
                WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x0000, // Address Space Granularity
                    0x0D00, // Address Range Minimum
                    0xFFFF, // Address Range Maximum
                    0x0000, // Address Translation Offset
                    0xF300,,,
                    , TypeStatic)
                DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x00000000, // Address Space Granularity
                    0x00000000, // Address Range Minimum
                    0x00000CF7, // Address Range Maximum
                    0x00000000, // Address Translation Offset
                    0x00000CF8,,,
                    , TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000, // Address Space Granularity
                    0x000C8000, // Address Range Minimum
                    0x000EFFFF, // Address Range Maximum
                    0x00000000, // Address Translation Offset
                    0x00028000,,,
                    , AddressRangeMemory, TypeStatic)
                DWordSpace (0xC3, ResourceConsumer, PosDecode, MinFixed, MaxFixed, 0xA5,
                    0x00000000, // Address Space Granularity
                    0x000C8000, // Address Range Minimum
                    0x000EFFFF, // Address Range Maximum
                    0x00000000, // Address Translation Offset
                    0x00028000,,,)
                QWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x0000000000000000, // Address Space Granularity
                    0x0000000000000000, // Address Range Minimum
                    0x0000000000000CF7, // Address Range Maximum
                    0x0000000000000000, // Address Translation Offset
                    0x0000000000000CF8, 0x44, "This is a ResouceSource string",
                    , TypeStatic)
                QWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x0000000000000000, // Address Space Granularity
                    0x0000000000000000, // Address Range Minimum
                    0x0000000000000CF7, // Address Range Maximum
                    0x0000000000000000, // Address Translation Offset
                    0x0000000000000CF8,,,
                    , TypeStatic)
                QWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x0000000000000000, // Address Space Granularity
                    0x0000000000100000, // Address Range Minimum
                    0x00000000FFDFFFFF, // Address Range Maximum
                    0x0000000000000000, // Address Translation Offset
                    0x00000000FFD00000,,,
                    , AddressRangeMemory, TypeStatic)
                QWordSpace (0xC3, ResourceConsumer, PosDecode, MinFixed, MaxFixed, 0xA5,
                    0x0000000000000000, // Address Space Granularity
                    0x0000000000000000, // Address Range Minimum
                    0x0000000000000CF7, // Address Range Maximum
                    0x0000000000000000, // Address Translation Offset
                    0x0000000000000CF8,,,)
                ExtendedIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x0000000000000000, // Address Space Granularity
                    0x0000000000000000, // Address Range Minimum
                    0x0000000000000CF7, // Address Range Maximum
                    0x0000000000000000, // Address Translation Offset
                    0x0000000000000CF8, // Address Length
                    0x0000000000000000, // Type Specific Attributes
                    , TypeStatic)
                ExtendedMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x0000000000000000, // Address Space Granularity
                    0x0000000000100000, // Address Range Minimum
                    0x00000000FFDFFFFF, // Address Range Maximum
                    0x0000000000000000, // Address Translation Offset
                    0x00000000FFD00000, // Address Length
                    0x0000000000000000, // Type Specific Attributes
                    , AddressRangeMemory, TypeStatic)
                ExtendedSpace (0xC3, ResourceProducer, PosDecode, MinFixed, MaxFixed, 0xA3,
                    0x0000000000000000, // Address Space Granularity
                    0x0000000000100000, // Address Range Minimum
                    0x00000000FFDFFFFF, // Address Range Maximum
                    0x0000000000000000, // Address Translation Offset
                    0x00000000FFD00000, // Address Length
                    0x0000000000000000) // Type Specific Attributes
                IO (Decode16, 0x0010, 0x0020, 0x01, 0x10)
                IO (Decode16, 0x0090, 0x00A0, 0x01, 0x10)
                FixedIO (0x0061, 0x01)
                IRQNoFlags () {2}
                DMA (Compatibility, BusMaster, Transfer8_16) {4}
                DMA (Compatibility, BusMaster, Transfer8) {2,5,7}
                Memory32Fixed (ReadWrite, 0x00100000, 0x00000000)
                Memory32Fixed (ReadOnly, 0xFFFE0000, 0x00020000)
                Memory32 (ReadOnly, 0x00020000, 0xFFFE0000, 0x00000004, 0x00000200)
                Memory24 (ReadOnly, 0x1111, 0x2222, 0x0004, 0x0200)
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive, 0xE, "\\_SB_.TEST")
                {
                    0x00000E01,
                }
                Interrupt (ResourceConsumer, Edge, ActiveHigh, Exclusive, 0x6, "xxxx")
                {
                    0x00000601,
                    0x00000003,
                    0x00000002,
                    0x00000001,
                }
                Interrupt (ResourceProducer, Edge, ActiveHigh, Exclusive)
                {
                    0xFFFF0000,
                    0x00000003,
                    0x00000002,
                    0x00000001,
                    0x00000005,
                    0x00000007,
                    0x00000009,
                }
                VendorShort () {0x01, 0x02, 0x03}
                VendorLong ()
                {
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x09
                }
                Register (SystemIO, 0x08, 0x00, 0x00000000000000B2, , R000)
                Register (SystemMemory, 0x08, 0x00, 0x00000000000000B2)
                StartDependentFnNoPri ()
                {
                    IRQNoFlags () {0,1,2}
                    IRQ (Level, ActiveLow, Shared) {3,4,5,6,7,9,10,11,14,15}
                }
                EndDependentFn ()
            })
            CreateWordField (PRT0, 0x08, BMIN)
            CreateByteField (PRT0, R000._ASZ, RSIZ)
            Store (0x03, BMIN)
            Return (PRT0)
        }

        Method (_PRS, 0, Serialized)
        {
            Name (BUF0, ResourceTemplate ()
            {
                StartDependentFn (0x01, 0x02)
                {
                    IO (Decode16, 0x03D8, 0x03F8, 0x01, 0x08)
                    IRQNoFlags () {4}
                }
                StartDependentFn (0x02, 0x01)
                {
                    IO (Decode16, 0x03D8, 0x03E8, 0x01, 0x08)
                    IRQNoFlags () {4}
                }
                StartDependentFn (0x00, 0x02)
                {
                    IO (Decode16, 0x02E8, 0x02F8, 0x01, 0x08)
                    IRQNoFlags () {3}
                }
                StartDependentFn (0x00, 0x02)
                {
                    IO (Decode16, 0x02D8, 0x02E8, 0x01, 0x08)
                    IRQNoFlags () {3}
                }
                StartDependentFn (0x02, 0x00)
                {
                    IO (Decode16, 0x0100, 0x03F8, 0x08, 0x08)
                    IRQNoFlags () {1,3,4,5,6,7,8,10,11,12,13,14,15}
                }
                EndDependentFn ()
            })
            Return (BUF0)
        }

        Method (_SRS, 1, Serialized)
        {
            Return (Zero)
        }
    }


    Name(\_S0,Package(0x04){
        0x00,
        0x00,
        0x00,
        0x00
    })
    Name(\_S3,Package(0x04){
        0x05,
        0x05,
        0x00,
        0x00
    })
    Name(\_S4,Package(0x04){
        0x06,
        0x06,
        0x00,
        0x00
    })
    Name(\_S5,Package(0x04){
        0x07,
        0x07,
        0x00,
        0x00
    })

/* Examine this table header (DSDT) */

/*
    DataTableRegion (HDR, "DSDT", "", "")
    Field (HDR, AnyAcc, NoLock, Preserve)
    {
        SIG,  32,
        LENG, 32,
        REV,  8,
        SUM,  8,
        OID,  48,
        OTID, 64,
        OREV, 32,
        CID,  32,
        CREV, 32
    }

    Method (SIZE)
    {
        If (LLess (REV, 2))
        {
            Store ("32-bit table", Debug)
        }
        else
        {
            Store ("64-bit table", Debug)
        }
        Return (0)
    }

*/
    Name (SIZE, 0)

    /* Custom operation region */

    OperationRegion(MYOP,0x80,0xFD60,0x6)
    Field(MYOP,ByteAcc,NoLock,Preserve)
    {
        MFLD,8
    }

    Method (TCOP,, Serialized)
    {
        Name (_STR, Unicode ("test"))
        Store (4, MFLD)
        Store (MFLD, Local0)
    }

    Name (ERRS, 0x0)

    /* Warning should be issued for premature string termination */

    NAME (ESC1, "abcdefg\x00hijklmn")
    NAME (ESC2, "abcdefg\000hijklmn")
    Name (ESC3, "abc\a\bdef\f\n\r\t\v\x03ffff\432")


    Name(CRSA,ResourceTemplate()
    {
        WORDBusNumber(ResourceProducer,MinFixed,MaxFixed,PosDecode,0x0000,0x0019,0x001D,0x0000,0x0005)
        WORDIO(ResourceProducer,MinFixed,MaxFixed,PosDecode,NonISAOnlyRanges,0x0000,0xC000,0xCFFF,0x0000,0x1000)
        DWORDMemory(ResourceProducer,PosDecode,MinFixed,MaxFixed,NonCacheable,ReadWrite,0x00000000,0xD8000000,0xDBFFFFFF,0x00000000,0x04000000)

    })
    Name(CRSB,ResourceTemplate()
    {
        DWORDMemory(ResourceProducer,PosDecode,MinFixed,MaxFixed,NonCacheable,ReadWrite,0x00000000,0xD8000000,0xDBFFFFFF,0x00000000,0x04000000)

    })

    Name(CRSC,ResourceTemplate()
    {
        VendorShort () {0x1, 0x2, 0x3}
    })
    Name(CRSD,ResourceTemplate()
    {
        VendorLong (VNDL) {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9}
    })

    Name(CRSE,ResourceTemplate()
    {
        IRQNoFlags(){3,4,10,11}
        IRQNoFlags(xxxt){3,4,10,11}
    })
    Name(CRSR, Buffer (Add (SizeOf(CRSA),SizeOf(CRSB))){})
    Method(_CRS,0,NotSerialized)
    {
        Return(CRSR)
    }


    //
    // Unnamed scope
    //
    Scope (\)
    {
        Name(Bxxx,0xFFFFFFFF)
    }

    Name (LANS, 0x0)

    PowerResource(LANP,1,0)
    {
        Method(_STA){
            If(LEqual(And(LANS,0x30),0x30)){
                Return(One)
            } Else {
                Return(Zero)
            }
        }
        Method(_ON){
            If(LNot(_STA())){
                Store (0x30, LANS)
            }
        }
        Method(_OFF){
            If(_STA()){
                Store (0, LANS)
            }
        }
    }


    /* Can a method define another method? */

    /**********************************
    Method (TASK, 2, SERIALIZED)
    {
        Sleep (100)

        Method (TAS2)
        {
            Sleep (100)
        }

        TAS2()
        Return

    }
    ************************************/

    /* A recursive method */

    Method (RCIV, 1)
    {
        Store (Arg0, Debug)
        If (Lequal (Arg0, 0))
        {
            Return ()
        }
        RCIV (Subtract (Arg0, 1))
    }

    Method (RTOP)
    {
        RCIV (100)
    }


    Scope(\_PR)
    {
        Processor(CPU0,0x0,0xFFFFFFFF,0x0) {}
    }

    Name(B1TP,0xFFFFFFFF)

    Name(B2TP,0xFFFFFFFF)
    Name(ADPS,0xFFFFFFFF)
    Name(B1PS,0xFFFFFFFF)
    Name(B1RS,0xFFFFFFFF)
    Name(B1CS,0xFFFFFFFF)
    Name(B2PS,0xFFFFFFFF)
    Name(B2RS,0xFFFFFFFF)
    Name(B2CS,0xFFFFFFFF)
    Name(B1DC,3000)
    Name(B2DC,2600)
    Name(B1LF,3000)
    Name(B2LF,2600)
    Name(BPIF,0)
    Name(PBLL,0)

    Name(RBIF,Package()
    {
        0x1,
        2200,
        2200,
        0x1,
        10800,
        0,
        0,
        1,
        1,
        "CA54200-5003/5",
        "1",
        "LION",
        "Fujitsu"
    })

    Method(SMWE, 4)
    {
       return(ONES)
    }

    Method(SMRE, 4)
    {
       return(ONES)
    }

/*
    Method(RDBT,0,Serialized){
        If(LNot(SMWE(0x09,0x15,1,1))){
                    Store(0x18,Local2)
            }
    }
*/
    Scope(_SB)
    {

        Name (SBUF, Buffer (128) {})

        CreateBitField (SBUF, 3, BITY)
        CreateByteField (SBUF, 1, BYTY)
        CreateWordField (SBUF, 2, WRDZ)
        CreateDwordField (SBUF, 4, DWDZ)
        CreateQwordField (SBUF, 8, QWDZ)
        CreateField (SBUF, 128, 12, FLDZ)
        CreateField (SBUF, 148, 96, FLDY)
        CreateField (SBUF, 148, 96, \_SB_.FLDW)

        Method (_INI)
        {
            CreateField (\_SB_.SBUF, 148, 96, FLDV)
        }


        Device(PCI0)
        {
            Name(_HID,EISAID("PNP0A03"))
            Name(_ADR,0x0)

            Method(_CRS,, Serialized)
            {
                Name(PRT0, ResourceTemplate() {
                    WORDBusNumber(                          // Bus number resource(0)
                            ResourceConsumer,               // bit 0 of general flags is 1
                            MinFixed,                       // Range is notfixed
                            MaxFixed,                       // Range is not fixed
                            SubDecode,                      // SubDecode
                            0x0000,                           // Granularity
                            0xfff1,                           // Min
                            0xfff2,                           // Max
                            0x0032,                           // Translation
                            0x0002,,,                         // Range Length
                            BUS0
                    ) } )// PRT0

                CreateWordField(PRT0, BUS0._MIN, BMIN)          //Minimum bus number supported under this bridge.

                Store(3, BMIN)
                Return(PRT0)

            } // _CRS

            Method(_SRS)
            {
                Return ()
            }

            Device(EIO)
            {
                OperationRegion(FJIO,SystemIO,0xFD60,0x6)
                    Field(FJIO,ByteAcc,NoLock,Preserve)
                    {
                        GIDX,8,

                        GDTA,8,

                        PIDX,8,

                        PDTA,8,

                        SIDX,8,

                        SDTA,8
                    }
                    IndexField(GIDX,GDTA,ByteAcc,NoLock,Preserve)
                    {
                        Offset(0x2),
                         ,5,
                        VGAS,2,
                        Offset(0x4),
                         ,4,
                        DCKE,1,
                        Offset(0x5),
                         ,6,
                        ACPW,1,

                        Offset(0xA),
                        B1P,1,

                        B2P,1,

                        B1C,1,

                        B2C,1,

                        B1ER,1,

                        B2ER,1,

                        Offset(0xB),
                        B1CP,8,

                        B2CP,8,

                        BCP,8,

                        B1VH,8,

                        B1VL,8,

                        B2VH,8,

                        B2VL,8,

                        B1TM,8,

                        B2TM,8,

                        B1CH,8,

                        B1CL,8,

                        B2CH,8,

                        B2CL,8
                    }
                }
            }
        }

        Method(RDBT,3,Serialized){
            Store(0x1FFF,Local1)
            If( Arg0 ){
                Store(0x2FFF,Local1)
            }
            Store(0x18,Local2)
            If( Arg1 ){
                Store(0x10,Local2)
            }
            If(LNot(SMRE(0x09,0x15,1,RefOf(Local0)))){
                If(LNot(SMWE(0x08,0x14,1,Local1))){
                    If(LNot(SMRE(0x09,0x17,Local2,RefOf(Local3)))){
                        Store(Local1,Arg2)
                    }
                }
                Or(Local0,0xFFF,Local0)
                SMWE(0x08,0x14,1,Local0)
            }
        }
        Method(MKWD,2)
        {
            If(And(Arg1,0x80)) {
                Or(0xFFFF0000,Arg0,Local0)
                Or(Local0,ShiftLeft(Arg1,8),Local0)
                Subtract(Zero,Local0,Local0)
            } else {
                Store(Arg0,Local0)
                Or(Local0,ShiftLeft(Arg1,8),Local0)
            }
            Return(Local0)
        }

        Device(CMB1)
        {
            Name(_HID,EISAID("PNP0C0A"))
            Name(_UID,0x1)
            Alias(\_SB.PCI0.EIO.B1P,\_SB_.PCI0.XXXX)
            Alias(\_SB.PCI0.EIO.B1P,B1P)
            Alias(\_SB.PCI0.EIO.B1C,B1C)
            Alias(\_SB.PCI0.EIO.B1CH,B1CH)
            Alias(\_SB.PCI0.EIO.B1CL,B1CL)
            Alias(\_SB.PCI0.EIO.B1VH,B1VH)
            Alias(\_SB.PCI0.EIO.B1VL,B1VL)
            Alias(\_SB.PCI0.EIO.B1CP,B1CP)

            Method(_INI)
                        {
                Store(B1P, B1PS)
                Store(B1CP,B1RS)
                Store(B1C, B1CS)
            }

            Method(_BIF){
                RDBT(Zero,Zero,RefOf(B1DC))
                RDBT(Zero,One,RefOf(B1LF))
                Store(B1DC,Index(RBIF,1))
                Store(B1LF,Index(RBIF,2))
                Store("CA54200-5003/5",Index(RBIF,9))
                Store("1",Index(RBIF,10))
                Return(RBIF)
            }

            Method(_BST,, Serialized) {

                _INI()

                Store(Zero,Local0)

                if (LAnd(B1P,LNot(B1C))){
                    Or(Local0,1,Local0)
                }

                if (LAnd(B1P,B1C)) {
                    Or(Local0,2,Local0)
                }

                if (LLessEqual(B1CP,1)) {
                    Or(Local0,4,Local0)
                }

                Store(MKWD(B1CL,B1CH),Local1)

                Store(Divide(Add(Multiply(B1CP,B1LF),99),100),Local2)

                Store(MKWD(B1VL,B1VH),Local3)

                Name(STAT,Package(4){})
                Store(Local0,Index(STAT,0))
                Store(Local1,Index(STAT,1))
                Store(Local2,Index(STAT,2))
                Store(Local3,Index(STAT,3))

                If(LNot(BPIF)){
//                    \_SB.PCI0.EIO.EC0.IECT()
//                    \_SB.PCI0.EIO.EC0.SECT()
                    Store(One,BPIF)
                }
                return(STAT)
            }

        }

    Device (DEV1)
    {
    }

    Scope(\_TZ)
    {
        ThermalZone(TZ1)
        {
            Name(_PSL,Package()
            {
                \_PR.CPU0
            })
        }
    }

    Method (TZ2, 0, SERIALIZED)
    {
        Name(_PSL,Package()
        {
            \_PR.CPU0
        })

        Return (_PSL)
    }

    ThermalZone (THM1)
    {
    }

    Method (NOTI)
    {
        Notify (\DEV1, 0)
        Notify (\THM1, 0)
        Notify (\_PR.CPU0, 0)
    }

    Method (_ERR, 3)
    {
        Increment (ERRS)
        Store ("Run-time exception:", Debug)
        Store (Arg0, Debug)
        Store (Arg1, Debug)

        Return (0)          // Map error to AE_OK
    }

    Method (DIV0)
    {
        Store (1, Local0)
        Store (0, Local1)
        Divide (Local0, Local1, Local3)

        Store ("DIV0 - noabort", Debug)
    }

    Method (ERR_, 2)
    {
        Local0 = ToDecimalString (Arg1)
        if (LEqual (Arg0, 0))
        {
            Printf ("+*+*+*+* MTHD_ERROR at line %o: Results not equal!", Local0)
        }
        if (LEqual (Arg0, 1))
        {
            Printf ("+*+*+*+* MTHD_ERROR at line %o: Numeric result is incorrect!", Local0)
        }
        if (LEqual (Arg0, 2))
        {
            Printf ("+*+*+*+* MTHD_ERROR at line %o: Operand was clobbered!", Local0)
        }

        Notify (DEV1, Arg0)
        Increment (ERRS)
    }

    Method (R226, 2)
    {
    }
    Method (R225, 2)
    {
        R226 (Arg0, Arg1)
    }
    Method (R224, 2)
    {
        R225 (Arg1, Arg0)
    }
    Method (R223, 2)
    {
        R224 (Arg0, Arg1)
    }
    Method (R222, 2)
    {
        R223 (Arg1, Arg0)
    }
    Method (R111)
    {
        Store (0x01010101, Local0)
        R222 (0xABAB, Local0)
        Store (Local0, Local1)
    }

    Method (MAIN)
    {

//      SIZE()
        Store (NUM1(), Local0)
        \CMB1._BST()
        RDBT(1,2,3)
        OBJ1(1)
        OBJ2(2)
        CHEK()
        RETZ()
        BITZ()
        LOGS()
        REFS()
        COND()
        TZ2()

        //
        // iPCO tests added
        //
        Store (\IFEL.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\NOSV.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\IDXF.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\_SB_.NSTL.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\RTBF.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\_SB_.RTLV.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\_SB_.RETP.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\WHLR.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\ANDO.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\BRKP.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\ADSU.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\INDC.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\LOPS.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\FDSO.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\MLDV.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\NBIT.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\SHFT.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\XORD.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\CRBF.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\IDX4.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\EVNT.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\SZLV.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\_SB_.BYTF.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\DWDF.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\DVAX.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\IDX6.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\IDX5.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\_SB_.IDX0.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\_SB_.IDX3.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\IDX7.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\MTCH.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\WHLB.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\_SB_.IDX2.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\SIZO.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        Store (\_SB_.SMIS.TEST(), Local0)
        if (LGreater (Local0, 0))
        {
            ERR_ (1, __LINE__)
            Return(Local0)
        }

        if (LGreater (ERRS, 0))
        {
            Store ("****** There were errors during the execution of the test ******", Debug)
        }

        // Flush all notifies

        Sleep (250)

        //
        // Last Test
        //

        Return(0) // Success
    }


    Method (OBJ1, 1, SERIALIZED)
    {

        Store (3, Local0)
        Name(BUFR, Buffer (Local0) {})
        Name(BUF1, Buffer (4) {1,2,3,4})
        Name(BUF2, Buffer (4) {})

        Store (BUF1, BUF2)
        Mutex (MTX1, 4)

        Alias (MTX1, MTX2)
    }


    Mutex (MTXT, 0)
    Mutex (MTXX, 0)

    /*
     * Field Creation
     */

    Method (FLDS,, Serialized)
    {
        Store ("++++++++ Creating BufferFields", Debug)
        Name (BUF2, Buffer (128) {})

        CreateBitField (BUF2, 3, BIT2)
        CreateByteField (BUF2, 1, BYT2)
        CreateWordField (BUF2, 2, WRD2)
        CreateDwordField (BUF2, 4, DWD2)
        CreateQwordField (BUF2, 8, QWD2)
        CreateField (BUF2, 128, 12, FLD2)
        CreateField (BUF2, 148, 96, FLD3)

        Store (0x1, BIT2)
        Store (BIT2, Local0)
        if (LNotEqual (Local0, 0x1))
        {
            ERR_ (1, __LINE__)
        }
        else
        {
            Store (DerefOf (Index (BUF2, 0)), Local0)
            if (LNotEqual (Local0, 0x08))
            {
                ERR_ (1, __LINE__)
            }
            else
            {
                Store ("++++++++ Bit BufferField I/O PASS", Debug)
            }
        }

        Store (0x1A, BYT2)
        Store (BYT2, Local0)
        if (LNotEqual (Local0, 0x1A))
        {
            ERR_ (1, __LINE__)
        }
        else
        {
            Store ("++++++++ Byte BufferField I/O PASS", Debug)
        }

        Store (0x1234, WRD2)
        Store (WRD2, Local0)
        if (LNotEqual (Local0, 0x1234))
        {
            ERR_ (1, __LINE__)
        }
        else
        {
            Store ("++++++++ Word BufferField I/O PASS", Debug)
        }

        Store (0x123, FLD2)
        Store (FLD2, Local0)
        if (LNotEqual (Local0, 0x123))
        {
            ERR_ (1, __LINE__)
        }
        else
        {
            Store ("++++++++ 12-bit BufferField I/O PASS", Debug)
        }

        Store (0x12345678, DWD2)
        Store (DWD2, Local0)
        if (LNotEqual (Local0, 0x12345678))
        {
            ERR_ (1, __LINE__)
        }
        else
        {
            Store ("++++++++ Dword BufferField I/O PASS", Debug)
        }

        Store (0x1234567887654321, QWD2)
        Store (QWD2, Local0)
        if (LNotEqual (Local0, 0x1234567887654321))
        {
            ERR_ (1, __LINE__)
        }
        else
        {
            Store ("++++++++ Qword BufferField I/O PASS", Debug)
        }
    }


    /* Field execution */

    Method (FLDX,, Serialized)
    {
        Field (\_SB_.MEM.SMEM, AnyAcc, NoLock, Preserve)
        {   //  Field:  SMEM overlay using 32-bit field elements
            SMD0,   32, //  32-bits
            SMD1,   32,     //  32-bits
            SMD2,   32,     //  32-bits
            SMD3,   32  //  32-bits
        }   //  Field:  SMEM overlay using 32-bit field elements
        Field (\_SB_.MEM.SMEM, AnyAcc, NoLock, Preserve)
        {   //  Field:  SMEM overlay using greater than 32-bit field elements
            SME0,   69, //  larger than an integer (32 or 64)
            SME1,   97  //  larger than an integer
        }   //  Field:  SMEM overlay using greater than 32-bit field elements
    }


    Method (MTX_, )
    {
        /* Test "Force release" of mutex on method exit */

        Acquire (MTXT, 0xFFFF)
        Acquire (MTXX, 0xFFFF)

        Store ("++++++++ Acquiring Mutex MTX2", Debug)
        Acquire (_GL_, 0xFFFF)


        Store ("++++++++ Releasing Mutex MTX2", Debug)
        Release (_GL_)
    }


    Method (OBJ2, 1, Serialized)
    {
        Store ("++++++++ Creating Buffer BUFO", Debug)
        Name (BUFO, Buffer (32) {})

        Store ("++++++++ Creating OpRegion OPR2", Debug)
        OperationRegion (OPR2, SystemMemory, Arg0, 256)

        Store ("++++++++ Creating Field(s) in OpRegion OPR2", Debug)
        Field (OPR2, ByteAcc, NoLock, Preserve)
        {
            IDX2, 8,
            DAT2, 8,
            BNK2, 4
        }

        Store ("++++++++ Creating BankField BNK2 in OpRegion OPR2", Debug)
        //
        // mcw 3/20/00 - changed FET0, 4, FET1, 3 to FET0, 1, FET1, 1
        //
        BankField (OPR2, BNK2, 0, ByteAcc, NoLock, Preserve)
        {
            FET0, 4,
            FET1, 3
        }

        Store ("++++++++ Creating IndexField", Debug)
        IndexField (IDX2, DAT2, ByteAcc, NoLock, Preserve)
        {
            FET2, 4,
            FET3, 3
        }

        Store ("++++++++ SizeOf (BUFO)", Debug)
        SizeOf (BUFO)

        Store ("++++++++ Store (SizeOf (BUFO), Local0)", Debug)
        Store (SizeOf (BUFO), Local0)

        Store ("++++++++ Concatenate (\"abd\", \"def\", Local0)", Debug)
        Concatenate ("abd", "def", Local0)
        Store (Local0, Debug)

        Store ("++++++++ Concatenate (\"abd\", 0x7B, Local0)", Debug)
        Concatenate ("abd", 0x7B, Local0)
        Store (Local0, Debug)

        Store ("++++++++ Creating Event EVT2", Debug)
        Event (EVT2)

        Store ("++++++++ Creating Mutex MTX2", Debug)
        Mutex (MTX2, 0)

        Store ("++++++++ Creating Alias MTXA to MTX2", Debug)
        Alias (MTX2, MTXA)

        Store ("++++++++ Acquiring Mutex MTX2", Debug)
        Acquire (MTX2, 0xFFFF)

        Store ("++++++++ Acquiring Mutex MTX2 (2nd acquire)", Debug)
        Acquire (MTX2, 1)

        Store ("++++++++ Releasing Mutex MTX2", Debug)
        Release (MTX2)

        // Type 1 opcodes

        Store ("++++++++ Signalling Event EVT2", Debug)
        Signal (EVT2)

        Store ("++++++++ Resetting Event EVT2", Debug)
        Reset (EVT2)

        Store ("++++++++ Signalling Event EVT2", Debug)
        Signal (EVT2)

        Store ("++++++++ Waiting Event EVT2", Debug)
        Wait (EVT2, 0xFFFF)

        Store ("++++++++ Sleep", Debug)
        Sleep (100)

        Store ("++++++++ Stall", Debug)
        Stall (254)

        Store ("++++++++ NoOperation", Debug)
        Noop

        // Type 2 Opcodes

        Store ("++++++++ Return from Method OBJ2", Debug)
        return (4)
    }


    Method (NUM1, 0)
    {
        /* ADD */

        Store ("++++++++ Add (0x12345678, 0x11111111, Local0)", Debug)
        Add (0x12345678, 0x11111111, Local0)

        Store ("++++++++ Store (Add (0x12345678, 0x11111111), Local1)", Debug)
        Store (Add (0x12345678, 0x11111111), Local1)

        Store ("++++++++ Checking result from ADD", Debug)
        if (LNotEqual (Local0, Local1))
        {
            ERR_ (0, __LINE__)
        }


        /* SUBTRACT */

        Store ("++++++++ Subtract (0x87654321, 0x11111111, Local4)", Debug)
        Subtract (0x87654321, 0x11111111, Local4)

        Store ("++++++++ Store (Subtract (0x87654321, 0x11111111), Local5)", Debug)
        Store (Subtract (0x87654321, 0x11111111), Local5)

        Store ("++++++++ Checking result from SUBTRACT", Debug)
        if (LNotEqual (Local4, Local5))
        {
            ERR_ (0, __LINE__)
        }


        /* MULTIPLY */

        Store ("++++++++ Multiply (33, 10, Local6)", Debug)
        Multiply (33, 10, Local6)

        Store ("++++++++ Store (Multiply (33, 10), Local7)", Debug)
        Store (Multiply (33, 10), Local7)


        Store ("++++++++ Checking result from MULTIPLY", Debug)
        if (LNotEqual (Local6, Local7))
        {
            ERR_ (0, __LINE__)
        }


        /* DIVIDE */

        Store ("++++++++ Divide (100, 9, Local1, Local2)", Debug)
        Divide (100, 9, Local1, Local2)

        Store ("++++++++ Store (Divide (100, 9), Local3)", Debug)
        Store (Divide (100, 9), Local3)

        Store ("++++++++ Checking (quotient) result from DIVIDE", Debug)
        if (LNotEqual (Local2, Local3))
        {
            ERR_ (0, __LINE__)
        }


        /* INCREMENT */

        Store ("++++++++ Increment (Local0)", Debug)
        Store (1, Local0)
        Store (2, Local1)
        Increment (Local0)

        Store ("++++++++ Checking result from INCREMENT", Debug)
        if (LNotEqual (Local0, Local1))
        {
            ERR_ (0, __LINE__)
        }


        /* DECREMENT */

        Store ("++++++++ Decrement (Local0)", Debug)
        Store (2, Local0)
        Store (1, Local1)
        Decrement (Local0)

        Store ("++++++++ Checking result from DECREMENT", Debug)
        if (LNotEqual (Local0, Local1))
        {
            ERR_ (0, __LINE__)
        }


        /* TOBCD */
        /* FROMBCD */

        Store ("++++++++ ToBCD (0x1234, Local5)", Debug)
        ToBCD (0x1234, Local5)

        Store ("++++++++ FromBCD (Local5, Local6)", Debug)
        FromBCD (Local5, Local6)

        Store ("++++++++ Return (Local6)", Debug)
        Return (Local6)
    }


    Method (CHEK)
    {

        Store (3, Local0)
        Store (3, Debug)
        Store (Local0, Debug)
        Store (7, Local1)

        Add (Local0, Local1)
        if (LNotEqual (Local0, 3))
        {
            ERR_ (2, __LINE__)
        }
        if (LNotEqual (Local1, 7))
        {
            ERR_ (2, __LINE__)
        }


        Add (Local0, Local1, Local2)
        if (LNotEqual (Local0, 3))
        {
            ERR_ (2, __LINE__)
        }
        if (LNotEqual (Local1, 7))
        {
            ERR_ (2, __LINE__)
        }
    }


    Method (RET1)
    {
        Store (3, Local0)
        Return (Local0)
    }

    Method (RET2)
    {
        Return (RET1())
    }

    Method (RETZ)
    {
        RET2 ()
    }


    Method (BITZ)
    {
        Store ("++++++++ FindSetLeftBit (0x00100100, Local0)", Debug)
        FindSetLeftBit (0x00100100, Local0)
        if (LNotEqual (Local0, 21))
        {
            ERR_ (1, __LINE__)
        }

        Store ("++++++++ FindSetRightBit (0x00100100, Local1)", Debug)
        FindSetRightBit (0x00100100, Local1)
        if (LNotEqual (Local1, 9))
        {
            ERR_ (1, __LINE__)
        }

        Store ("++++++++ And (0xF0F0F0F0, 0x11111111, Local2)", Debug)
        And (0xF0F0F0F0, 0x11111111, Local2)
        if (LNotEqual (Local2, 0x10101010))
        {
            ERR_ (1, __LINE__)
        }

        Store ("++++++++ NAnd (0xF0F0F0F0, 0x11111111, Local3)", Debug)
        NAnd (0xF0F0F0F0, 0x11111111, Local3)
        if (LNotEqual (Local3, 0xEFEFEFEF))
        {
            ERR_ (1, __LINE__)
        }

        Store ("++++++++ Or (0x11111111, 0x22222222, Local4)", Debug)
        Or (0x11111111, 0x22222222, Local4)
        if (LNotEqual (Local4, 0x33333333))
        {
            ERR_ (1, __LINE__)
        }

        Store ("++++++++ NOr (0x11111111, 0x22222222, Local5)", Debug)
        NOr (0x11111111, 0x22222222, Local5)
        if (LNotEqual (Local5, 0xCCCCCCCC))
        {
            ERR_ (1, __LINE__)
        }

        Store ("++++++++ XOr (0x11113333, 0x22222222, Local6)", Debug)
        XOr (0x11113333, 0x22222222, Local6)
        if (LNotEqual (Local6, 0x33331111))
        {
            ERR_ (1, __LINE__)
        }

        Store ("++++++++ ShiftLeft (0x11112222, 2, Local7)", Debug)
        ShiftLeft (0x11112222, 2, Local7)
        if (LNotEqual (Local7, 0x44448888))
        {
            ERR_ (1, __LINE__)
        }

        Store ("++++++++ ShiftRight (Local7, 2, Local7)", Debug)
        ShiftRight (Local7, 2, Local7)
        if (LNotEqual (Local7, 0x11112222))
        {
            ERR_ (1, __LINE__)
        }


        Store ("++++++++ Not (Local0, Local1)", Debug)
        Store (0x22224444, Local0)
        Not (Local0, Local1)
        if (LNotEqual (Local0, 0x22224444))
        {
            ERR_ (2, __LINE__)
        }

        if (LNotEqual (Local1, 0xDDDDBBBB))
        {
            ERR_ (1, __LINE__)
        }

        Return (Local7)
    }


    Method (LOGS)
    {

        Store ("++++++++ Store (LAnd (0xFFFFFFFF, 0x11111111), Local0)", Debug)
        Store (LAnd (0xFFFFFFFF, 0x11111111), Local0)

        Store ("++++++++ Store (LEqual (0xFFFFFFFF, 0x11111111), Local)", Debug)
        Store (LEqual (0xFFFFFFFF, 0x11111111), Local1)

        Store ("++++++++ Store (LGreater (0xFFFFFFFF, 0x11111111), Local2)", Debug)
        Store (LGreater (0xFFFFFFFF, 0x11111111), Local2)

        Store ("++++++++ Store (LGreaterEqual (0xFFFFFFFF, 0x11111111), Local3)", Debug)
        Store (LGreaterEqual (0xFFFFFFFF, 0x11111111), Local3)

        Store ("++++++++ Store (LLess (0xFFFFFFFF, 0x11111111), Local4)", Debug)
        Store (LLess (0xFFFFFFFF, 0x11111111), Local4)

        Store ("++++++++ Store (LLessEqual (0xFFFFFFFF, 0x11111111), Local5)", Debug)
        Store (LLessEqual (0xFFFFFFFF, 0x11111111), Local5)

        Store ("++++++++ Store (LNot (0x31313131), Local6)", Debug)
        Store (0x00001111, Local6)
        Store (LNot (Local6), Local7)
        if (LNotEqual (Local6, 0x00001111))
        {
            ERR_ (2, __LINE__)
        }

        if (LNotEqual (Local7, 0x0))
        {
            ERR_ (1, __LINE__)
        }


        Store ("++++++++ Store (LNotEqual (0xFFFFFFFF, 0x11111111), Local7)", Debug)
        Store (LNotEqual (0xFFFFFFFF, 0x11111111), Local7)

        Store ("++++++++ Lor (0x0, 0x1)", Debug)
        if (Lor (0x0, 0x1))
        {
            Store ("+_+_+_+_+ Lor (0x0, 0x1) returned TRUE", Debug)
        }

        return (Local7)
    }


    Method (COND)
    {
        Store ("++++++++ Store (0x4, Local0)", Debug)
        Store (0x4, Local0)

        Store ("++++++++ While (Local0)", Debug)
        While (Local0)
        {
            Store ("++++++++ Decrement (Local0)", Debug)
            Decrement (Local0)
        }


        Store ("++++++++ Store (0x3, Local6)", Debug)
        Store (0x3, Local6)

        Store ("++++++++ While (Subtract (Local6, 1))", Debug)
        While (Subtract (Local6, 1))
        {
            Store ("++++++++ Decrement (Local6)", Debug)
            Decrement (Local6)
        }


        Store ("++++++++ [LVL1] If (LGreater (0x2, 0x1))", Debug)
        If (LGreater (0x2, 0x1))
        {
            Store ("++++++++ [LVL2] If (LEqual (0x11111111, 0x22222222))", Debug)
            If (LEqual (0x11111111, 0x22222222))
            {
                Store ("++++++++ ERROR: If (LEqual (0x11111111, 0x22222222)) returned TRUE", Debug)
            }

            else
            {
                Store ("++++++++ [LVL3] If (LNot (0x0))", Debug)
                If (LNot (0x0))
                {
                    Store ("++++++++ [LVL4] If (LAnd (0xEEEEEEEE, 0x2))", Debug)
                    If (LAnd (0xEEEEEEEE, 0x2))
                    {
                        Store ("++++++++ [LVL5] If (LLess (0x44444444, 0x3))", Debug)
                        If (LLess (0x44444444, 0x3))
                        {
                            Store ("++++++++ ERROR: If (LLess (0x44444444, 0x3)) returned TRUE", Debug)
                        }

                        else
                        {
                            Store ("++++++++ Exiting from nested IF/ELSE statements", Debug)
                        }
                    }
                }
            }
        }


        Store ("++++++++ [LVL1] If (LGreater (0x2, 0x1))", Debug)
        If (LGreater (0x2, 0x1))
        {
            Store ("++++++++ [LVL2] If (LEqual (0x11111111, 0x22222222))", Debug)
            If (LEqual (0x11111111, 0x22222222))
            {
                Store ("++++++++ ERROR: If (LEqual (0x11111111, 0x22222222)) returned TRUE", Debug)
            }

            else
            {
                Store ("++++++++ [LVL3] If (LNot (0x0))", Debug)
                If (LNot (0x0))
                {
                    Store ("++++++++ [LVL4] If (LAnd (0xEEEEEEEE, 0x2))", Debug)
                    If (LAnd (0xEEEEEEEE, 0x2))
                    {
                        Store ("++++++++ [LVL5] If (LLess (0x44444444, 0x3))", Debug)
                        If (LLess (0x44444444, 0x3))
                        {
                            Store ("++++++++ ERROR: If (LLess (0x44444444, 0x3)) returned TRUE", Debug)
                        }

                        else
                        {
                            Store ("++++++++ Returning from nested IF/ELSE statements", Debug)
                            Return (Local6)
                        }
                    }
                }
            }
        }

    }


    Method (REFS,, Serialized)
    {
        Name (BBUF, Buffer() {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7})

        Name (NEST, Package ()
        {
            Package ()
            {
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06
            },
            Package ()
            {
                0x11, 0x12, 0x12, 0x14, 0x15, 0x16
            }
        })

        Store (RefOf (MAIN), Local5)

        // For this to work, ABCD must NOT exist.

        Store (CondRefOf (ABCD, Local0), Local1)
        if (LNotEqual (Local1, 0))
        {
            ERR_ (2, __LINE__)
        }

        Store (CondRefOf (BBUF, Local0), Local1)
        if (LNotEqual (Local1, Ones))
        {
            ERR_ (2, __LINE__)
        }

        Store (DeRefOf (Index (BBUF, 3)), Local6)
        if (LNotEqual (Local6, 0xB3))
        {
            ERR_ (2, __LINE__)
        }

        Store (DeRefOf (Index (DeRefOf (Index (NEST, 1)), 3)), Local0)
        if (LNotEqual (Local0, 0x14))
        {
            ERR_ (2, __LINE__)
        }


        Store (0x11223344, Local0)
        Store (RefOf (Local0), Local1)

        Store (DerefOf (Local1), Local2)
        If (LNotEqual (Local2, 0x11223344))
        {
            ERR_ (2, __LINE__)
        }


    /* Parser thinks this is a method invocation!! */

    //  RefOf (MAIN)


    //  RefOf (R___)
    //  RefOf (BBUF)

    //  Store (RefOf (Local0), Local1)

    //  CondRefOf (BBUF, Local2)
    //  CondRefOf (R___, Local3)

    //  Store (DerefOf (Local1), Local4)

    //  Return (Local4)
    }


    Method (INDX, 0, Serialized)
    {
        Name(STAT,Package(4){})
        Store(0x44443333,Index(STAT,0))
    }

//=================================================================
//=================================================================
//===================== iPCO TESTS ================================
//=================================================================
//=================================================================
//
//
// test IfElseOp.asl
//
//  test for IfOp and ElseOp, including validation of object stack cleanup
//
    Device (IFEL)
    {
        Name (DWRD, 1)
        Name (RSLT, 0)

        //  IFNR control method executes IfOp branch with NO nested Return
        //  and no Else branch
        Method (IFNR)
        {
            Store (DWRD, RSLT)
            If (LEqual (DWRD, 1))
            {
                Store (0, RSLT)
            }
        }   //  IFNR

        //  NINR control method does not execute If branch and has no Else branch
        Method (NINR)
        {
            Store (0, RSLT)
            If (LNotEqual (DWRD, 1))
            {
                Store (DWRD, RSLT)
            }
        }   //  NINR

        //  IENR control method executes IfOp branch with NO nested Return
        Method (IENR)
        {
            If (LEqual (DWRD, 1))
            {
                Store (0, RSLT)
            }
            Else
            {
                Store (DWRD, RSLT)
            }
        }   //  IENR

        //  ELNR control method executes ElseOp branch with NO nested Return
        Method (ELNR)
        {
            If (LNotEqual (DWRD, 1))
            {
                Store (DWRD, RSLT)
            }
            Else
            {
                Store (0, RSLT)
            }
        }   //  ELNR

        //  IFRT control method executes IfOp branch with nested Return with
        //  no Else branch
        Method (IFRT)

        {
            If (LEqual (DWRD, 1))
            {
                Return (0)
            }
            Return (DWRD)
        }   //  IFRT

        //  IERT control method executes IfOp branch with nested Return with
        //  Else branch
        Method (IERT)
        {
            If (LEqual (DWRD, 1))
            {
                Return (0)
            }
            Else
            {
                Return (DWRD)
            }
        }   //  IERT

        //  ELRT control method executes ElseOp branch with nested Return
        Method (ELRT)
        {
            If (LNotEqual (DWRD, 1))
            {
                Return (DWRD)
            }
            Else
            {
                Return (0)
            }
        }   //  ELRT

        Method (TEST)
        {
            Store ("++++++++ IfElseOp Test", Debug)

            //  IfOp with NO return value
            IFNR()
            If (LNotEqual (RSLT, 0))
            {
                Return (RSLT)
            }

            //  IfOp with NO return value
            NINR()
            If (LNotEqual (RSLT, 0))
            {
                Return (RSLT)
            }

            //  IfOp with NO return value
            IENR()
            If (LNotEqual (RSLT, 0))
            {
                Return (RSLT)
            }

            //  ElseOp with NO return value
            ELNR()
            If (LNotEqual (RSLT, 0))
            {
                Return (RSLT)
            }

            //  IfOp with return value
            Store (IFRT, RSLT)
            If (LNotEqual (RSLT, 0))
            {
                Return (RSLT)
            }

            //  IfOp with return value
            Store (IERT, RSLT)
            If (LNotEqual (RSLT, 0))
            {
                Return (RSLT)
            }

            //  ElseOp with return value
            Store (ELRT, RSLT)
            If (LNotEqual (RSLT, 0))
            {
                Return (RSLT)
            }

            Return (0)
        }   //  TEST
    }   //  IFEL

//
// test NoSave.asl
//
//
//  Internal test cases to validate IfOp (Operator (,,)) where Operator
//  target is ZeroOp to throw away the results.
//  Includes internal test cases for logical operators with no destination
//  operands.
//
    Device (NOSV)
    {
        Method (TEST,, Serialized)
        {
            Store ("++++++++ NoSave Test", Debug)

            Name (WRD, 0x1234)

            //
            //  Begin test of nested operators without saving results
            //

            //  Test If (And ()) with no save of And result
            If (And (3, 1, ))
            {
                Store (1, WRD)  //  pass -- just do something
            }
            else
            {
                Return (1)      //  fail
            }

            //  Test If (And ()) with no save of And result
            If (And (4, 1, ))
            {
                Return (2)      //  fail
            }
            else
            {
                Store (2, WRD)  //  pass -- just do something
            }


            //  Test If (NAnd ()) with no save of NAnd result
            If (NAnd (3, 1, ))
            {
                Store (3, WRD)  //  pass -- just do something
            }
            else
            {
                Return (3)      //  fail
            }

            //  Test If (NAnd ()) with no save of NAnd result
            If (NAnd (0xFFFFFFFF, 0xFFFFFFFF, ))
            {
                Return (4)      // fail
            }
            else
            {
                Store (4, WRD)  //  pass -- just do something
            }


            //  Test If (NOr ()) with no save of NOr result
            If (NOr (0, 1, ))
            {
                Store (5, WRD)  //  pass -- just do something
            }
            else
            {
                Return (5)      //  fail
            }

            //  Test If (NOr ()) with no save of NOr result
            If (NOr (0xFFFFFFFE, 1, ))
            {
                Return (6)      // fail
            }
            else
            {
                Store (6, WRD)  //  pass -- just do something
            }


            //  Test If (Not ()) with no save of Not result
            If (Not (1, ))
            {
                Store (7, WRD)  //  pass -- just do something
            }
            else
            {
                Return (7)      //  fail
            }

            //  Test If (Not ()) with no save of Not result
            If (Not (0xFFFFFFFF, ))
            {
                Return (8)      // fail
            }
            else
            {
                Store (8, WRD)  //  pass -- just do something
            }


            //  Test If (Or ()) with no save of Or result
            If (Or (3, 1, ))
            {
                Store (9, WRD)  //  pass -- just do something
            }
            else
            {
                Return (9)      //  fail
            }

            //  Test If (Or ()) with no save of Or result
            If (Or (0, 0, ))
            {
                Return (10)     //  fail
            }
            else
            {
                Store (10, WRD) //  pass -- just do something
            }


            //  Test If (XOr ()) with no save of XOr result
            If (XOr (3, 1, ))
            {
                Store (11, WRD) //  pass -- just do something
            }
            else
            {
                Return (11)     // fail
            }

            //  Test If (XOr ()) with no save of XOr result
            If (XOr (3, 3, ))
            {
                Return (12)     // fail
            }
            else
            {
                Store (12, WRD) //  pass -- just do something
            }


            //
            //  Begin test of logical operators with no destination operands
            //

            //  Test If (LAnd ()) with no save of LAnd result
            If (LAnd (3, 3))
            {
                Store (21, WRD) //  pass -- just do something
            }
            else
            {
                Return (21)     // fail
            }

            //  Test If (LAnd ()) with no save of LAnd result
            If (LAnd (3, 0))
            {
                Return (22)     // fail
            }
            else
            {
                Store (22, WRD) //  pass -- just do something
            }

            //  Test If (LAnd ()) with no save of LAnd result
            If (LAnd (0, 3))
            {
                Return (23)     //  fail
            }
            else
            {
                Store (23, WRD) //  pass -- just do something
            }

            //  Test If (LAnd ()) with no save of LAnd result
            If (LAnd (0, 0))
            {
                Return (24)     //  fail
            }
            else
            {
                Store (24, WRD) //  pass -- just do something
            }


            //  Test If (LEqual ()) with no save of LEqual result
            If (LEqual (3, 3))
            {
                Store (31, WRD) //  pass -- just do something
            }
            else
            {
                Return (31)     //  fail
            }

            //  Test If (LEqual ()) with no save of LEqual result
            If (LEqual (1, 3))
            {
                Return (32)     //  fail
            }
            else
            {
                Store (32, WRD) //  pass -- just do something
            }


            //  Test If (LGreater ()) with no save of LGreater result
            If (LGreater (3, 1))
            {
                Store (41, WRD) //  pass -- just do something
            }
            else
            {
                Return (41)     //  fail
            }

            //  Test If (LGreater ()) with no save of LGreater result
            If (LGreater (4, 4))
            {
                Return (42)     //  fail
            }
            else
            {
                Store (42, WRD) //  pass -- just do something
            }

            //  Test If (LGreater ()) with no save of LGreater result
            If (LGreater (1, 4))
            {
                Return (43)     //  fail
            }
            else
            {
                Store (43, WRD) //  pass -- just do something
            }

            //  Test If (LGreaterEqual ()) with no save of LGreaterEqual result
            If (LGreaterEqual (3, 1))
            {
                Store (44, WRD) //  pass -- just do something
            }
            else
            {
                Return (44)     //  fail
            }

            //  Test If (LGreaterEqual ()) with no save of LGreaterEqual result
            If (LGreaterEqual (3, 3))
            {
                Store (45, WRD) //  pass -- just do something
            }
            else
            {
                Return (45)     //  fail
            }

            //  Test If (LGreaterEqual ()) with no save of LGreaterEqual result
            If (LGreaterEqual (3, 4))
            {
                Return (46)     //  fail
            }
            else
            {
                Store (46, WRD) //  pass -- just do something
            }


            //  Test If (LLess ()) with no save of LLess result
            If (LLess (1, 3))
            {
                Store (51, WRD) //  pass -- just do something
            }
            else
            {
                Return (51)     //  fail
            }

            //  Test If (LLess ()) with no save of LLess result
            If (LLess (2, 2))
            {
                Return (52)     //  fail
            }
            else
            {
                Store (52, WRD) //  pass -- just do something
            }

            //  Test If (LLess ()) with no save of LLess result
            If (LLess (4, 2))
            {
                Return (53)     //  fail
            }
            else
            {
                Store (53, WRD) //  pass -- just do something
            }


            //  Test If (LLessEqual ()) with no save of LLessEqual result
            If (LLessEqual (1, 3))
            {
                Store (54, WRD) //  pass -- just do something
            }
            else
            {
                Return (54)     //  fail
            }

            //  Test If (LLessEqual ()) with no save of LLessEqual result
            If (LLessEqual (2, 2))
            {
                Store (55, WRD) //  pass -- just do something
            }
            else
            {
                Return (55)     //  fail
            }

            //  Test If (LLessEqual ()) with no save of LLessEqual result
            If (LLessEqual (4, 2))
            {
                Return (56)     //  fail
            }
            else
            {
                Store (56, WRD) //  pass -- just do something
            }


            //  Test If (LNot ()) with no save of LNot result
            If (LNot (0))
            {
                Store (61, WRD) //  pass -- just do something
            }
            else
            {
                Return (61)     //  fail
            }

            //  Test If (LNot ()) with no save of LNot result
            If (LNot (1))
            {
                Return (62)     //  fail
            }
            else
            {
                Store (62, WRD) //  pass -- just do something
            }


            //  Test If (LNotEqual ()) with no save of LNotEqual result
            If (LNotEqual (3, 3))
            {
                Return (63)     //  fail
            }
            else
            {
                Store (63, WRD) //  pass -- just do something
            }

            //  Test If (LNotEqual ()) with no save of LNotEqual result
            If (LNotEqual (1, 3))
            {
                Store (64, WRD) //  pass -- just do something
            }
            else
            {
                Return (64)     //  fail
            }


            //  Test If (LOr ()) with no save of LOr result
            If (LOr (3, 1))
            {
                Store (71, WRD) //  pass -- just do something
            }
            else
            {
                Return (71)     //  fail
            }

            //  Test If (LOr ()) with no save of LOr result
            If (LOr (0, 1))
            {
                Store (72, WRD) //  pass -- just do something
            }
            else
            {
                Return (72)     //  fail
            }

            //  Test If (LOr ()) with no save of LOr result
            If (LOr (3, 0))
            {
                Store (73, WRD) //  pass -- just do something
            }
            else
            {
                Return (73)     //  fail
            }

            //  Test If (LOr ()) with no save of LOr result
            If (LOr (0, 0))
            {
                Return (74)     //  fail
            }
            else
            {
                Store (74, WRD) //  pass -- just do something
            }

            Return (0)
        }   //  TEST
    }   //  NOSV


//
// test IndxFld.asl
//
//  IndexFld test
//      This is just a subset of the many RegionOp/Index Field test cases.
//      Tests index field element AccessAs macro.
//
    Device (IDXF)
    {   //  Test device name

        OperationRegion (SIO, SystemIO, 0x100, 2)
        Field (SIO, ByteAcc, NoLock, Preserve)
        {
            INDX,   8,
            DATA,   8
        }
        IndexField (INDX, DATA, AnyAcc, NoLock, WriteAsOnes)
        {
            AccessAs (ByteAcc, 0),
            IFE0,   8,
            IFE1,   8,
            IFE2,   8,
            IFE3,   8,
            IFE4,   8,
            IFE5,   8,
            IFE6,   8,
            IFE7,   8,
            IFE8,   8,
            IFE9,   8,
        }

        Method (TEST)
        {
            Store ("++++++++ IndxFld Test", Debug)

            Store (IFE0, Local0)
            Store (IFE1, Local1)
            Store (IFE2, Local2)

            Return (0)
        }   //  TEST
    }   //  IDXF

//
// test NestdLor.asl
//
    Scope (\_SB)    //  System Bus
    {   //  _SB system bus

        Name (ZER0, 0)
        Name (ZER1, 0)
        Name (ZER2, 0)
        Name (ONE0, 1)

        Device (NSTL)
        {
            Method (TEST)
            {
                Store ("++++++++ NestdLor Test", Debug)

                If (Lor (ZER0, Lor (ZER1, Lor (ZER2, ONE0))))
                {   //  Indicate Pass
                    Store (0x00, Local0)
                }

                Else
                {   //  Indicate Fail
                    Store (0x01, Local0)
                }

                Return (Local0)
            }   //  End Method TEST
        }   //  Device NSTL
    }   //  _SB system bus

//
// test RetBuf.asl
//
//  Test ReturnOp(Buffer)
//      This is required to support Control Method Batteries on
//          Dell Latitude Laptops (e.g., CP1-A)
//
    Device (RTBF)
    {
        Method (SUBR, 1)
        {
            Return (Arg0)
        }

        Method (RBUF,, Serialized)
        {   //  RBUF: Return Buffer from local variable
            Name (ABUF, Buffer() {"ARBITRARY_BUFFER"})

            //  store local buffer ABUF into Local0
            Store (ABUF, Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Buffer
            If (LNotEqual (Local1, 3))  //  Buffer type is 3
            {
                Return (1)      //  failure
            }

            //  store value returned by control method SUBR into Local0
            Store (SUBR (ABUF), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Buffer
            If (LNotEqual (Local1, 3))  //  Buffer type is 3
            {
                Return (2)      //  failure
            }

            //  allocate buffer using Local1 as buffer size (run-time evaluation)
            Store (5, Local1)
            Name (BUFR, Buffer(Local1) {})

            //  store value returned by control method SUBR into Local0
            Store (SUBR (BUFR), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Buffer
            If (LNotEqual (Local1, 3))  //  Buffer type is 3
            {
                Return (3)      //  failure
            }

            //  store BUFR Buffer into Local0
            Store (BUFR, Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Buffer
            If (LNotEqual (Local1, 3))  //  Buffer type is 3
            {
                Return (4)      //  failure
            }


            //  return Local0 Buffer
            Return (Local0)
        }   //  RBUF

        Method (TEST)
        {
            Store ("++++++++ RetBuf Test", Debug)

            //  store RBUF Buffer return value into Local0
            Store (RBUF, Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Buffer
            If (LNotEqual (Local1, 3))  //  Buffer type is 3
            {
                Return (10)     //  failure
            }
            Else
            {
                Return (0)      //  success
            }
        }   //  TEST
    }   //  RTBF

//
// test RetLVal.asl
//
//  Test ReturnOp(Lvalue)
//      This is required to support _PSR on IBM ThinkPad 560D and
//      _DCK on Toshiba Tecra 8000.
//

    Device (GPE2)
    {
        Method (_L03)
        {
            Store ("Method GPE2._L03 invoked", Debug)
            Return ()
        }

        Method (_E05)
        {
            Store ("Method GPE2._E05 invoked", Debug)
            Return ()
        }
    }

    Device (PRW2)
    {
        Name (_PRW, Package(2) {Package(2){\GPE2, 0x05}, 3})
    }


    Scope (\_GPE)
    {
        Name (ACST, 0xFF)

        Method (_L08)
        {
            Store ("Method _GPE._L08 invoked", Debug)
            Return ()
        }

        Method (_E09)
        {
            Store ("Method _GPE._E09 invoked", Debug)
            Return ()
        }

        Method (_E11)
        {
            Store ("Method _GPE._E11 invoked", Debug)
            Notify (\PRW1, 2)
        }

        Method (_L22)
        {
            Store ("Method _GPE._L22 invoked", Debug)
            Return ()
        }

        Method (_L33)
        {
            Store ("Method _GPE._L33 invoked", Debug)
            Return ()
        }

        Method (_E64)
        {
            Store ("Method _GPE._E64 invoked", Debug)
            Return ()
        }

    }   //  _GPE

    Device (PRW1)
    {
        Name (_PRW, Package(2) {0x11, 3})
    }

    Device (PWRB)
    {
        Name (_HID, EISAID("PNP0C0C"))
        Name (_PRW, Package(2) {0x33, 3})
    }


    Scope (\_SB)    //  System Bus
    {   //  _SB system bus

        Device (ACAD)
        {   //  ACAD:   AC adapter device
            Name (_HID, "ACPI0003") //  AC adapter device

            Name (_PCL, Package () {\_SB})

            OperationRegion (AREG, SystemIO, 0x0372, 2)
            Field (AREG, ByteAcc, NoLock, Preserve)
            {
                AIDX,   8,
                ADAT,   8
            }
            IndexField (AIDX, ADAT, ByteAcc, NoLock, Preserve)
            {
                     ,  1,  //  skips
                ACIN,   1,
                     ,  2,  //  skips
                CHAG,   1,
                     ,  3,  //  skips
                     ,  7,  //  skips
                ABAT,   1,
            }   //  IndexField

            Method (_PSR)
            {
                Store (\_GPE.ACST, Local0)
                Store (ACIN, Local1)
                If (LNotEqual (\_GPE.ACST, Local1))
                {
                    Store (Local1, \_GPE.ACST)
                    // This Notify is commented because it causes a
                    //  method error when running on a system without the
                    //  specific device.
                    // Notify (\_SB_.ACAD, 0)
                }
                Return (Local0)
            }   //  _PSR

            Method (_STA)
            {
                Return (0x0F)
            }

            Method (_INI)
            {
                Store (ACIN, \_GPE.ACST)
            }
        }   //  ACAD:   AC adapter device

        //  test implicit return from control method
        Method (DIS_, 1)
        {
            Store (Arg0, Local0)
        }

        Device (RTLV)
        {
            //  test implicit return inside nested if with explicit return of Lvalue
            Method (_DCK, 1)
            //  Arg0:   1 == dock, 0 == undock
            {
                If (Arg0)
                {   //  dock
                    Store (0x87, Local0)

                    If (Local0)
                    {
                        DIS_ (0x23)
                        Return (1)
                    }

                    Return (0)
                }   //  dock
                Else
                {   //  undock
                    Store (Arg0, Local0)

                    If (Local0)
                    {
                        DIS_ (0x23)
                        Return (1)
                    }

                    Return (0)
                }   //  undock
            }   //  _DCK control method

            Method (TEST)
            {
                Store ("++++++++ RetLVal Test", Debug)

                //  store _PSR return value into Local0
                Store (\_SB_.ACAD._PSR, Local0)

                //  save Local0 object type value into Local1
                Store (ObjectType (Local0), Local1)

                //  validate Local0 is a Number
                If (LNotEqual (Local1, 1))  //  Number/Integer type is 1
                {
                    Return (1)      //  failure
                }

                //  test implicit return inside nested if with explicit return of Lvalue
                Store (_DCK (1), Local2)

                //  save Local2 object type value into Local3
                Store (ObjectType (Local2), Local3)

                //  validate Local2 is a Number
                If (LNotEqual (Local3, 1))  //  Number/Integer type is 1
                {
                    Return (2)      //  failure
                }

                If (LNotEqual (Local2, 1))
                {
                    Return (3)      //  failure
                }

                Return (0)  //  success
            }   //  TEST
        }   //  RTLV
    }   //  _SB system bus

//
// test RetPkg.asl
//
//  Test ReturnOp(Package)
//      This is required to support _PRT on Dell Optiplex Workstations (e.g. GX1)
//

    Scope (\_SB)    //  System Bus
    {   //  _SB system bus
        Device(LNKA)
        {
            Name (_HID, EISAID("PNP0C0F"))  //  PCI interrupt link
            Name (_UID, 1)
        }
        Device(LNKB)
        {
            Name (_HID, EISAID("PNP0C0F"))  //  PCI interrupt link
            Name (_UID, 2)
        }
        Device(LNKC)
        {
            Name (_HID, EISAID("PNP0C0F"))  //  PCI interrupt link
            Name (_UID, 3)
        }
        Device(LNKD)
        {
            Name (_HID, EISAID("PNP0C0F"))  //  PCI interrupt link
            Name (_UID, 4)
        }

        Device (PCI1)
        {   //  PCI1:   Root PCI Bus
            Name (_HID, "PNP0A03")  //  Need _HID for root device (String format)
            Name (_ADR,0x00000000)
            Name (_CRS,0)

            Name (_PRT, Package ()
            {
                Package () {0x0004ffff, 0, LNKA, 0},            //  Slot 1, INTA
                Package () {0x0004ffff, 1, LNKB, 0},            //  Slot 1, INTB
                Package () {0x0004ffff, 2, LNKC, 0},            //  Slot 1, INTC
                Package () {0x0004ffff, 3, LNKD, 0},            //  Slot 1, INTD
                Package () {0x0005ffff, 0, \_SB_.LNKB, 0},  //  Slot 2, INTA
                Package () {0x0005ffff, 1, \_SB_.LNKC, 0},  //  Slot 2, INTB
                Package () {0x0005ffff, 2, \_SB_.LNKD, 0},  //  Slot 2, INTC
                Package () {0x0006ffff, 3, \_SB_.LNKA, 0},  //  Slot 2, INTD
                Package () {0x0006ffff, 0, LNKC, 0},            //  Slot 3, INTA
                Package () {0x0006ffff, 1, LNKD, 0},            //  Slot 3, INTB
                Package () {0x0006ffff, 2, LNKA, 0},            //  Slot 3, INTC
                Package () {0x0006ffff, 3, LNKB, 0},            //  Slot 3, INTD
            })

            Device (PX40)
            {   // Map f0 space, Start PX40
                Name (_ADR,0x00070000)  //  Address+function.
            }
        }   //  PCI0:   Root PCI Bus

        Device (RETP)
        {
            Method (RPKG)
            {   //  RPKG: Return Package from local variable

                //  store _PRT package into Local0
                Store (\_SB_.PCI1._PRT, Local0)

                //  return Local0 Package
                Return (Local0)
            }   //  RPKG

            Method (TEST)
            {
                Store ("++++++++ RetPkg Test", Debug)

                //  store RPKG package return value into Local0
                Store (RPKG, Local0)

                //  save Local0 object type value into Local1
                Store (ObjectType (Local0), Local1)

                //  validate Local0 is a Package
                If (LNotEqual (Local1, 4))  //  Package type is 4
                    {   Return (1)  }   //  failure
                Else
                    {   Return (0)  }   //  success
            }   //  TEST
        }   //  RETP
    } // _SB_

//
// test WhileRet.asl
//
//  WhileRet.asl tests a ReturnOp nested in a IfOp nested in a WhileOp.
//
    Device (WHLR)
    {
        Name (LCNT, 0)
        Method (WIR)
        {   //  WIR:    control method that returns inside of IfOp inside of WhileOp
            While (LLess (LCNT, 4))
            {
                    If (LEqual (LCNT, 2))
                    {
                        Return (0)
                    }

                Increment (LCNT)
            }

            Return (LCNT)
        }   //  WIR:    control method that returns inside of IfOp inside of WhileOp

        Method (TEST)
        {
            Store ("++++++++ WhileRet Test", Debug)

            Store (WIR, Local0)

            Return (Local0)
        }   //  TEST
    }   //  WHLR

//
// test AndOrOp.asl
//
//This code tests the bitwise AndOp and OrOp Operator terms
//
//Syntax of Andop term
//And - Bitwise And
//AndTerm   := And(
//  Source1,    //TermArg=>Integer
//  Source2,    //TermArg=>Integer
//  Result  //Nothing | SuperName
//) => Integer
//Source1 and Source2 are evaluated as integer data types,
// a bit-wise AND is performed, and the result is optionally
//stored into Result.
//
//
//Syntax of OrOp
//Or - Bit-wise Or
//OrTerm    := Or(
//  Source1,    //TermArg=>Integer
//  Source2 //TermArg=>Integer
//  Result  //Nothing | SuperName
//) => Integer
//Source1 and Source2 are evaluated as integer data types,
// a bit-wide OR is performed, and the result is optionally
//stored in Result
//
    Device (ANDO)
    {
        OperationRegion (TMEM, SystemMemory, 0xC4, 0x02)
        Field (TMEM, ByteAcc, NoLock, Preserve)
        {
                ,   3,
            TOUD,   13
        }

        //Create System Memory Operation Region and field overlays
        OperationRegion (RAM, SystemMemory, 0x400000, 0x100)
        Field (RAM, AnyAcc, NoLock, Preserve)
        {
            SMDW,   32, //  32-bit DWORD
            SMWD,   16, //  16-bit WORD
            SMBY,   8,  //  8-bit BYTE
        }// Field(RAM)


        //And with Byte Data
        Name (BYT1, 0xff)
        Name (BYT2, 0xff)
        Name (BRSL, 0x00)

        //And with Word Data
        Name (WRD1, 0xffff)
        Name (WRD2, 0xffff)
        Name (WRSL, 0x0000)

        //And with DWord Data
        Name (DWD1, 0xffffffff)
        Name (DWD2, 0xffffffff)
        Name (DRSL, 0x00000000)

        Method (ANDP)
        {
            //Check with 1 And 1 on byte data
            And(BYT1, BYT2, BRSL)
            if(LNotEqual(BRSL,0xff))
            {Return(1)}

            //Check with 1 And 1 on Word data
            And(WRD1, WRD2, WRSL)
            if(LNotEqual(WRSL,0xffff))
            {
                Return (1)      //  failure
            }

            //Check with 1 And 1 Dword
            And(DWD1, DWD2, DRSL)
            if(LNotEqual(DRSL,0xffffffff))
            {
                Return (1)      //  failure
            }

            //Check with 0 And 0 on byte data
            Store(0x00,BYT1)
            Store(0x00,BYT2)
            Store(0x00,BRSL)
            And(BYT1, BYT2, BRSL)
            if(LNotEqual(BRSL,0x00))
            {
                Return (1)      //  failure
            }

            //Check with 0 And 0 on Word data
            Store (0x0000,WRD1)
            Store (0x0000,WRD2)
            Store (0x0000,WRSL)
            And(WRD1, WRD2, WRSL)
            if(LNotEqual(WRSL,0x0000))
            {
                Return (1)      //  failure
            }

            //Check with 0 And 0 Dword
            Store (0x00000000,DWD1)
            Store (0x00000000,DWD2)
            Store (0x00000000,DRSL)
            And(DWD1, DWD2, DRSL)
            if(LNotEqual(DRSL,0x00000000))
            {
                Return (1)      //  failure
            }


            //Check with 1 And 0 on byte data
            Store(0x55,BYT1)
            Store(0xAA,BYT2)
            Store(0x00,BRSL)
            And(BYT1, BYT2, BRSL)
            if(LNotEqual(BRSL,0x00))
            {
                Return (1)      //  failure
            }

            //Check with 1 And 0 on Word data
            Store (0x5555,WRD1)
            Store (0xAAAA,WRD2)
            Store (0x0000,WRSL)
            And(WRD1, WRD2, WRSL)
            if(LNotEqual(WRSL,0x0000))
            {
                Return (1)      //  failure
            }

            //Check with 1 And 0 on Dword
            Store (0x55555555,DWD1)
            Store (0xAAAAAAAA,DWD2)
            Store (0x00000000,DRSL)
            And(DWD1, DWD2, DRSL)
            if(LNotEqual(DRSL,0x00000000))
            {
                Return (1)      //  failure
            }

            Store (0x1FFF, TOUD)
            Store (TOUD, Local0)
            if(LNotEqual(Local0,0x1FFF))
            {
                Return (1)      //  failure
            }

            //TBD- Do We need to check for system memory data also for each test case ??

            Return(0)

        }//ANDP

        Method (OROP)
        {

            //Check with 1 Ored with 1 on byte data
            Store(0xff,BYT1)
            Store(0xff,BYT2)
            Store(0x00,BRSL)
            Or(BYT1, BYT2, BRSL)
            if(LNotEqual(BRSL,0xff))
            {
                Return (1)      //  failure
            }


            //Check with 1 Ored with 1 on Word data
            Store(0xffff,WRD1)
            Store(0xffff,WRD2)
            Store(0x0000,WRSL)
            Or(WRD1, WRD2, WRSL)
            if(LNotEqual(WRSL,0xffff))
            {
                Return (1)      //  failure
            }

            //Check with 1 Ored with 1 on Dword data
            Store(0xffffffff,DWD1)
            Store(0xffffffff,DWD2)
            Store(0x00000000,DRSL)
            Or(DWD1, DWD2, DRSL)
            if(LNotEqual(DRSL,0xffffffff))
            {
                Return (1)      //  failure
            }

            //Check with 0 Ored with 0 on byte data
            Store(0x00,BYT1)
            Store(0x00,BYT2)
            Store(0x00,BRSL)
            Or(BYT1, BYT2, BRSL)
            if(LNotEqual(BRSL,0x00))
            {
                Return (1)      //  failure
            }

            //Check with 0 Ored with 0 on Word data
            Store (0x0000,WRD1)
            Store (0x0000,WRD2)
            Store (0x0000,WRSL)
            Or(WRD1, WRD2, WRSL)
            if(LNotEqual(WRSL,0x0000))
            {
                Return (1)      //  failure
            }

            //Check with 0 Ored with  0 Dword data
            Store (0x00000000,DWD1)
            Store (0x00000000,DWD2)
            Store (0x00000000,DRSL)
            Or(DWD1, DWD2, DRSL)
            if(LNotEqual(DRSL,0x00000000))
            {
                Return (1)      //  failure
            }


            //Check with 1 Ored with 0 on byte data
            Store(0x55,BYT1)
            Store(0xAA,BYT2)
            Store(0x00,BRSL)
            Or(BYT1, BYT2, BRSL)
            if(LNotEqual(BRSL,0xff))
            {
                Return (1)      //  failure
            }

            //Check with 1 Ored with 0 on Word data
            Store (0x5555,WRD1)
            Store (0xAAAA,WRD2)
            Store (0x0000,WRSL)
            Or(WRD1, WRD2, WRSL)
            if(LNotEqual(WRSL,0xffff))
            {
                Return (1)      //  failure
            }

            //Check with 1 Ored with 0 on Dword data
            Store (0x55555555,DWD1)
            Store (0xAAAAAAAA,DWD2)
            Store (0x00000000,DRSL)
            Or(DWD1, DWD2, DRSL)
            if(LNotEqual(DRSL,0xffffffff))
            {
                Return (1)      //  failure
            }

            //TBD - Do We need to check for system memory data also for each test case ??

            Return(0)

        }//OROP

        Method(TEST,, Serialized)
        {
            Store ("++++++++ AndOrOp Test", Debug)

            Name(RSLT,1)
            //Call Andop method
            Store(ANDP,RSLT)
            if(LEqual(RSLT,1))
            {
                Return (RSLT)
            }

            //Call OrOp Method
            Store(OROP,RSLT)
            if(LEqual(RSLT,1))
            {
                Return(RSLT)
            }

            //
            // Return original conditions to allow iterative execution
            //
            Store(0xff,BYT1)
            Store(0xff,BYT2)
            Store(0x00,BRSL)
            Store (0xffff,WRD1)
            Store (0xffff,WRD2)
            Store (0x0000,WRSL)
            Store (0xffffffff,DWD1)
            Store (0xffffffff,DWD2)
            Store (0x00000000,DRSL)

            Return(0)
        }   //TEST
    }   //ANDO

//
// test BreakPnt.asl
//
// This code tests the BreakPoint opcode term. The syntax of BreakPoint Term is
// BreakPointTerm    := BreakPoint
// Used for debugging, the Breakpoint opcode stops the execution and enters the AML debugger.
// In the non-debug version of the interpreter, BreakPoint is equivalent to Noop.
//
    Device (BRKP)
    {
        Name(CNT0,0)

        Method (BK1)
        {
            BreakPoint
            Return(0)
        }

        Method (TEST)
        {
            Store ("++++++++ BreakPnt Test", Debug)

            Store(0,CNT0)

            //Check BreakPoint statement
            While(LLess(CNT0,10))
            {
                Increment(CNT0)
            }

            //Check the BreakPoint statement
            If(LEqual(CNT0,10))
            {
    //            BreakPoint
                Return(0)
            }

            //failed
            Return(1)
        }
    }

//
// test AddSubOp.asl
//
    Device (ADSU)
    {
        //  create System Memory Operation Region and field overlays
        OperationRegion (RAM, SystemMemory, 0x400000, 0x100)
        Field (RAM, AnyAcc, NoLock, Preserve)
        {
            SMDW,   32, //  32-bit DWORD
            SMWD,   16, //  16-bit WORD
            SMBY,   8,  //  8-bit BYTE
        }   //  Field(RAM)

        Method (TEST,, Serialized)
        {
            Store ("++++++++ AddSubOp Test", Debug)

            Name (DWRD, 0x12345678)
            Name (WRD, 0x1234)
            Name (BYT, 0x12)

            //  Test AddOp with DWORD data
            Store (0x12345678, DWRD)
            Add (DWRD, 7, DWRD)
            If (LNotEqual (DWRD, 0x1234567F))
                {   Return (DWRD)   }

            //  Test AddOp with WORD data
            Add (WRD, 5, WRD)
            If (LNotEqual (WRD, 0x1239))
                {   Return (WRD)    }

            //  Test AddOp with BYTE data
            Add (BYT, 3, BYT)
            If (LNotEqual (BYT, 0x15))
                {   Return (BYT)    }

            //  Test SubtractOp with DWORD data
            Subtract (DWRD, 7, DWRD)
            If (LNotEqual (DWRD, 0x12345678))
                {   Return (DWRD)   }

            //  Test SubtractOp with WORD data
            Subtract (WRD, 3, WRD)
            If (LNotEqual (WRD, 0x1236))
                {   Return (WRD)    }

            //  Test SubtractOp with BYTE data
            Subtract (BYT, 3, BYT)
            If (LNotEqual (BYT, 0x12))
                {   Return (BYT)    }


            //  test AddOp with DWORD SystemMemory OpRegion
            Store (0x01234567, SMDW)
            Add (SMDW, 8, SMDW)
            If (LNotEqual (SMDW, 0x0123456F))
                {   Return (SMDW)   }

            //  test SubtractOp with DWORD SystemMemory OpRegion
            Subtract (SMDW, 7, SMDW)
            If (LNotEqual (SMDW, 0x01234568))
                {   Return (SMDW)   }


            //  test AddOp with WORD SystemMemory OpRegion
            Store (0x0123, SMWD)
            Add (SMWD, 6, SMWD)
            If (LNotEqual (SMWD, 0x0129))
                {   Return (SMWD)   }

            //  test SubtractOp with WORD SystemMemory OpRegion
            Subtract (SMWD, 5, SMWD)
            If (LNotEqual (SMWD, 0x0124))
                {   Return (SMWD)   }


            //  test AddOp with BYTE SystemMemory OpRegion
            Store (0x01, SMBY)
            Add (SMBY, 4, SMBY)
            If (LNotEqual (SMBY, 0x05))
                {   Return (SMBY)   }

            //  test SubtractOp with BYTE SystemMemory OpRegion
            Subtract (SMBY, 3, SMBY)
            If (LNotEqual (SMBY, 0x02))
                {   Return (SMBY)   }

            Return (0)
        }   //  TEST
    }   //  ADSU

//
// test IncDecOp.asl
//
    Device (INDC)
    {
        //  create System Memory Operation Region and field overlays
        OperationRegion (RAM, SystemMemory, 0x400000, 0x100)
        Field (RAM, AnyAcc, NoLock, Preserve)
        {
            SMDW,   32, //  32-bit DWORD
            SMWD,   16, //  16-bit WORD
            SMBY,   8,  //  8-bit BYTE
        }   //  Field(RAM)

        Method (TEST,, Serialized)
        {
            Store ("++++++++ IncDecOp Test", Debug)

            Name (DWRD, 0x12345678)
            Name (WRD, 0x1234)
            Name (BYT, 0x12)

            //  Test IncrementOp with DWORD data
            Store (0x12345678, DWRD)
            Increment (DWRD)
            If (LNotEqual (DWRD, 0x12345679))
                {   Return (DWRD)   }

            //  Test IncrementOp with WORD data
            Increment (WRD)
            If (LNotEqual (WRD, 0x1235))
                {   Return (WRD)    }

            //  Test IncrementOp with BYTE data
            Increment (BYT)
            If (LNotEqual (BYT, 0x13))
                {   Return (BYT)    }

            //  Test DecrementOp with DWORD data
            Decrement (DWRD)
            If (LNotEqual (DWRD, 0x12345678))
                {   Return (DWRD)   }

            //  Test DecrementOp with WORD data
            Decrement (WRD)
            If (LNotEqual (WRD, 0x1234))
                {   Return (WRD)    }

            //  Test DecrementOp with BYTE data
            Decrement (BYT)
            If (LNotEqual (BYT, 0x12))
                {   Return (BYT)    }


            //  test IncrementOp with DWORD SystemMemory OpRegion
            Store (0x01234567, SMDW)
            Increment (SMDW)
            If (LNotEqual (SMDW, 0x01234568))
                {   Return (SMDW)   }

            //  test DecrementOp with DWORD SystemMemory OpRegion
            Decrement (SMDW)
            If (LNotEqual (SMDW, 0x01234567))
                {   Return (SMDW)   }


            //  test IncrementOp with WORD SystemMemory OpRegion
            Store (0x0123, SMWD)
            Increment (SMWD)
            If (LNotEqual (SMWD, 0x0124))
                {   Return (SMWD)   }

            //  test DecrementOp with WORD SystemMemory OpRegion
            Decrement (SMWD)
            If (LNotEqual (SMWD, 0x0123))
                {   Return (SMWD)   }


            //  test IncrementOp with BYTE SystemMemory OpRegion
            Store (0x01, SMBY)
            Increment (SMBY)
            If (LNotEqual (SMBY, 0x02))
                {   Return (SMBY)   }

            //  test DecrementOp with BYTE SystemMemory OpRegion
            Decrement (SMBY)
            If (LNotEqual (SMBY, 0x01))
                {   Return (SMBY)   }

            Return (0)
        }   //  TEST
    }   //  INDC

//
// test LOps.asl
//
//This source tests all the logical operators. Logical operators in ASL are as follows.
//LAnd, LEqual, LGreater, LLess, LNot, LNotEqual, LOr.
// Success will return 0 and failure will return a non zero number. Check the source code for
// non zero number to find where the test failed

    Device (LOPS)
    {
        //Create System Memory Operation Region and field overlays
        OperationRegion (RAM, SystemMemory, 0x400000, 0x100)
        Field (RAM, AnyAcc, NoLock, Preserve)
        {
            SMDW,   32, //  32-bit DWORD
            SMWD,   16, //  16-bit WORD
            SMBY,   8,  //  8-bit BYTE
        }// Field(RAM)

        //And with Byte Data
        Name (BYT1, 0xff)
        Name (BYT2, 0xff)
        Name (BRSL, 0x00)

        //And with Word Data
        Name (WRD1, 0xffff)
        Name (WRD2, 0xffff)
        Name (WRSL, 0x0000)

        //And with DWord Data
        Name (DWD1, 0xffffffff)
        Name (DWD2, 0xffffffff)
        Name (DRSL, 0x00000000)

        Name(RSLT,1)

        Method (ANDL,2) // Test Logical And
        {
            //test with the arguments passed
            if(LEqual(Arg0,Arg1))
            { Store(LAnd(Arg0,Arg1),RSLT)
                if(LNotEqual(Ones,RSLT))
                {Return(11)}
            }

            //test with he locals
            Store(Arg0,Local0)
            Store(Arg1,Local1)

            if(LEqual(Local0,Local1))
            {
                Store(LAnd(Local0,Local1),RSLT)
                if(LNotEqual(Ones,RSLT))
                    {Return(12)}
            }

            //test with BYTE data
            if(LEqual(BYT1,BYT2))
            { Store(LAnd(BYT1,BYT2),BRSL)
                if(LNotEqual(Ones,BRSL))
                {Return(13)}
            }

            //test with WORD data
            if(LEqual(WRD1,WRD2))
            { Store(LAnd(WRD1,WRD2),WRSL)
                if(LNotEqual(Ones,WRSL))
                {Return(14)}
            }

            //test with DWORD data
            if(LEqual(DWD1,DWD2))
            { Store(LAnd(DWD1,DWD2),DRSL)
                if(LNotEqual(Ones,DRSL))
                {Return(15)}
            }

            //Test for system memory data for each test case.

                Store(0xff,BYT1)
                Store(0xff,SMBY)
                Store(0x00,BRSL)

            //test with BYTE system memory data
            if(LEqual(BYT1,SMBY))
            { Store(LAnd(BYT1,SMBY),BRSL)
                if(LNotEqual(Ones,BRSL))
                {Return(16)}
            }

            Store (0xffff,WRD1)
            Store(0xffff,SMWD)
            Store(0x0000,WRSL)
            //test with WORD system memory data
            if(LEqual(WRD1,SMWD))
            { Store(LAnd(WRD1,SMWD),WRSL)
                if(LNotEqual(Ones,WRSL))
                {Return(17)}
            }

            Store(0x000000,DRSL)
            Store (0xffffff,DWD1)
            Store(0xffffff,SMDW)

            //test with DWORD system memory data
            if(LEqual(DWD1,SMDW))
            { Store(LAnd(DWD1,SMDW),DRSL)
                if(LNotEqual(Ones,DRSL))
                {Return(18)}
            }

            Return(0)

        }//ANDL

        //Test the LOr Operator

        Method (ORL_,2)
        {//ORL_

            //test with the arguments passed
            if(LEqual(Arg0,Arg1))
            {
                Store(LOr(Arg0,Arg1),RSLT)
                if(LNotEqual(Ones,RSLT))
                {
                    Return(21)
                }
            }

            //test with he locals
            Store(Arg0,Local0)
            Store(Arg1,Local1)

            if(LEqual(Local0,Local1))
            {
                Store(LOr(Local0,Local1),RSLT)
                if(LNotEqual(Ones,RSLT))
                    {Return(22)}
            }

            //Check with 1 LOred with 0 on byte data
            Store(0xff,BYT1)
            Store(0x00,BYT2)
            Store(0x00,BRSL)

            if(LNotEqual(BYT1, BYT2))
            {
                Store(LOr(BYT1, BYT2), BRSL)
                if(LNotEqual(Ones,BRSL))
                {Return(23)}
            }

            //Check with 1 LOred with 0 on WORD data
            Store(0xffff,WRD1)
            Store(0x0000,WRD2)
            Store(0x0000,WRSL)

            if(LNotEqual(WRD1, WRD2))
            {
                Store(LOr(WRD1, WRD2), WRSL)
                if(LNotEqual(Ones,WRSL))
                {Return(24)}
            }

            //Check with 1 LOred with 0 on DWORD data
            Store(0xffffffff,DWD1)
            Store(0x00000000,DWD2)
            Store(0x00000000,DRSL)

            if(LNotEqual(DWD1, DWD2))
            {
                Store(LOr(DWD1, DWD2), DRSL)
                if(LNotEqual(Ones,DRSL))
                {Return(25)}
            }

            Store(0x00,BYT1)
            Store(0xff,SMBY)
            Store(0x00,BRSL)

            //test with BYTE system memory data
            if(LEqual(BYT1,SMBY))
            { Store(LOr(BYT1,SMBY),BRSL)
                if(LNotEqual(Ones,BRSL))
                {Return(26)}
            }

            Store (0x0000,WRD1)
            Store(0xffff,SMWD)
            Store(0x0000,WRSL)

            //test with WORD system memory data
            if(LEqual(WRD1,SMWD))
            { Store(LOr(WRD1,SMWD),WRSL)
                if(LNotEqual(Ones,WRSL))
                {Return(27)}
            }


            Store(0x00000000,DWD1)
            Store(0xffffffff,SMDW)
            Store(0x00000000,DRSL)

            //test with DWORD system memory data
            if(LEqual(DWD1,SMDW))
            { Store(LAnd(DWD1,SMDW),DRSL)
                if(LNotEqual(Ones,DRSL))
                {Return(28)}
            }
            Return(0)

        }//ORL_

        //This method tests LGreater and LNot operator
        Method(LSGR,2)
        {//LSGR

            //Test on arguments passed

            //in test data, Arg1 > Arg0
            if(LEqual(Ones,LNot(LGreater(Arg1,Arg0))))
            {Return(31)}

            //test LLessEqual
            if(LEqual(Ones,LNot(LGreaterEqual(Arg1,Arg0))))
            {Return(32)}

            if(LEqual(Ones,LLess(Arg1,Arg0)))
            {Return(33)}

            //test LLessEqual
            if(LEqual(Ones,LLessEqual(Arg1,Arg0)))
            {Return(34)}

            Store(Arg0,Local0)
            Store(Arg1,Local1)

            //test with the locals
            if(LNot(LGreater(Local1,Local0)))
                {Return(35)}

            //test on Byte data
            Store(0x12,BYT1)
            Store(0x21,BYT2)

            if(LNot(LGreater(BYT2,BYT1)))
                {Return(36)}

            if(LNot(LLess(BYT1,BYT2)))
                {Return(37)}

            //test LGreaterEqual with byte data
            if(LNot(LGreaterEqual(BYT2,BYT1)))
                {Return(38)}

            //test LLessEqual byte data
            if(LNot(LLessEqual(BYT1,BYT2)))
                {Return(39)}


            //test on Word data
            Store(0x1212,WRD1)
            Store(0x2121,WRD2)

            if(LNot(LGreater(WRD2,WRD1)))
                {Return(310)}

            if(LNot(LLess(WRD1,WRD2)))
                {Return(311)}

            //Test LGreaterEqual with Word Data
            if(LNot(LGreaterEqual(WRD2,WRD1)))
                {Return(312)}


            //Test LLessEqual with Word Data
            if(LNot(LLessEqual(WRD1,WRD2)))
                {Return(313)}

            //test on DWord data
            Store(0x12121212,DWD1)
            Store(0x21212121,DWD2)

            if(LNot(LGreater(DWD2,DWD1)))
                {Return(314)}

            if(LNot(LLess(DWD1,DWD2)))
                {Return(315)}


            //Test LGreaterEqual with Dword
            if(LNot(LGreaterEqual(DWD2,DWD1)))
                {Return(316)}

            //Test LLessEqual DWord
            if(LNot(LLessEqual(DWD1,DWD2)))
                {Return(317)}

            Return(0)
        }//LSGR

        //The test method
        Method(TEST)
        {
            Store ("++++++++ LOps Test", Debug)

            Store(0,RSLT)
            //Call LAndOp method
            Store(ANDL(2,2),RSLT)
            if(LNotEqual(RSLT,0))
             {Return(RSLT)}

            //Call LOrOp Method
            Store(ORL_(5,5),RSLT)
            if(LNotEqual(RSLT,0))
            {Return(RSLT)}

            //Call LSGR Method
            Store(LSGR(5,7),RSLT)
            if(LNotEqual(RSLT,0))
            {Return(RSLT)}

            Return(0)
        }//TEST
    }//LOPS

//
// test FdSetOps.asl
//
//  FindSetLeftBit - Find Set Left Bit
//  FindSetLeftBitTerm  := FindSetLeftBit
//  (   Source, //TermArg=>Integer
//      Result  //Nothing | SuperName
//  ) => Integer
//  Source is evaluated as integer data type, and the one-based bit location of
//  the first MSb (most significant set bit) is optionally stored into Result.
//  The result of 0 means no bit was set, 1 means the left-most bit set is the
//  first bit, 2 means the left-most bit set is the second bit, and so on.
//  FindSetRightBit - Find Set Right Bit

//  FindSetRightBitTerm := FindSetRightBit
//  (   Source, //TermArg=>Integer
//      Result  //Nothing | SuperName
//  ) => Integer
//  Source is evaluated as integer data type, and the one-based bit location of
//  the most LSb (least significant set bit) is optionally stored in Result.
//  The result of 0 means no bit was set, 32 means the first bit set is the
//  32nd bit, 31 means the first bit set is the 31st bit, and so on.

//  If the Control method is success Zero is returned. Otherwise a non-zero
//  number is returned.
//
    Device (FDSO)
    {   //  FDSO

        //  Create System Memory Operation Region and field overlays
        OperationRegion (RAM, SystemMemory, 0x400000, 0x100)
        Field (RAM, AnyAcc, NoLock, Preserve)
        {
            SMDW,   32, //  32-bit DWORD
            SMWD,   16, //  16-bit WORD
            SMBY,   8,      //  8-bit BYTE
        }   //  Field(RAM)

        //  Byte Data
        Name (BYT1, 1)
        Name (BRSL, 0x00)

        //  Word Data
        Name (WRD1, 0x100)
        Name (WRSL, 0x0000)

        //  DWord Data
        Name (DWD1, 0x10000)
        Name (DRSL, 0x00000000)
        Name (RSLT, 1)
        Name (CNTR, 1)

        Method (SHFT,2)
        //  Arg0 is the actual data and Arg1 is the bit position
        {   //  SHFT
            Store (Arg0, Local0)
            Store (Arg1, Local1)

            FindSetLeftBit (Arg0, BRSL)
            If (LNotEqual (BRSL, Arg1))
                {   Return (0x11)   }
            If (LNotEqual (Arg0, Local0))
                {   Return (0x12)   }

            FindSetLeftBit (Local0, BRSL)
            If (LNotEqual (BRSL, Local1))
                {   Return (0x13)   }
            If (LNotEqual (Arg0, Local0))
                {   Return (0x14)   }

            //  test the byte value for SetLeftBit
            Store (7, BYT1)
            FindSetLeftBit (BYT1, BRSL)
            If (LNotEqual (BRSL, 3))
                {   Return (0x15)   }
            If (LNotEqual (BYT1, 7))
                {   Return (0x16)   }

            Store (1, BYT1)
            Store (1, CNTR)
            While (LLessEqual (CNTR, 8))
            {   //  FindSetLeftBit check loop for byte data
                FindSetLeftBit (BYT1, BRSL)
                If (LNotEqual (BRSL, CNTR))
                    {   Return (0x17)   }

                //  Shift the bits to check the same
                ShiftLeft (BYT1, 1, BYT1)
                Increment (CNTR)
            }   //  FindSetLeftBit check loop for byte data


            //  Check BYTE value for SetRightBit
            Store (7, BYT1)
            FindSetRightBit (BYT1, BRSL)
            If (LNotEqual (BRSL, 1))
                {   Return (0x21)   }
            If (LNotEqual (BYT1, 7))
                {   Return (0x22)   }

            Store (1, CNTR)
            Store (0xFF, BYT1)
            While (LLessEqual (CNTR, 8))
            {   //  FindSetRightBit check loop for byte data
                FindSetRightBit (BYT1, BRSL)
                If (LNotEqual (BRSL, CNTR))
                    {   Return (0x23)   }

                ShiftLeft (BYT1, 1, BYT1)
                Increment (CNTR)
            }   //  FindSetRightBit check loop for byte data


            //  Test Word value for SetLeftBit
            Store (9, CNTR)
            Store (0x100, WRD1)
            While (LLessEqual (CNTR, 16))
            {
                //  FindSetLeftBit check loop for Word data
                FindSetLeftBit (WRD1, WRSL)
                If (LNotEqual (WRSL, CNTR))
                    {   Return (0x31)   }

                //  Shift the bits to check the same
                ShiftLeft (WRD1, 1, WRD1)
                Increment (CNTR)
            }   //  FindSetLeftBit check loop for Word data

            //  Check Word value for SetRightBit
            Store (9, CNTR)
            Store (0xFF00, WRD1)
            While (LLessEqual (CNTR, 16))
            {
                //  FindSetRightBit check loop for Word data
                FindSetRightBit (WRD1, WRSL)
                If (LNotEqual (WRSL, CNTR))
                    {   Return (0x32)   }

                ShiftLeft (WRD1, 1, WRD1)
                Increment (CNTR)
            }   //  FindSetRightBit check loop for Word data

            //  Test the DWord value for SetLeftBit
            Store (17, CNTR)
            Store (0x10000, DWD1)
            While (LLessEqual (CNTR, 32))
            {
                //  FindSetLeftBit check loop for Dword
                FindSetLeftBit (DWD1, DRSL)
                If (LNotEqual (DRSL, CNTR))
                    {   Return (0x41)   }

                //  Shift the bits to check the same
                ShiftLeft (DWD1, 1, DWD1)
                Increment (CNTR)
            }   //  FindSetLeftBit check loop for Dword

            //  Check DWord value for SetRightBit
            Store (17, CNTR)
            Store (0xFFFF0000, DWD1)
            While (LLessEqual (CNTR, 32))
            {   //  FindSetRightBit Check loop for DWORD
                FindSetRightBit (DWD1, DRSL)
                If (LNotEqual (DRSL, CNTR))
                    {   Return (0x42)   }

                ShiftLeft (DWD1, 1, DWD1)
                Increment (CNTR)
            }   //  FindSetRightBit Check loop for DWORD

            Return (0)
        }   //  SHFT

        //  Test method called from amlexec
        Method (TEST)
        {   //  TEST

            Store ("++++++++ FdSetOps Test", Debug)

            Store (SHFT (0x80, 8), RSLT)
            If (LNotEqual (RSLT, 0))
                {   Return (RSLT)   }

            Return (0)  //  pass
        }   //  TEST
    }   //  Device FDSO

//
// test MulDivOp.asl
//
    Device (MLDV)
    {
        //  create System Memory Operation Region and field overlays
        OperationRegion (RAM, SystemMemory, 0x400000, 0x100)
        Field (RAM, AnyAcc, NoLock, Preserve)
        {
            SMDW,   32, //  32-bit DWORD
            SMWD,   16, //  16-bit WORD
            SMBY,   8,  //  8-bit BYTE
        }   //  Field(RAM)

        Method (TEST,, Serialized)
        {
            Store ("++++++++ MulDivOp Test", Debug)

            Name (RMDR, 0)
            Name (DWRD, 0x12345678)
            Name (WRD, 0x1234)
            Name (BYT, 0x12)

            //  Test MultiplyOp with DWORD data
            Store (0x12345678, DWRD)
            Multiply (DWRD, 3, DWRD)
            If (LNotEqual (DWRD, 0x369D0368))
                {   Return (DWRD)   }

            //  Test MultiplyOp with WORD data
            Multiply (WRD, 4, WRD)
            If (LNotEqual (WRD, 0x48D0))
                {   Return (WRD)    }

            //  Test MultiplyOp with BYTE data
            Multiply (BYT, 5, BYT)
            If (LNotEqual (BYT, 0x5A))
                {   Return (BYT)    }

            //  Test DivideOp with DWORD data
            Divide (DWRD, 3, DWRD, RMDR)
            If (LNotEqual (DWRD, 0x12345678))
                {   Return (DWRD)   }
            If (LNotEqual (RMDR, 0))
                {   Return (RMDR)   }

            //  Test DivideOp with WORD data
            Divide (WRD, 4, WRD, RMDR)
            If (LNotEqual (WRD, 0x1234))
                {   Return (WRD)    }
            If (LNotEqual (RMDR, 0))
                {   Return (RMDR)   }

            //  Test DivideOp with BYTE data
            Divide (BYT, 5, BYT, RMDR)
            If (LNotEqual (BYT, 0x12))
                {   Return (BYT)    }
            If (LNotEqual (RMDR, 0))
                {   Return (RMDR)   }


            //  test MultiplyOp with DWORD SystemMemory OpRegion
            Store (0x01234567, SMDW)
            Multiply (SMDW, 2, SMDW)
            If (LNotEqual (SMDW, 0x02468ACE))
                {   Return (SMDW)   }

            //  test DivideOp with DWORD SystemMemory OpRegion
            Divide (SMDW, 3, SMDW, RMDR)
            If (LNotEqual (SMDW, 0x00C22E44))
                {   Return (SMDW)   }
            If (LNotEqual (RMDR, 2))
                {   Return (RMDR)   }


            //  test MultiplyOp with WORD SystemMemory OpRegion
            Store (0x0123, SMWD)
            Multiply (SMWD, 3, SMWD)
            If (LNotEqual (SMWD, 0x369))
                {   Return (SMWD)   }

            //  test DivideOp with WORD SystemMemory OpRegion
            Divide (SMWD, 2, SMWD, RMDR)
            If (LNotEqual (SMWD, 0x01B4))
                {   Return (SMWD)   }
            If (LNotEqual (RMDR, 1))
                {   Return (RMDR)   }


            //  test MultiplyOp with BYTE SystemMemory OpRegion
            Store (0x01, SMBY)
            Multiply (SMBY, 7, SMBY)
            If (LNotEqual (SMBY, 0x07))
                {   Return (SMBY)   }

            //  test DivideOp with BYTE SystemMemory OpRegion
            Divide (SMBY, 4, SMBY, RMDR)
            If (LNotEqual (SMBY, 0x01))
                {   Return (SMBY)   }
            If (LNotEqual (RMDR, 3))
                {   Return (RMDR)   }

            Return (0)
        }   //  TEST
    }   //  MLDV

//
// test NBitOps.asl
//
//NAnd - Bit-wise NAnd
//NAndTerm  := NAnd(
//  Source1,    //TermArg=>Integer
//  Source2 //TermArg=>Integer
//  Result  //Nothing | SuperName
//) => Integer
//Source1 and Source2 are evaluated as integer data types, a bit-wise NAND is performed, and the result is optionally
//stored in Result.

//NOr - Bitwise NOr
//NOrTerm   := NOr(
//  Source1,    //TermArg=>Integer
//  Source2 //TermArg=>Integer
//  Result  //Nothing | SuperName
//) => Integer
//Source1 and Source2 are evaluated as integer data types, a bit-wise NOR is performed, and the result is optionally
//stored in Result.
// Not - Not
//NotTerm   := Not(
//  Source, //TermArg=>Integer
//  Result  //Nothing | SuperName
//) => Integer
//Source1 is evaluated as an integer data type, a bit-wise NOT is performed, and the result is optionally stored in
//Result.

//If the Control method is success Zero is returned else a non-zero number is returned

    Device (NBIT)
    {//NBIT

        //Create System Memory Operation Region and field overlays
        OperationRegion (RAM, SystemMemory, 0x400000, 0x100)
        Field (RAM, AnyAcc, NoLock, Preserve)
        {
            SMDW,   32, //  32-bit DWORD
            SMWD,   16, //  16-bit WORD
            SMBY,   8,  //  8-bit BYTE
        }// Field(RAM)


        //And with Byte Data
        Name (BYT1, 0xff)
        Name (BYT2, 0xff)
        Name (BRSL, 0x00)

        //And with Word Data
        Name (WRD1, 0xffff)
        Name (WRD2, 0xffff)
        Name (WRSL, 0x0000)

        //And with DWord Data
        Name (DWD1, 0xffffffff)
        Name (DWD2, 0xffffffff)
        Name (DRSL, 0x00000000)
        Name(RSLT,1)


        Name(ARSL,0x00)
        Name(LRSL,0x00)

        Method(NNDB,2)
        {//NNDB

            Store(0xffffffff,SMDW)
            Store(0xffff,SMWD)
            Store(0xff,SMBY)


            NAnd(Arg0,Arg1,ARSL)
            if(LNotEqual(ARSL,0xfffffffd))
             {Return(11)}

             Store(Arg0,local0)
             Store(Arg1,Local1)

             NAnd(Local0,Local1,LRSL)
                if(LNotEqual(LRSL,0xfffffffd))
             {Return(12)}


            //Byte data
            NAnd(BYT1,BYT2,BRSL)
            if(LNotEqual(BRSL,0xffffff00))
             {Return(13)}

            //Word Data
             NAnd(WRD1,WRD2,WRSL)
            if(LNotEqual(WRSL,0xffff0000))
             {Return(14)}

             //DWord Data
             NAnd(DWD1,DWD2,DRSL)
            if(LNotEqual(DRSL,0x00000000))
             {Return(15)}

             //Byte data
            NAnd(SMBY,0xff,BRSL)
            if(LNotEqual(BRSL,0xffffff00))
             {Return(16)}

            //Word Data
             NAnd(SMWD,0xffff,WRSL)
            if(LNotEqual(WRSL,0xffff0000))
             {Return(17)}

             //DWord Data
             NAnd(SMDW,0xffffffff,DRSL)
            if(LNotEqual(DRSL,0x00000000))
             {Return(18)}

            Return(0)

        }//NNDB

        Method(NNOR,2)
        {//NNOR

            NOr(Arg0,Arg1,ARSL)
            if(LNotEqual(ARSL,0xfffffffd))
             {Return(21)}

            Store(Arg0,local0)
            Store(Arg1,Local1)

            NOr(Local0,Local1,LRSL)
            if(LNotEqual(LRSL,0xfffffffd))
             {Return(22)}


            //Byte data
            NOr(BYT1,BYT2,BRSL)
            if(LNotEqual(BRSL,0xffffff00))
             {Return(23)}

            //Word Data
            NOr(WRD1,WRD2,WRSL)
            if(LNotEqual(WRSL,0xffff0000))
             {Return(24)}

            //DWord Data
            NOr(DWD1,DWD2,DRSL)
            if(LNotEqual(DRSL,0x00000000))
             {Return(25)}

             //System Memory Byte data
            NOr(SMBY,0xff,BRSL)
            if(LNotEqual(BRSL,0xffffff00))
             {Return(26)}

            //System Memory Word Data
            NOr(SMWD,0xffff,WRSL)
            if(LNotEqual(WRSL,0xffff0000))
             {Return(27)}

            //System Memory DWord Data
            NOr(SMDW,0xffffffff,DRSL)
            if(LNotEqual(DRSL,0x00000000))
             {Return(28)}

            Return(0)

        }//NNOR

        Method(NNOT,2)
        {//NNOT

            Or(Arg0,Arg1,ARSL)
            Not(ARSL,ARSL)
            if(LNotEqual(ARSL,0xfffffffd))
             {Return(31)}

            Store(Arg0,local0)
            Store(Arg1,Local1)

            Or(Local0,Local1,LRSL)
            Not(LRSL,LRSL)
            if(LNotEqual(LRSL,0xfffffffd))
             {Return(32)}


            //Byte data
            Or(BYT1,BYT2,BRSL)
            Not(BRSL,BRSL)
            if(LNotEqual(BRSL,0xffffff00))
             {Return(33)}

            //Word Data
            Or(WRD1,WRD2,WRSL)
            Not(WRSL,WRSL)
            if(LNotEqual(WRSL,0xffff0000))
             {Return(34)}

            //DWord Data
            Or(DWD1,DWD2,DRSL)
            Not(DRSL,DRSL)
            if(LNotEqual(DRSL,0x00000000))
             {Return(35)}

             //System Memory Byte data
            Or(SMBY,0xff,BRSL)
            Not(BRSL,BRSL)
            if(LNotEqual(BRSL,0xffffff00))
             {Return(36)}

            //System Memory Word Data
            Or(SMWD,0xffff,WRSL)
            Not(WRSL,WRSL)
            if(LNotEqual(WRSL,0xffff0000))
             {Return(37)}

            //System Memory DWord Data
            Or(SMDW,0xffffffff,DRSL)
            Not(DRSL,DRSL)
            if(LNotEqual(DRSL,0x00000000))
             {Return(38)}

            Return(0)
        }//NNOT


        Method(TEST)
        {

            Store ("++++++++ NBitOps Test", Debug)

            Store(NNDB(2,2),RSLT)
            if(LNotEqual(RSLT,0))
                {Return(RSLT)}

            Store(NNOR(2,2),RSLT)
            if(LNotEqual(RSLT,0))
                {Return(RSLT)}

            Store(NNOT(2,2),RSLT)
            if(LNotEqual(RSLT,0))
                {Return(RSLT)}


           Return(0)
        }

    }//Device NBIT

//
// test ShftOp.asl
//
//ShiftRightTerm    := ShiftRight(
//  Source, //TermArg=>Integer
//  ShiftCount  //TermArg=>Integer
//  Result  //Nothing | SuperName
//) => Integer
//Source and ShiftCount are evaluated as integer data types. Source is shifted right with the most significant bit
//zeroed ShiftCount times. The result is optionally stored into Result.

//ShiftLeft(
//  Source, //TermArg=>Integer
//  ShiftCount  //TermArg=>Integer
//  Result  //Nothing | SuperName
//) => Integer
//Source and ShiftCount are evaluated as integer data types. Source is shifted left with the least significant
//bit zeroed ShiftCount times. The result is optionally stored into Result.

//If the Control method is success Zero is returned else a non-zero number is returned
    Device (SHFT)
    {//SHFT

        //Create System Memory Operation Region and field overlays
        OperationRegion (RAM, SystemMemory, 0x400000, 0x100)
        Field (RAM, AnyAcc, NoLock, Preserve)
        {
            SMDW,   32, //  32-bit DWORD
            SMWD,   16, //  16-bit WORD
            SMBY,   8,  //  8-bit BYTE
        }// Field(RAM)


        Name(SHFC,0x00)

        //And with Byte Data
        Name (BYT1, 0xff)
        Name (BRSL, 0x00)

        //And with Word Data
        Name (WRD1, 0xffff)
        Name (WRSL, 0x0000)

        //And with DWord Data
        Name (DWD1, 0xffffffff)
        Name (DRSL, 0x00000000)

        Name(RSLT,1)

        Name(ARSL,0x00)
        Name(LRSL,0x00)

        Method(SLFT,2)
        {//SLFT

            Store(0xffffffff,SMDW)
            Store(0xffff,SMWD)
            Store(0xff,SMBY)


            //Arg0-> 2 & Arg1->2
            ShiftLeft(Arg0,Arg1,ARSL)
            if(LNotEqual(ARSL,8))
            {Return(11)}

             Store(Arg0,local0)
             Store(Arg1,Local1)

             //Local0->8 and Local1->2
             ShiftLeft(Local0,Local1,LRSL)
                if(LNotEqual(LRSL,8))
             {Return(12)}

            Store(2,SHFC)
            //Byte data
            ShiftLeft(BYT1,SHFC,BRSL)
            if(LNotEqual(BRSL,0x3FC))
             {Return(13)}

            Store(4,SHFC)
            //Word Data
             ShiftLeft(WRD1,SHFC,WRSL)
            if(LNotEqual(WRSL,0xFFFF0))
             {Return(14)}

            Store(8,SHFC)
            //DWord Data
            ShiftLeft(DWD1,SHFC,DRSL)
            if(LNotEqual(DRSL,0xFFFFFF00))
             {Return(15)}


             //System Memory Byte data
            Store(4,SHFC)
            ShiftLeft(SMBY,SHFC,BRSL)
            if(LNotEqual(BRSL,0xFF0))
            {Return(16)}

            //Word Data
            Store(4,SHFC)
            ShiftLeft(SMWD,SHFC,WRSL)
            if(LNotEqual(WRSL,0xffff0))
             {Return(17)}

            //DWord Data
            Store(8,SHFC)
            ShiftLeft(SMDW,SHFC,DRSL)
            if(LNotEqual(DRSL,0xFFFFFF00))
                {Return(18)}

            Return(0)

        }//SLFT

        Method(SRGT,2)
        {//SRGT
            //And with Byte Data
            Store (0xff,BYT1)
            Store (0x00,BRSL)

            //And with Word Data
            Store (0xffff,WRD1)
            Store (0x0000,WRSL)

            //And with DWord Data
            Store(0xffffffff,DWD1)
            Store (0x00000000,DRSL)

            //Reinitialize the result objects
            Store(0x00,ARSL)
            Store(0x00,LRSL)

            Store(0xffffffff,SMDW)
            Store(0xffff,SMWD)
            Store(0xff,SMBY)

            //Arg0-> 2 & Arg1->2
            ShiftRight(Arg0,Arg1,ARSL)
            if(LNotEqual(ARSL,0))
            {Return(21)}

             Store(Arg0,local0)
             Store(Arg1,Local1)

             //Local0->8 and Local1->2
             ShiftRight(Local0,Local1,LRSL)
                if(LNotEqual(LRSL,0))
             {Return(22)}

            Store(2,SHFC)
            //Byte data
            ShiftRight(BYT1,SHFC,BRSL)
            if(LNotEqual(BRSL,0x3F))
             {Return(23)}

            Store(4,SHFC)
            //Word Data
             ShiftRight(WRD1,SHFC,WRSL)
            if(LNotEqual(WRSL,0xFFF))
             {Return(24)}

            Store(8,SHFC)
            //DWord Data
            ShiftRight(DWD1,SHFC,DRSL)
            if(LNotEqual(DRSL,0xFFFFFF))
             {Return(25)}

            //System Memory Byte data
            Store(4,SHFC)
            ShiftRight(SMBY,SHFC,BRSL)
            if(LNotEqual(BRSL,0xF))
            {Return(26)}

            //Word Data
            Store(4,SHFC)
            ShiftRight(SMWD,SHFC,WRSL)
            if(LNotEqual(WRSL,0xFFF))
             {Return(27)}

            //DWord Data
            Store(8,SHFC)
            ShiftRight(SMDW,SHFC,DRSL)
            if(LNotEqual(DRSL,0xFFFFFF))
                {Return(28)}

            Return(0)
        }//SRGT

        //Test method called from amlexec
        Method(TEST)
        {
            Store ("++++++++ ShftOp Test", Debug)

            Store(SLFT(2,2),RSLT)
            if(LNotEqual(RSLT,0))
                {Return(RSLT)}
            Store(SRGT(2,2),RSLT)
            if(LNotEqual(RSLT,0))
                {Return(RSLT)}
           Return(0)
        }

    }//Device SHFT

//
// test Xor.asl and slightly modified
//
//This code tests the XOR opcode term
//Syntax of XOR term
//          XOr(
//                  Source1  //TermArg=>BufferTerm
//                  Source2  //TermArg=>Integer
//                  Result //NameString
//              )
//"Source1" and "Source2" are evaluated as integers, a bit-wise XOR is performed, and the result is optionally stored in
// Result
    Device (XORD)
    {
        //This Method tests XOr operator for all the data types i.e. BYTE, WORD and DWORD
        Method (TEST,, Serialized)
        {
            Store ("++++++++ Xor Test", Debug)

            //Overlay in system memory
            OperationRegion (RAM, SystemMemory, 0x800000, 256)
            Field (RAM, ByteAcc, NoLock, Preserve)
            {
                RES1,   1,  //Offset
                BYT1,   8,  //First BYTE
                BYT2,   8,  //Second BYTE
                RBYT,   8,  //Result Byte
                RES2,   1,  //Offset
                WRD1,   16, //First WORD field
                WRD2,   16, //Second WORD field
                RWRD,   16, //RSLT WORD field
                RES3,   1,  //Offset
                DWD1,   32, //First Dword
                DWD2,   32, //Second Dword
                RDWD,   32, //Result Dword
                RES4,   1,  //Offset
            }

            // Store bits in the single bit fields for checking
            //  at the end
            Store(1, RES1)
            Store(1, RES2)
            Store(1, RES3)
            Store(1, RES4)

            // Check the stored single bits
            if(LNotEqual(RES1, 1))
            {
                Return(1)
            }

            if(LNotEqual(RES2, 1))
            {
                Return(1)
            }

            if(LNotEqual(RES3, 1))
            {
                Return(1)
            }

            if(LNotEqual(RES4, 1))
            {
                Return(1)
            }

            //************************************************
            // (BYT1) Bit1 ->0 and (BYT2)Bit2 -> 0 condition
            Store(0x00,BYT1)
            Store(0x00,BYT2)
            XOr(BYT1,BYT2,Local0)
            Store (Local0, RBYT)
            if(LNotEqual(RBYT,0))
            {   Return(1)}

            // (BYT1) Bit1 ->1 and (BYT2)Bit2 -> 1 condition
            Store(0xff,BYT1)
            Store(0xff,BYT2)
            XOr(BYT1,BYT2,Local0)
            Store (Local0, RBYT)
            if(LNotEqual(RBYT,0))
            {   Return(1)}

            // (BYT1) Bit1 ->1 and (BYT)Bit2 -> 0 condition
            Store(0x55,BYT1)
            Store(0xAA,BYT2)
            XOr(BYT1,BYT2,Local0)
            Store (Local0, RBYT)
            if(LNotEqual(RBYT,0xFF))
            {   Return(1)}

            //(BYT1) Bit1 ->0 and (BYT2)Bit2 -> 1 condition
            Store(0xAA,BYT1)
            Store(0x55,BYT2)
            XOr(BYT1,BYT2,Local0)
            Store (Local0, RBYT)
            if(LNotEqual(RBYT,0xFF))
            {   Return(1)}

            Store(0x12,BYT1)
            Store(0xED,BYT2)

            XOr(BYT1,BYT2,Local0)
            Store (Local0, RBYT)
            if(LNotEqual(RBYT,0xFF))
            {
                Return(1)
            }

            // Store known values for checking later
            Store(0x12, BYT1)
            if(LNotEqual(BYT1, 0x12))
            {
                Return(1)
            }

            Store(0xFE, BYT2)
            if(LNotEqual(BYT2, 0xFE))
            {
                Return(1)
            }

            Store(0xAB, RBYT)
            if(LNotEqual(RBYT, 0xAB))
            {
                Return(1)
            }

            //***********************************************
            // (WRD1) Bit1 ->0 and (WRD2)Bit2 -> 0 condition
            Store(0x0000,WRD1)
            Store(0x0000,WRD2)
            XOr(WRD1,WRD2,RWRD)
            if(LNotEqual(RWRD,0))
            {   Return(1)}

            // (WRD1) Bit1 ->1 and (WRD2)Bit2 -> 1 condition
            Store(0xffff,WRD1)
            Store(0xffff,WRD2)
            XOr(WRD1,WRD2,RWRD)
            if(LNotEqual(RWRD,0))
            {   Return(1)}

            // (WRD1) Bit1 ->1 and (WRD2)Bit2 -> 0 condition
            Store(0x5555,WRD1)
            Store(0xAAAA,WRD2)
            XOr(WRD1,WRD2,RWRD)
            if(LNotEqual(RWRD,0xFFFF))
            {   Return(1)}

            //(WRD1) Bit1 ->0 and (WRD2)Bit2 -> 1 condition
            Store(0xAAAA,WRD1)
            Store(0x5555,WRD2)
            XOr(WRD1,WRD2,RWRD)
            if(LNotEqual(RWRD,0xFFFF))
            {   Return(1)}

            Store(0x1234,WRD1)
            Store(0xEDCB,WRD2)
            XOr(WRD1,WRD2,RWRD)
            if(LNotEqual(RWRD,0xFFFF))
            {   Return(1)}

            // Store known values for checking later
            Store(0x1234, WRD1)
            if(LNotEqual(WRD1, 0x1234))
            {
                Return(1)
            }

            Store(0xFEDC, WRD2)
            if(LNotEqual(WRD2, 0xFEDC))
            {
                Return(1)
            }

            Store(0x87AB, RWRD)
            if(LNotEqual(RWRD, 0x87AB))
            {
                Return(1)
            }


            //**************************************************
            // (DWD1) Bit1 ->0 and (DWD2)Bit2 -> 0 condition
            Store(0x00000000,DWD1)
            Store(0x00000000,DWD2)
            XOr(DWD1,DWD2,RDWD)
            if(LNotEqual(RDWD,0))
            {   Return(1)}

            // (DWD1) Bit1 ->1 and (DWD2)Bit2 -> 1 condition
            Store(0xffffffff,DWD1)
            Store(0xffffffff,DWD2)
            XOr(DWD1,DWD2,RDWD)
            if(LNotEqual(RDWD,0))
            {   Return(1)}

            // (DWD1) Bit1 ->1 and (DWD2)Bit2 -> 0 condition
            Store(0x55555555,DWD1)
            Store(0xAAAAAAAA,DWD2)
            XOr(DWD1,DWD2,RDWD)
            if(LNotEqual(RDWD,0xFFFFFFFF))
            {   Return(1)}

            //(DWD1) Bit1 ->0 and (DWD2)Bit2 -> 1 condition
            Store(0xAAAAAAAA,DWD1)
            Store(0x55555555,DWD2)
            XOr(DWD1,DWD2,RDWD)
            if(LNotEqual(RDWD,0xFFFFFFFF))
            {   Return(1)}

            // (DWD1) Bit1 ->1 and (DWD2)Bit2 -> 0 condition
            Store(0x12345678,DWD1)
            Store(0xEDCBA987,DWD2)
            XOr(DWD1,DWD2,RDWD)
            if(LNotEqual(RDWD,0xFFFFFFFF))
            {   Return(1)}

            Store(0x12345678,DWD1)
            if(LNotEqual(DWD1,0x12345678))
            {
                Return(1)
            }

            Store(0xFEDCBA98,DWD2)
            if(LNotEqual(DWD2,0xFEDCBA98))
            {
                Return(1)
            }

            Store(0x91827364,RDWD)
            if(LNotEqual(RDWD,0x91827364))
            {
                Return(1)
            }

            //****************************************************
            // Check the stored single bits
            if(LNotEqual(RES1, 1))
            {
                Return(1)
            }

            if(LNotEqual(RES2, 1))
            {
                Return(1)
            }

            if(LNotEqual(RES3, 1))
            {
                Return(1)
            }

            if(LNotEqual(RES4, 1))
            {
                Return(1)
            }

            // Change all of the single bit fields to zero
            Store(0, RES1)
            Store(0, RES2)
            Store(0, RES3)
            Store(0, RES4)

            // Now, check all of the fields

            // Byte
            if(LNotEqual(BYT1, 0x12))
            {
                Return(1)
            }

            if(LNotEqual(BYT2, 0xFE))
            {
                Return(1)
            }

            if(LNotEqual(RBYT, 0xAB))
            {
                Return(1)
            }

            // Word
            if(LNotEqual(WRD1, 0x1234))
            {
                Return(1)
            }

            if(LNotEqual(WRD2, 0xFEDC))
            {
                Return(1)
            }

            if(LNotEqual(RWRD, 0x87AB))
            {
                Return(1)
            }

            // Dword
            if(LNotEqual(DWD1, 0x12345678))
            {
                Return(1)
            }

            if(LNotEqual(DWD2, 0xFEDCBA98))
            {
                Return(1)
            }

            if(LNotEqual(RDWD, 0x91827364))
            {
                Return(1)
            }

            // Bits
            if(LNotEqual(RES1, 0))
            {
                Return(1)
            }

            if(LNotEqual(RES2, 0))
            {
                Return(1)
            }

            if(LNotEqual(RES3, 0))
            {
                Return(1)
            }

            if(LNotEqual(RES4, 0))
            {
                Return(1)
            }


            Return(0)
        }   //  TEST
    }   //  XORD

//
// test CrBytFld.asl
//
//  CrBytFld test
//      Test for CreateByteField.
//      Tests creating byte field overlay of buffer stored in Local0.
//      Tests need to be added for Arg0 and Name buffers.
//
    Device (CRBF)
    {   //  Test device name
        Method (TEST)
        {
            Store ("++++++++ CrBytFld Test", Debug)

            //  Local0 is uninitialized buffer with 4 elements
            Store (Buffer (4) {}, Local0)

            //  create Byte Field named BF0 based on Local0 element 0
            CreateByteField (Local0, 0, BF0)

            //  validate CreateByteField did not alter Local0
            Store (ObjectType (Local0), Local1) //  Local1 = Local0 object type
            If (LNotEqual (Local1, 3))  //  Buffer object type value is 3
                {   Return (2)  }

            //  store something into BF0
            Store (1, BF0)

            //  validate Store did not alter Local0 object type
            Store (ObjectType (Local0), Local1) //  Local1 = Local0 object type
            If (LNotEqual (Local1, 3))  //  Buffer object type value is 3
                {   Return (3)  }

            //  verify that the Store into BF0 was successful
            If (LNotEqual (BF0, 1))
                {   Return (4)  }


            //  create Byte Field named BF1 based on Local0 element 1
            CreateByteField (Local0, 1, BF1)

            //  validate CreateByteField did not alter Local0
            Store (ObjectType (Local0), Local1) //  Local1 = Local0 object type
            If (LNotEqual (Local1, 3))  //  Buffer object type value is 3
                {   Return (10) }

            //  store something into BF1
            Store (5, BF1)

            //  validate Store did not alter Local0 object type
            Store (ObjectType (Local0), Local1) //  Local1 = Local0 object type
            If (LNotEqual (Local1, 3))  //  Buffer object type value is 3
                {   Return (11) }

            //  verify that the Store into BF1 was successful
            If (LNotEqual (BF1, 5))
                {   Return (12) }

            //  verify that the Store into BF1 did not alter BF0
            If (LNotEqual (BF0, 1))
                {   Return (13) }


            //  store something into BF0
            Store (0xFFFF, BF0)

            //  verify that the Store into BF0 was successful
            If (LNotEqual (BF0, 0xFF))
                {   Return (20) }

            //  verify that the Store into BF0 did not alter BF1
            If (LNotEqual (BF1, 5))
                {   Return (21) }


            Return (0)
        }   //  TEST
    }   //  CRBF

//
// test IndexOp4.asl
//
//  IndexOp4 test
//      This is just a subset of the many RegionOp/Index Field test cases.
//      Tests access of index fields smaller than 8 bits.
//
    Device (IDX4)
    {   //  Test device name

        //  MADM:   Misaligned Dynamic RAM SystemMemory OperationRegion
        //          Tests OperationRegion memory access using misaligned BYTE,
        //          WORD, and DWORD field element accesses. Validation is performed
        //          using both misaligned field entries and aligned field entries.
        //
        //          MADM returns 0 if all test cases pass or non-zero identifying
        //          the failing test case for debug purposes. This non-zero numbers
        //          are not guaranteed to be in perfect sequence (i.e., test case
        //          index), but are guaranteed to be unique so the failing test
        //          case can be uniquely identified.
        //
        Method (MADM, 1, Serialized)    //  Misaligned Dynamic RAM SystemMemory OperationRegion
        //  Arg0    --  SystemMemory OperationRegion base address
        {   //  MADM:   Misaligned Dynamic RAM SystemMemory OperationRegion
            OperationRegion (RAM, SystemMemory, Arg0, 0x100)
            Field (RAM, DwordAcc, NoLock, Preserve)
            {   //  aligned field definition (for verification)
                DWD0,   32, //  aligned DWORD field
                DWD1,   32      //  aligned DWORD field
            }
            Field (RAM, ByteAcc, NoLock, Preserve)
            {   //  bit access field definition
                BIT0,   1,      //  single bit field entry
                BIT1,   1,      //  single bit field entry
                BIT2,   1,      //  single bit field entry
                BIT3,   1,      //  single bit field entry
                BIT4,   1,      //  single bit field entry
                BIT5,   1,      //  single bit field entry
                BIT6,   1,      //  single bit field entry
                BIT7,   1,      //  single bit field entry
                BIT8,   1,      //  single bit field entry
                BIT9,   1,      //  single bit field entry
                BITA,   1,      //  single bit field entry
                BITB,   1,      //  single bit field entry
                BITC,   1,      //  single bit field entry
                BITD,   1,      //  single bit field entry
                BITE,   1,      //  single bit field entry
                BITF,   1,      //  single bit field entry
                BI10,   1,      //  single bit field entry
                BI11,   1,      //  single bit field entry
                BI12,   1,      //  single bit field entry
                BI13,   1,      //  single bit field entry
                BI14,   1,      //  single bit field entry
                BI15,   1,      //  single bit field entry
                BI16,   1,      //  single bit field entry
                BI17,   1,      //  single bit field entry
                BI18,   1,      //  single bit field entry
                BI19,   1,      //  single bit field entry
                BI1A,   1,      //  single bit field entry
                BI1B,   1,      //  single bit field entry
                BI1C,   1,      //  single bit field entry
                BI1D,   1,      //  single bit field entry
                BI1E,   1,      //  single bit field entry
                BI1F,   1       //  single bit field entry
            }   //  bit access field definition

            Field (RAM, ByteAcc, NoLock, Preserve)
            {   //  two-bit access field definition
                B2_0,   2,      //  single bit field entry
                B2_1,   2,      //  single bit field entry
                B2_2,   2,      //  single bit field entry
                B2_3,   2,      //  single bit field entry
                B2_4,   2,      //  single bit field entry
                B2_5,   2,      //  single bit field entry
                B2_6,   2,      //  single bit field entry
                B2_7,   2,      //  single bit field entry
                B2_8,   2,      //  single bit field entry
                B2_9,   2,      //  single bit field entry
                B2_A,   2,      //  single bit field entry
                B2_B,   2,      //  single bit field entry
                B2_C,   2,      //  single bit field entry
                B2_D,   2,      //  single bit field entry
                B2_E,   2,      //  single bit field entry
                B2_F,   2       //  single bit field entry
            }   //  bit access field definition

            //  initialize memory contents using aligned field entries
            Store (0x5AA55AA5, DWD0)
            Store (0x5AA55AA5, DWD1)

            //  set memory contents to known values using misaligned field entries
            Store (0, BIT0)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BIT0, 0))
                    {   Return (1)  }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA55AA4))
                    {   Return (2)  }

            //  set memory contents to known values using misaligned field entries
            Store (1, BIT1)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BIT1, 1))
                    {   Return (3)  }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA55AA6))
                    {   Return (4)  }

            //  set memory contents to known values using misaligned field entries
            Store (0, BIT2)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BIT2, 0))
                    {   Return (5)  }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA55AA2))
                    {   Return (6)  }

            //  set memory contents to known values using misaligned field entries
            Store (1, BIT3)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BIT3, 1))
                    {   Return (7)  }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA55AAA))
                    {   Return (8)  }

            //  set memory contents to known values using misaligned field entries
            Store (1, BIT4)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BIT4, 1))
                    {   Return (9)  }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA55ABA))
                    {   Return (10) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BIT5)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BIT5, 0))
                    {   Return (11) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA55A9A))
                    {   Return (12) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BIT6)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BIT6, 1))
                    {   Return (13) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA55ADA))
                    {   Return (14) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BIT7)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BIT7, 0))
                    {   Return (15) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA55A5A))
                    {   Return (16) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BIT8)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BIT8, 1))
                    {   Return (17) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA55B5A))
                    {   Return (18) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BIT9)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BIT9, 0))
                    {   Return (19) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA5595A))
                    {   Return (20) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BITA)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BITA, 1))
                    {   Return (21) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA55D5A))
                    {   Return (22) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BITB)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BITB, 0))
                    {   Return (23) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA5555A))
                    {   Return (24) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BITC)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BITC, 0))
                    {   Return (25) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA5455A))
                    {   Return (26) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BITD)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BITD, 1))
                    {   Return (27) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA5655A))
                    {   Return (28) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BITE)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BITE, 0))
                    {   Return (29) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA5255A))
                    {   Return (30) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BITF)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BITF, 1))
                    {   Return (31) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA5A55A))
                    {   Return (32) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BI10)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI10, 0))
                    {   Return (33) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA4A55A))
                    {   Return (34) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BI11)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI11, 1))
                    {   Return (35) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA6A55A))
                    {   Return (36) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BI12)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI12, 0))
                    {   Return (37) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AA2A55A))
                    {   Return (38) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BI13)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI13, 1))
                    {   Return (39) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5AAAA55A))
                    {   Return (40) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BI14)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI14, 1))
                    {   Return (41) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5ABAA55A))
                    {   Return (42) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BI15)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI15, 0))
                    {   Return (43) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5A9AA55A))
                    {   Return (44) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BI16)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI16, 1))
                    {   Return (45) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5ADAA55A))
                    {   Return (46) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BI17)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI17, 0))
                    {   Return (47) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5A5AA55A))
                    {   Return (48) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BI18)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI18, 1))
                    {   Return (49) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5B5AA55A))
                    {   Return (50) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BI19)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI19, 0))
                    {   Return (51) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x595AA55A))
                    {   Return (52) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BI1A)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI1A, 1))
                    {   Return (53) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x5D5AA55A))
                    {   Return (54) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BI1B)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI1B, 0))
                    {   Return (55) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x555AA55A))
                    {   Return (56) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BI1C)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI1C, 0))
                    {   Return (57) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x455AA55A))
                    {   Return (58) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BI1D)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI1D, 1))
                    {   Return (59) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x655AA55A))
                    {   Return (60) }

            //  set memory contents to known values using misaligned field entries
            Store (0, BI1E)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI1E, 0))
                    {   Return (61) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x255AA55A))
                    {   Return (62) }

            //  set memory contents to known values using misaligned field entries
            Store (1, BI1F)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (BI1F, 1))
                    {   Return (63) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55AA55A))
                    {   Return (64) }


            //  set memory contents to known values using misaligned field entries
            Store (3, B2_0)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_0, 3))
                    {   Return (65) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55AA55B))
                    {   Return (66) }

            //  set memory contents to known values using misaligned field entries
            Store (1, B2_1)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_1, 1))
                    {   Return (67) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55AA557))
                    {   Return (68) }

            //  set memory contents to known values using misaligned field entries
            Store (0, B2_2)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_2, 0))
                    {   Return (69) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55AA547))
                    {   Return (70) }

            //  set memory contents to known values using misaligned field entries
            Store (3, B2_3)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_3, 3))
                    {   Return (71) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55AA5C7))
                    {   Return (72) }

            //  set memory contents to known values using misaligned field entries
            Store (3, B2_4)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_4, 3))
                    {   Return (73) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55AA7C7))
                    {   Return (74) }

            //  set memory contents to known values using misaligned field entries
            Store (0, B2_5)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_5, 0))
                    {   Return (75) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55AA3C7))
                    {   Return (76) }

            //  set memory contents to known values using misaligned field entries
            Store (1, B2_6)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_6, 1))
                    {   Return (77) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55A93C7))
                    {   Return (78) }

            //  set memory contents to known values using misaligned field entries
            Store (1, B2_7)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_7, 1))
                    {   Return (79) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55A53C7))
                    {   Return (80) }

            //  set memory contents to known values using misaligned field entries
            Store (0, B2_8)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_8, 0))
                    {   Return (81) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55853C7))
                    {   Return (82) }

            //  set memory contents to known values using misaligned field entries
            Store (1, B2_9)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_9, 1))
                    {   Return (83) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA55453C7))
                    {   Return (84) }

            //  set memory contents to known values using misaligned field entries
            Store (2, B2_A)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_A, 2))
                    {   Return (85) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA56453C7))
                    {   Return (86) }

            //  set memory contents to known values using misaligned field entries
            Store (2, B2_B)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_B, 2))
                    {   Return (87) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA5A453C7))
                    {   Return (88) }

            //  set memory contents to known values using misaligned field entries
            Store (3, B2_C)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_C, 3))
                    {   Return (89) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xA7A453C7))
                    {   Return (90) }

            //  set memory contents to known values using misaligned field entries
            Store (3, B2_D)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_D, 3))
                    {   Return (91) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0xAFA453C7))
                    {   Return (92) }

            //  set memory contents to known values using misaligned field entries
            Store (1, B2_E)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_E, 1))
                    {   Return (93) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x9FA453C7))
                    {   Return (94) }

            //  set memory contents to known values using misaligned field entries
            Store (0, B2_F)
                //  verify memory contents using misaligned field entries
                If (LNotEqual (B2_F, 0))
                    {   Return (95) }
                //  verify memory contents using aligned field entries
                If (LNotEqual (DWD0, 0x1FA453C7))
                    {   Return (96) }


            Return (0)  //  pass
        }   //  MADM:   Misaligned Dynamic RAM SystemMemory OperationRegion

        Method (TEST)
        {
            Store ("++++++++ IndexOp4 Test", Debug)

            //  MADM (Misaligned Dynamic RAM SystemMemory OperationRegion) arguments:
            //      Arg0    --  SystemMemory OperationRegion base address
            Store (MADM (0x800000), Local0)
            If (LNotEqual (Local0, 0))      //  MADM returns zero if successful
                {   Return (Local0) }       //  failure:    return MADM error code

            Return (Local0)
        }   //  TEST
    }   //  IDX4

//
// test Event.asl
//
//  EventOp, ResetOp, SignalOp, and WaitOp test cases.
//
    Device (EVNT)
    {
        Event (EVNT)    //  event synchronization object

        Method (TEVN, 1)
        //  Arg0:   time to Wait for event in milliseconds
        {   //  TEVN control method to test ResetOp, SignalOp, and WaitOp
            //  reset EVNT to initialization (zero) state
            Reset (EVNT)

            //  prime EVNT with two outstanding signals
            Signal (EVNT)
            Signal (EVNT)


            //  acquire existing signal
            Store (Wait (EVNT, Arg0), Local0)

            //  validate Local0 is a Number
            Store (ObjectType (Local0), Local1)
            If (LNotEqual (Local1, 1))  //  Number is type 1
                {   Return (0x21)   }       //  Local1 indicates Local0 is not a Number

            If (LNotEqual (Local0, 0))  //  Number is type 1
                {   Return (0x22)   }       //  timeout occurred without acquiring signal

            Store ("Acquire 1st existing signal PASS", Debug)


            //  acquire existing signal
            Store (Wait (EVNT, Arg0), Local0)

            //  validate Local0 is a Number
            Store (ObjectType (Local0), Local1)
            If (LNotEqual (Local1, 1))  //  Number is type 1
                {   Return (0x31)   }       //  Local1 indicates Local0 is not a Number

            If (LNotEqual (Local0, 0))  //  Number is type 1
                {   Return (0x32)   }       //  timeout occurred without acquiring signal

            Store ("Acquire 2nd existing signal PASS", Debug)


            //  ensure WaitOp timeout test cases do not hang
            if (LEqual (Arg0, 0xFFFF))
                {   Store (0xFFFE, Arg0)    }

            //  acquire non-existing signal
            Store (Wait (EVNT, Arg0), Local0)

            //  validate Local0 is a Number
            Store (ObjectType (Local0), Local1)
            If (LNotEqual (Local1, 1))  //  Number is type 1
                {   Return (0x41)   }       //  Local1 indicates Local0 is not a Number

            If (LEqual (Local0, 0))     //  Number is type 1
                {   Return (0x42)   }       //  non-existent signal was acquired

            Store ("Acquire signal timeout PASS", Debug)


            //  prime EVNT with two outstanding signals
            Signal (EVNT)
            Signal (EVNT)

            //  reset EVNT to initialization (zero) state
            Reset (EVNT)

            //  acquire non-existing signal
            Store (Wait (EVNT, Arg0), Local0)

            //  validate Local0 is a Number
            Store (ObjectType (Local0), Local1)
            If (LNotEqual (Local1, 1))  //  Number is type 1
                {   Return (0x51)   }       //  Local1 indicates Local0 is not a Number

            If (LEqual (Local0, 0))     //  Number is type 1
                {   Return (0x52)   }       //  non-existent signal was acquired

            Store ("Reset signal PASS", Debug)


            //  acquire non-existing signal using Lvalue timeout
            Store (Wait (EVNT, Zero), Local0)

            //  validate Local0 is a Number
            Store (ObjectType (Local0), Local1)
            If (LNotEqual (Local1, 1))  //  Number is type 1
                {   Return (0x61)   }       //  Local1 indicates Local0 is not a Number

            If (LEqual (Local0, 0))     //  Number is type 1
                {   Return (0x62)   }       //  non-existent signal was acquired

            Store ("Zero Lvalue PASS", Debug)


            //  acquire non-existing signal using Lvalue timeout
            Store (Wait (EVNT, One), Local0)

            //  validate Local0 is a Number
            Store (ObjectType (Local0), Local1)
            If (LNotEqual (Local1, 1))  //  Number is type 1
                {   Return (0x71)   }       //  Local1 indicates Local0 is not a Number

            If (LEqual (Local0, 0))     //  Number is type 1
                {   Return (0x72)   }       //  non-existent signal was acquired

            Store ("One Lvalue PASS", Debug)

            //  Lvalue Event test cases
    // ILLEGAL SOURCE OPERAND        Store (EVNT, Local2)

            //  validate Local2 is an Event
            Store (ObjectType (EVNT), Local1)
            If (LNotEqual (Local1, 7))  //  Event is type 7
                {   Return (0x81)   }       //  Local1 indicates Local0 is not a Number

            //  reset EVNT to initialization (zero) state
            Reset (EVNT)

            //  prime EVNT with two outstanding signals
            Signal (EVNT)

            //  acquire existing signal
            Store (Wait (EVNT, Arg0), Local0)

            //  validate Local0 is a Number
            Store (ObjectType (Local0), Local1)
            If (LNotEqual (Local1, 1))  //  Number is type 1
                {   Return (0x82)   }       //  Local1 indicates Local0 is not a Number

            If (LNotEqual (Local0, 0))  //  Number is type 1
                {   Return (0x83)   }       //  timeout occurred without acquiring signal

            Store ("Acquire Lvalue existing signal PASS", Debug)


            //  acquire non-existing signal
            Store (Wait (EVNT, Arg0), Local0)

            //  validate Local0 is a Number
            Store (ObjectType (Local0), Local1)
            If (LNotEqual (Local1, 1))  //  Number is type 1
                {   Return (0x84)   }       //  Local1 indicates Local0 is not a Number

            If (LEqual (Local0, 0))     //  Number is type 1
                {   Return (0x85)   }       //  non-existent signal was acquired

            Store ("Acquire Lvalue signal timeout PASS", Debug)


            Return (0)  //  success
        }   //  TEVN control method to test ResetOp, SignalOp, and WaitOp

        Method (TEST)
        {
            Store ("++++++++ Event Test", Debug)

            Store (TEVN (100), Local0)

            Return (Local0)
        }   //  TEST
    }   //  EVNT

//
// test SizeOfLv.asl
//
//  Test for SizeOf (Lvalue)
//
//  This next section will contain the packages that the SizeOfOp will be
//  exercised on. The first one, PKG0, is a regular package of 3 elements.
//  The 2nd one, PKG1, is a nested package with 3 packages inside it, each
//  with 3 elements. It is expected that SizeOf operator will return the
//  same value for these two packages since they both have 3 elements. The
//  final package, PKG2, has 4 elements and the SizeOf operator is expected
//  to return different results for this package.

    Name (PKG0,
        Package (3)
        {0x0123, 0x4567, 0x89AB}
    )   //  PKG0

    Name (PKG1,
        Package (3)
        {
            Package (3) {0x0123, 0x4567, 0x89AB},
            Package (3) {0xCDEF, 0xFEDC, 0xBA98},
            Package (3) {0x7654, 0x3210, 0x1234}
        }
    )   //  PKG1

    Name (PKG2,
        Package (4)
        {0x0123, 0x4567, 0x89AB, 0x8888}
    )   //  PKG2

    Name (PKG3,
        Package (5)
        {0x0123, 0x4567, 0x89AB, 0x8888, 0x7777}
    )   //  PKG3

//  End Packages    **********************************************************

//  The following section will declare the data strings that will be used to
//  exercise the SizeOf operator. STR0 and STR1 are expected to be equal,
//  STR2 is expected to have a different SizeOf value than STR0 and STR1.

    Name (STR0, "ACPI permits very flexible methods of expressing a system")

    Name (STR1, "MIKE permits very flexible methods of expressing a system")

    Name (STR2, "Needless to say, Mike and ACPI are frequently at odds")

//  This string is being made in case we want to do a SizeOf comparison
//  between strings and packages or buffers
    Name (STR3, "12345")

//  End Strings     **********************************************************

//  The following section will declare the buffers that will be used to exercise
//  the SizeOf operator.

    Name (BUF0, Buffer (10) {})
    Name (BUF1, Buffer (10) {})
    Name (BUF2, Buffer (8)  {})
    Name (BUF3, Buffer (5)  {})

//  End Buffers     **********************************************************
    Device (SZLV)
    {

        Method (CMPR, 2)
        {
            //  CMPR is passed two arguments. If unequal, return 1 to indicate
            //  that, otherwise return 0 to indicate SizeOf each is equal.

            Store (0x01, Local0)

            if (LEqual (SizeOf(Arg0), SizeOf(Arg1)))
            {
                Store (0x00, Local0)
            }

            return (Local0)
        }   //  CMPR


        Method (TEST)
        {

            Store ("++++++++ SizeOfLv Test", Debug)

            //  TBD:    SizeOf ("string")
            //          SizeOf (Buffer)
            //          SizeOf (Package)
            //          SizeOf (String)
            //          SizeOf (STR0)   --  where Name (STR0,...) -- lot's of cases
            //              buffer, string, package,
            //          SizeOf (METH) -- where METH control method returns
            //              buffer, string, package,

            //  TBD:    SLOC [SizeOf (Local0)] -- dup SARG

            //  Compare the elements that we expect to be the same. Exit out with an error
            //  code on the first failure.
            if (LNotEqual (0x00, CMPR (STR0, STR1)))
            {
                Return (0x01)
            }

            if (LNotEqual (0x00, CMPR (STR3, BUF3)))
            {
                Return (0x02)
            }

            if (LNotEqual (0x00, CMPR (STR3, PKG3)))
            {
                Return (0x03)
            }

            //  In the following section, this test will cover the SizeOf
            //  operator for Local values.
            //  In this case, both Local0 and Local1 should have the same Size
            Store (STR0, Local0)
            Store (STR1, Local1)

            if (LNotEqual (SizeOf (Local0), SizeOf (Local1)))
            {
                Return (0x04)
            }

            //  Now create a case where Local0 and Local1 are different
            Store (STR2, Local1)

            if (LEqual (SizeOf (Local0), SizeOf (Local1)))
            {
                Return (0x05)
            }

            //  Finally, check for the return of SizeOf for a known Buffer. Just
            //  in case we magically pass above cases due to all Buffers being Zero
            //  bytes in size, or Infinity, etc.
            if (LNotEqual (0x05, SizeOf (BUF3)))
            {
                Return (0x06)
            }

            Return (0)
        }   //  TEST
    }   //  SZLV


//
// test BytField.asl
//
//  BytField test
//      This is just a subset of the many RegionOp/Index Field test cases.
//      Tests access of TBD.
//
    Scope (\_SB)    //  System Bus
    {   //  _SB system bus
        Device (BYTF)
        {   //  Test device name
            Method (TEST)
            {
                Store ("++++++++ BytField Test", Debug)

                Return (\_TZ.C19B.RSLT)
            }   //  TEST
        }   //  BYTF

        Device (C005)
        {   //  Device C005
            Device (C013)
            {   //  Device C013
            }   //  Device C013
        }   //  Device C005

        Method (C115)
        {   //  C115 control method
            Acquire (\_GL, 0xFFFF)
            Store (\_SB.C005.C013.C058.C07E, Local0)
            Release (\_GL)
            And (Local0, 16, Local0)
            Store (ShiftRight (Local0, 4, ), Local1)
            If (LEqual (Local1, 0))
                {   Return (1)  }
            Else
                {   Return (0)  }
        }   //  C115 control method
    }   //  _SB system bus

    OperationRegion (C018, SystemIO, 0x5028, 4)
    Field (C018, AnyAcc, NoLock, Preserve)
    {   //  Field overlaying C018
        C019,   32
    }   //  Field overlaying C018

    OperationRegion (C01A, SystemIO, 0x5030, 4)
    Field (C01A, ByteAcc, NoLock, Preserve)
    {   //  Field overlaying C01A
        C01B,   8,
        C01C,   8,
        C01D,   8,
        C01E,   8
    }   //  Field overlaying C01A

    Mutex (\C01F, 0)
    Name (\C020, 0)
    Name (\C021, 0)

    Method (\C022, 0)
    {   //  \C022 control method
        Acquire (\C01F, 0xFFFF)
        If (LEqual (\C021, 0))
        {
            Store (C019, Local0)
            And (Local0, 0xFFFEFFFE, Local0)
            Store (Local0, C019)
            Increment (\C021)
        }
        Release (\C01F)
    }   //  \C022 control method

    Scope (\_SB.C005.C013)
    {   //  Scope \_SB.C005.C013
        Device (C058)
        {   //  Device C058
            Name (_HID, "*PNP0A06")

            OperationRegion (C059, SystemIO, 0xE0, 2)
            Field (C059, ByteAcc, NoLock, Preserve)
            {   //  Field overlaying C059
                C05A,   8,
                C05B,   8
            }   //  Field overlaying C059

            OperationRegion (C05C, SystemIO, 0xE2, 2)
            Field (C05C, ByteAcc, NoLock, Preserve)
            {   //  Field overlaying C05C
                C05D,   8,
                C05E,   8
            }   //  Field overlaying C05C
            IndexField (C05D, C05E, ByteAcc, NoLock, Preserve)
            {   //  IndexField overlaying C05D/C05E
                ,       0x410,  //  skip
                C05F,   8,
                C060,   8,
                C061,   8,
                C062,   8,
                C063,   8,
                C064,   8,
                C065,   8,
                C066,   8,
                C067,   8,
                C068,   8,
                C069,   8,
                C06A,   8,
                C06B,   8,
                C06C,   8,
                C06D,   8,
                C06E,   8,
                ,       0x70,       //  skip
                C06F,   8,
                C070,   8,
                C071,   8,
                C072,   8,
                C073,   8,
                C074,   8,
                C075,   8,
                C076,   8,
                C077,   8,
                C078,   8,
                C079,   8,
                C07A,   8,
                C07B,   8,
                C07C,   8,
                C07D,   8,
                C07E,   8
            }   //  IndexField overlaying C05D/C05E

            OperationRegion (C07F, SystemIO, 0xE4, 2)
            Field (C07F, ByteAcc, NoLock, Preserve)
            {   //  Field overlaying C07F
                C080,   8,
                C081,   8
            }   //  Field overlaying C07F

            OperationRegion (C082, SystemIO, 0xE0, 1)
            Field (C082, ByteAcc, NoLock, Preserve)
            {   //  Field overlaying C082
                C083,   8
            }   //  Field overlaying C082

            OperationRegion (C084, SystemIO, 0xFF, 1)
            Field (C084, ByteAcc, NoLock, Preserve)
            {   //  Field overlaying C084
                C085,   8
            }   //  Field overlaying C084

            OperationRegion (C086, SystemIO, 0xFD, 1)
            Field (C086, ByteAcc, NoLock, Preserve)
            {   //  Field overlaying C086
                C087,   8
            }   //  Field overlaying C086

            Mutex (C088, 0)
            Mutex (C089, 0)
            Mutex (C08A, 0)
            Mutex (C08B, 0)
            Mutex (C08C, 0)
            Mutex (C08D, 0)

            Name (C08E, 0xFFFFFFFD)
            Name (C08F, 0)

            Method (C0AA, 4)
            {   //  C0AA control method
                Store (Buffer (4) {}, Local7)
                CreateByteField (Local7, 0, C0AB)
                CreateByteField (Local7, 1, C0AC)
                CreateByteField (Local7, 2, C0AD)
                CreateByteField (Local7, 3, C0AE)
                Acquire (^C08B, 0xFFFF)
                Acquire (\_GL, 0xFFFF)
                \C022 ()
                Store (1, \_SB.C005.C013.C058.C06B)
                While (LNot (LEqual (0, \_SB.C005.C013.C058.C06B)))
                    {   Stall (100) }
                Store (Arg3, \_SB.C005.C013.C058.C06E)
                Store (Arg2, \_SB.C005.C013.C058.C06D)
                Store (Arg1, \_SB.C005.C013.C058.C06C)
                Store (Arg0, \_SB.C005.C013.C058.C06B)
                While (LNot (LEqual (0, \_SB.C005.C013.C058.C06B)))
                    {   Stall (100) }
                Store (\_SB.C005.C013.C058.C06E, C0AB)
                Store (\_SB.C005.C013.C058.C06D, C0AC)
                Store (\_SB.C005.C013.C058.C06C, C0AD)
                Store (\_SB.C005.C013.C058.C06B, C0AE)
                If (LNot (LEqual (Arg0, 23)))
                {
                    Store (2, \_SB.C005.C013.C058.C06B)
                    Stall (100)
                }
                Release (\_GL)
                Release (^C08B)
                Return (Local7)
            }   //  C0AA control method
        }   //  Device C058
    }   //  Scope \_SB.C005.C013

    Scope (\_TZ)
    {   //  \_TZ thermal zone scope
        Name (C18B, Package (2)
        {
            Package (2)
            {
                Package (5) {0x05AC, 0x0CD2, 0x0D68, 0x0DE0, 0x0E4E},
                Package (5) {0x0D04, 0x0D9A, 0x0DFE, 0x0E80, 0x0FA2}
            },
            Package (2)
            {
                Package (5) {0x05AC, 0x0CD2, 0x0D68, 0x0DE0, 0x0E4E},
                Package (5) {0x0D04, 0x0D9A, 0x0DFE, 0x0E80, 0x0FA2}
            }
        })  //  C18B

        Name (C18C, Package (2)
        {
            Package (2)
            {
                Package (3) {0x64, 0x4B, 0x32},
                Package (3) {0x64, 0x4B, 0x32}
            }
        })  //  C81C

        Name (C18D, 0)
        Name (C18E, 0)
        Name (C18F, 0)
        Name (C190, 0)
        Name (C191, 3)
        Name (C192, 0)
        Name (C193, 1)
        Name (C194, 2)
        Mutex (C195, 0)
        Name (C196, 1)
        Name (C197, 0x0B9C)
        Name (C198, 0x0B9C)
        Name (C199, 0xFFFFFFFD)
        Name (C19A, 0)

        Device (C19B)
        {   //  Device C19B
            Name (RSLT, 0)  //  default to zero

            Method (XINI)
            {   //  _INI control method (Uses Global Lock -- can't run under AcpiExec)
                Store (\_SB.C115, C19A)
                \_TZ.C19C._SCP (0)
                Subtract (0x0EB2, 0x0AAC, Local1)   //  Local1 = AACh - EB2h
                Divide (Local1, 10, Local0, Local2) //  Local0 = Local1 / 10
                                                                //  Local2 = Local1 % 10
                \_SB.C005.C013.C058.C0AA (14, Local2, 0, 0)
                Store
                    (DerefOf (Index (DerefOf (Index (\_TZ.C18C, C19A, )), 0, )), C18D)
                Store
                    (DerefOf (Index (DerefOf (Index (\_TZ.C18C, C19A, )), 1, )), C18E)
                Store
                    (DerefOf (Index (DerefOf (Index (\_TZ.C18C, C19A, )), 2, )), C18F)

                Store (1, RSLT) //  set RSLT to 1 if _INI control method completes
            }   //  _INI control method

            //  PowerResource (C19D) {...}
        }   //  Device C19B

        ThermalZone (C19C)
        {
            Method (_SCP, 1)
            {   //  _SCP control method
                Store (Arg0, Local0)
                If (LEqual (Local0, 0))
                {
                    Store (0, \_TZ.C192)
                    Store (1, \_TZ.C193)
                    Store (2, \_TZ.C194)
                    Store (3, \_TZ.C191)
                }
                Else
                {
                    Store (0, \_TZ.C191)
                    Store (1, \_TZ.C192)
                    Store (2, \_TZ.C193)
                    Store (3, \_TZ.C194)
                }
            }   //  _SCP control method
        }   //  ThermalZone C19C
    }   //  \_TZ thermal zone scope


//
// test DwrdFld.asl
//
    Name (BUFR, buffer(10) {0,0,0,0,0,0,0,0,0,0} )

    Device (DWDF)
    {
        Method (TEST)
        {
            Store ("++++++++ DwrdFld Test", Debug)

            CreateByteField (BUFR, 0, BYTE)
            Store (0xAA, BYTE)

            CreateWordField (BUFR, 1, WORD)
            Store (0xBBCC, WORD)

            CreateDWordField (BUFR, 3, DWRD)
            Store (0xDDEEFF00, DWRD)

            CreateByteField (BUFR, 7, BYT2)
            Store (0x11, BYT2)

            CreateWordField (BUFR, 8, WRD2)
            Store (0x2233, WRD2)

            Return (0)

        }   //  End Method TEST
    }   //  Device DWDF

    //
    // test DivAddx.asl
    //
    Name (B1LO, 0xAA)
    Name (B1HI, 0xBB)

    Method (MKW_, 2)
    {   //  This control method will take two bytes and make them into a WORD

        Multiply (B1HI, 256, Local0)    //  Make high byte.....high
        Or (Local0, B1LO, Local0)       //  OR in the low byte
        Return (Local0)                 //  Return the WORD

    }   //  MKW_

    Device (DVAX)
    {
        Method (TEST)
        {

            Store ("++++++++ DivAddx Test", Debug)

            Store (25, B1LO)
            Store (0, B1HI)

            //  We'll multiply 25 * 3 to get 75, add 99 to it then divide
            //  by 100. We expect to get 74 for the remainder and 1 for
            //  the quotient.
            Divide(
                Add (Multiply (3, MKW_ (B1LO, B1HI)), 0x63),
                            //  Dividend,
                100,        //  Divisor
                Local4,     //  Remainder
                Local2)     //  Quotient

            If (LAnd (LEqual (74, Local4), LEqual (1, Local2)))
            {   //  Indicate Pass
                Store (0x00, Local0)
            }

            Else
            {   //  Indicate Fail
                Store (0x01, Local0)
            }

            Return (Local0)
        }   //  End Method TEST
    }   //  Device DVAX

//
// test IndexFld.asl (IndexOp6.asl)
//
//  IndexFld test
//      This is just a subset of the many RegionOp/Index Field test cases.
//      Tests index field element AccessAs macro.
//      Also tests name resolution of index field elements with same names
//      but different namespace scopes.
//
    Device (IDX6)
    {   //  Test device name

        OperationRegion (SIO, SystemIO, 0x100, 2)
        Field (SIO, ByteAcc, NoLock, Preserve)
        {
            INDX,   8,
            DATA,   8
        }
        IndexField (INDX, DATA, AnyAcc, NoLock, WriteAsOnes)
        {
            AccessAs (ByteAcc, 0),
            IFE0,   8,
            IFE1,   8,
            IFE2,   8,
            IFE3,   8,
            IFE4,   8,
            IFE5,   8,
            IFE6,   8,
            IFE7,   8,
            IFE8,   8,
            IFE9,   8,
        }

        Device (TST_)
        {   //  TST_:   provides a different namespace scope for IFE0 and IFE1
            OperationRegion (SIO2, SystemIO, 0x100, 2)
            Field (SIO2, ByteAcc, NoLock, Preserve)
            {
                IND2,   8,
                DAT2,   8
            }
            IndexField (IND2, DAT2, AnyAcc, NoLock, WriteAsOnes)
            {
                IFE0,   8,  //  duplicate IndexField name with different scope
                IFE1,   8
            }
        }   //  TST_:   provides a different namespace scope for IFE0 and IFE1

        Method (TEST)
        {
            Store ("++++++++ IndexOp6 Test", Debug)

            Store (IFE0, Local0)
            Store (IFE1, Local1)
            Store (IFE2, Local2)

            //  validate name resolution of IndexFields with different scopes
            Store (\IDX6.IFE0, Local3)
            Store (\IDX6.IFE1, Local4)
            //  verioading of namespace can resolve following names
            Store (\IDX6.TST_.IFE0, Local5)
            Store (\IDX6.TST_.IFE1, Local6)

            Return (0)
        }   //  TEST
    }   //  IDX6

//
// test IndexOp5.asl
//
//  IndexOp5 test
//      This is just a subset of the many RegionOp/Index Field test cases.
//      Tests copying string into buffer then performing IndexOp on result.
//
    Device (IDX5)
    {   //  Test device name

        Name (OSFL, 0)  //  0 == Windows 98, 1 == Windows NT

        //  MCTH is a control method to compare two strings. It returns
        //  zero if the strings mismatch, or 1 if the strings match.
        //  This exercises the test case of copying a string into a buffer
        //  and performing an IndexOp on the resulting buffer.
        Method (MCTH, 2, Serialized)    //  Control Method to compare two strings
        {   //  MCTH:   Control Method to compare two strings
            //  Arg0:       first string to compare
            //  Arg1:       second string to compare
            //  Return: zero if strings mismatch, 1 if strings match

            //  check if first string's length is less than second string's length
            If (LLess (SizeOf (Arg0), SizeOf (Arg1)))
                {   Return (0)  }

            //  increment length to include NULL termination character
            Add (SizeOf (Arg0), 1, Local0)  //  Local0 = strlen(Arg0) + 1

            //  create two buffers of size Local0 [strlen(Arg0)+1]
            Name (BUF0, Buffer (Local0) {})
            Name (BUF1, Buffer (Local0) {})

            //  copy strings into buffers
            Store (Arg0, BUF0)
            Store (Arg1, BUF1)

            //  validate BUF0 and BUF1 are still buffers
            Store (ObjectType (BUF0), Local1)
            If (LNotEqual (Local1, 3))  //  Buffer is type 3
                {   Return (20) }
            Store (ObjectType (BUF1), Local1)
            If (LNotEqual (Local1, 3))  //  Buffer is type 3
                {   Return (21) }

            // Decrement because the Index base below is zero based
            //  while Local0 length is one based.
            Decrement (Local0)

            While (Local0)
            {   //  loop through all BUF0 buffer elements
                Decrement (Local0)

                //  check if BUF0[n] == BUF1[n]
                If (LEqual (DerefOf (Index (BUF0, Local0, )),
                        DerefOf (Index (BUF1, Local0, ))))
                    {   }   //  this is how the code was really implemented
                Else
                    {   Return (Zero)   }
            }   //  loop through all BUF0 buffer elements

            Return (One)    //  strings / buffers match
        }   //  MCTH:   Control Method to compare two strings


        Method (TEST)
        {
            Store ("++++++++ IndexOp5 Test", Debug)

            If (MCTH (\_OS, "Microsoft Windows NT"))
                {   Store (1, OSFL) }

            If (LNotEqual (OSFL, 1))
                {   Return (11) }

            Return (0)
        }   //  TEST
    }   //  IDX5

//
// test IndexOp.asl
//
    Scope (\_SB)    //  System Bus
    {   //  _SB system bus

        Method (C097)
            {   Return (1)  }

        Device (PCI2)
        {   //  Root PCI Bus
            Name (_HID, EISAID("PNP0A03"))
            Name (_ADR, 0x00000000)
            Name (_CRS, Buffer(26)  {"\_SB_.PCI2._CRS..........."})
            Method (_STA)   {Return (0x0F)}

            Device (ISA)
            {   //  ISA bridge
                Name (_ADR, 0x00030000)     //  ISA bus ID

                Device (EC0)
                {   //  Embedded Controller
                    Name (_GPE, 0)              //  EC use GPE0
                    Name (_ADR, 0x0030000)  //  PCI address

                    Method (_STA,0)         //  EC Status
                        {   Return(0xF) }       //  EC is functioning

                    Name (_CRS, ResourceTemplate()
                        {
                            IO (Decode16, 0x62, 0x62, 1, 1)
                            IO (Decode16, 0x66, 0x66, 1, 1)
                        }
                    )

                //  create EC's region and field
                    OperationRegion (RAM, SystemMemory, 0x400000, 0x100)
                    Field (RAM, AnyAcc, NoLock, Preserve)
                    {
                        //  AC information
                        ADP,    1,      //  AC Adapter 1:On-line, 0:Off-line
                        AFLT,   1,      //  AC Adapter Fault  1:Fault  0:Normal
                        BAT0,   1,      //  BAT0  1:present, 0:not present
                        ,       1,      //  reserved
                        ,       28, //  filler to force DWORD alignment

                        //  CMBatt information
                        BPU0,   32, //  Power Unit
                        BDC0,   32, //  Designed Capacity
                        BFC0,   32, //  Last Full Charge Capacity
                        BTC0,   32, //  Battery Technology
                        BDV0,   32, //  Design Voltage
                        BST0,   32, //  Battery State
                        BPR0,   32, //  Battery Present Rate
                                        //  (Designed Capacity)x(%)/{(h)x100}
                        BRC0,   32, //  Battery Remaining Capacity
                                        //  (Designed Capacity)(%)^100
                        BPV0,   32, //  Battery Present Voltage
                        BTP0,   32, //  Trip Point
                        BCW0,   32, //  Design capacity of Warning
                        BCL0,   32, //  Design capacity of Low
                        BCG0,   32, //  capacity granularity 1
                        BG20,   32, //  capacity granularity 2
                        BMO0,   32, //  Battery model number field
                        BIF0,   32, //  OEM Information(00h)
                        BSN0,   32, //  Battery Serial Number
                        BTY0,   32, //  Battery Type (e.g., "Li-Ion")
                        BTY1,   32      //  Battery Type (e.g., "Li-Ion")
                    }   //  Field
                }   //  EC0: Embedded Controller
            }   //  ISA bridge
        }   //  PCI2 Root PCI Bus

        Device (IDX0)
        {   //  Test device name
            Name (_HID, EISAID ("PNP0C0A"))     //  Control Method Battey ID
            Name (_PCL, Package() {\_SB})
            Method (_STA)
            {
                //  _STA bits 0-3 indicate existence of battery slot
                //  _STA bit 4 indicates battery (not) present
                If (\_SB.PCI2.ISA.EC0.BAT0)
                    {   Return (0x1F)   }   //  Battery present
                else
                    {   Return (0x0F)   }   //  Battery not present
            }   //  _STA

            Method (_BIF,, Serialized)
            {
                Name (BUFR, Package(13) {})
                Store (\_SB.PCI2.ISA.EC0.BPU0, Index (BUFR,0))  //  Power Unit
                Store (\_SB.PCI2.ISA.EC0.BDC0, Index (BUFR,1))  //  Designed Capacity
                Store (\_SB.PCI2.ISA.EC0.BFC0, Index (BUFR,2))  //  Last Full Charge Capa.
                Store (\_SB.PCI2.ISA.EC0.BTC0, Index (BUFR,3))  //  Battery Technology
                Store (\_SB.PCI2.ISA.EC0.BDV0, Index (BUFR,4))  //  Designed Voltage
                Store (\_SB.PCI2.ISA.EC0.BCW0, Index (BUFR,5))  //  Designed warning level
                Store (\_SB.PCI2.ISA.EC0.BCL0, Index (BUFR,6))  //  Designed Low level
                Store (\_SB.PCI2.ISA.EC0.BCG0, Index (BUFR,7))  //  Capacity granularity 1
                Store (\_SB.PCI2.ISA.EC0.BG20, Index (BUFR,8))  //  Capacity granularity 2

                Store ("", Index (BUFR,9))                              //  Model Number

                Store ("", Index (BUFR,10))                         //  Serial Number

                Store ("LiOn", Index (BUFR,11))                     //  Battery Type

                Store ("Chicony", Index (BUFR,12))                  //  OEM Information

                Return (BUFR)
            }   //  _BIF

            Method (_BST,, Serialized)
            {
                Name (BUFR, Package(4) {1, 0x100, 0x76543210, 0x180})
                Return (BUFR)
            }   //  _BST

            Method (_BTP,1)
            {
                Store (arg0, \_SB.PCI2.ISA.EC0.BTP0)    //  Set Battery Trip point
            }

            Method (TEST,, Serialized)
            {

                Store ("++++++++ IndexOp Test", Debug)

                //  test storing into uninitialized package elements
                Name (PBUF, Package(4) {})  //  leave uninitialized
                Store (0x01234567, Index (PBUF,0))
                Store (0x89ABCDEF, Index (PBUF,1))
                Store (0xFEDCBA98, Index (PBUF,2))
                Store (0x76543210, Index (PBUF,3))

                //  verify values stored into uninitialized package elements
                If (LNotEqual (DerefOf (Index (PBUF,0)), 0x01234567))
                    {   Return (0x10)   }

                If (LNotEqual (DerefOf (Index (PBUF,1)), 0x89ABCDEF))
                    {   Return (0x11)   }

                If (LNotEqual (DerefOf (Index (PBUF,2)), 0xFEDCBA98))
                    {   Return (0x12)   }

                If (LNotEqual (DerefOf (Index (PBUF,3)), 0x76543210))
                    {   Return (0x13)   }


                //  store _BIF package return value into Local0
                Store (_BIF, Local0)

                //  save Local0 object type value into Local1
                Store (ObjectType (Local0), Local1)

                //  validate Local0 is a Package
                If (LNotEqual (Local1, 4))  //  Package type is 4
                    {   Return (0x21)   }   //  failure


                //  test storing into buffer field elements
                Name (BUFR, Buffer(16)
                    {   //  initial values
                        00, 00, 00, 00, 00, 00, 00, 00,
                        00, 00, 00, 00, 00, 00, 00, 00,
                    }
                )   //  BUFR
                //  test storing into buffer field elements
                Store (0x01234567, Index (BUFR,0))  //  should only store 0x67
                Store (0x89ABCDEF, Index (BUFR,4))  //  should only store 0xEF
                Store (0xFEDCBA98, Index (BUFR,8))  //  should only store 0x98
                Store (0x76543210, Index (BUFR,12)) //  should only store 0x10

                //  verify storing into buffer field elements
                If (LNotEqual (DerefOf (Index (BUFR,0)), 0x67))
                    {   Return (0x30)   }

                If (LNotEqual (DerefOf (Index (BUFR,1)), 0))
                    {   Return (0x31)   }

                If (LNotEqual (DerefOf (Index (BUFR,4)), 0xEF))
                    {   Return (0x34)   }

                If (LNotEqual (DerefOf (Index (BUFR,8)), 0x98))
                    {   Return (0x38)   }

                If (LNotEqual (DerefOf (Index (BUFR,12)), 0x10))
                    {   Return (0x3C)   }


                Return (0)  //  pass
            }   //  TEST
        }   //  IDX0
    }   //  _SB system bus

//
// test BitIndex.asl
//
//  BitIndex test
//  This is a test case for accessing fields defined as single bits in
//  memory. This is done by creating two index fields that overlay the
//  same DWORD in memory. One field accesses the DWORD as a DWORD, the
//  other accesses individual bits of the same DWORD field in memory.
//
    Scope (\_SB)    //  System Bus
    {   //  _SB system bus
        OperationRegion (RAM, SystemMemory, 0x800000, 0x100)
        Field (RAM, AnyAcc, NoLock, Preserve)
        {   //  Any access
            TREE,   3,
            WRD0,   16,
            WRD1,   16,
            WRD2,   16,
            WRD3,   16,
            WRD4,   16,
            DWRD,   32, //  DWORD field
        }
        Field (RAM, AnyAcc, NoLock, Preserve)
        {   //  Any access
            THRE,   3,
            WD00,   16,
            WD01,   16,
            WD02,   16,
            WD03,   16,
            WD04,   16,
            BYT0,   8,  //  Start off with a BYTE
            BIT0,   1,  //  single-bit field
            BIT1,   1,  //  single-bit field
            BIT2,   1,  //  single-bit field
            BIT3,   1,  //  single-bit field
            BIT4,   1,  //  single-bit field
            BIT5,   1,  //  single-bit field
            BIT6,   1,  //  single-bit field
            BIT7,   1,  //  single-bit field
            BIT8,   1,  //  single-bit field
            BIT9,   1,  //  single-bit field
            BITA,   1,  //  single-bit field
            BITB,   1,  //  single-bit field
            BITC,   1,  //  single-bit field
            BITD,   1,  //  single-bit field
            BITE,   1,  //  single-bit field
            BITF,   1,  //  single-bit field
            BYTZ,   8,  //  End with a BYTE for a total of 32 bits
        }

        Device (BITI)
        {   //  Test device name

            Method (MBIT)   //  Test single bit memory accesses
            {

                If (LNotEqual (DWRD, 0x00))
                {
                    Store (0xFF00, Local0)
                }
                Else
                {
                    //  Prime Local0 with 0...assume passing condition
                    Store (0, Local0)

                    //  set memory contents to known values using DWORD field
                    Store (0x5A5A5A5A, DWRD)

                    //  Given the value programmed into DWRD, only the odd bits
                    //  of the lower nibble should be set. BIT1, BIT3 should be set.
                    //  BIT0 and BIT2 should be clear

                    If (BIT0)
                    {
                        Or (Local0, 0x01, Local0)
                    }

                    If (LNot (BIT1))
                    {
                        Or (Local0, 0x02, Local0)
                    }

                    If (BIT2)
                    {
                        Or (Local0, 0x04, Local0)
                    }

                    If (LNot (BIT3))
                    {
                        Or (Local0, 0x08, Local0)
                    }

                    //  Now check the upper nibble. Only the "even" bits should
                    //  be set. BIT4, BIT6. BIT5 and BIT7 should be clear.
                    If (LNot (BIT4))
                    {
                        Or (Local0, 0x10, Local0)
                    }

                    If (BIT5)
                    {
                        Or (Local0, 0x20, Local0)
                    }

                    If (LNot (BIT6))
                    {
                        Or (Local0, 0x40, Local0)
                    }

                    If (BIT7)
                    {
                        Or (Local0, 0x80, Local0)
                    }
                }   //  End Else DWRD zeroed out

                Return (Local0)
            }   //  MBIT:   Test single bit memory accesses

            Method (TEST)
            {

                Store ("++++++++ BitIndex Test", Debug)

                //  Zero out DWRD
                Store (0x00000000, DWRD)

                //  MBIT returns zero if successful
                //  This may be causing problems -- Return (MBIT)
                Store (MBIT, Local0)

                Return (Local0)
            }   //  TEST
        }   //  BITI
    }   //  _SB system bus

//
// test IndexOp3.asl
//
//  Additional IndexOp test cases to support ACPICMB (control method battery
//  test) on Compaq laptops. Test cases include storing a package into
//  an IndexOp target and validating that changing source and destination
//  package contents are independent of each other.
//
    Scope (\_SB)    //  System Bus
    {   //  _SB system bus

        Name (C174, 13)
        Name (C175, 8)

        Device (C158)
        {   //  C158:   AC Adapter device
            Name (_HID, "ACPI0003") //  AC Adapter device
            Name (_PCL, Package (1) {\_SB})

            Method (_PSR)
            {
                Acquire (\_GL, 0xFFFF)
                Release (\_GL)
                And (Local0, 1, Local0) //  Local0 &= 1
                Return (Local0)
            }   //  _PSR
        }   //  C158:   AC Adapter device

        Name (C176, Package (4) {"Primary", "MultiBay", "DockRight", "DockLeft"})

        Name (C177, Package (4) {0x99F5, 0x99F5, 0x995F, 0x995F})

        Name (C178, Package (4)
        {
            Package (4) {0, 0, 0x966B, 0x4190},
            Package (4) {0, 0, 0x966B, 0x4190},
            Package (4) {0, 0, 0x966B, 0x4190},
            Package (4) {0, 0, 0x966B, 0x4190}
        })  //  C178

        Name (C179, Package (4) {0, 0, 0x966B, 0x4190})

        Name (C17A, Package (4)
        {
            Package (3) {0, 0, 0},
            Package (3) {0, 0, 0},
            Package (3) {0, 0, 0},
            Package (3) {0, 0, 0}
        })  //  C17A

        Method (C17B, 1, Serialized)
        {   //  C17B:   _BIF implementation
            Name (C17C, Package (13)
            {   //  C17C:   _BIF control method return package
                0,                  //  Power Unit (0 ==> mWh and mW)
                0x99F5,         //  Design Capacity
                0x99F5,         //  Last Full Charge Capacity
                1,                  //  Battery Technology (1 ==> rechargeable)
                0x3840,         //  Design Voltage
                0x1280,         //  Design Capacity of Warning
                0x0AC7,         //  Design Capacity of Low
                1,                  //  Battery Capacity Granularity 1 (Low -- Warning)
                1,                  //  Battery Capacity Granularity 2 (Warning -- Full)
                "2891",         //  Model Number (ASCIIZ)
                "(-Unknown-)",  //  Serial Number (ASCIIZ)
                "LIon",         //  Battery Type (ASCIIZ)
                0                   //  OEM Information (ASCIIZ)
            })  //  C17C:   _BIF control method return package

            And (Arg0, 7, Local0)                       //  Local0 = Arg0 & 7

            ShiftRight (Local0, 1, Local4)          //  Local4 = Local0 >> 1

            Store (C179, Index (C178, Local4, ))    //  C178->Local4 = C179

            //  verify source and destination packages can be altered independent
            //  of each other (i.e., changing one's contents does NOT change other's
            //  contents)
            Store (0x1234, Index (C179, 2, ))               //  C179[2] = 0x1234
            Store (DerefOf (Index (C179, 2, )), Local2) //  Local2 = C179[2]
            if (LNotEqual (Local2, 0x1234))
                {   Return (0x1234) }
                                                                        //  Local2 = C178[0,2]
            Store (DerefOf (Index (DerefOf (Index (C178, 0, )), 2, )), Local2)
            if (LNotEqual (Local2, 0x966B))
                {   Return (0x1234) }

            // Restore data to allow iterative execution
            Store (0x966B, Index (C179, 2, ))               //  C179[2] = 0x966B

                                                                        //  C178[0,3] = 0x5678
            Store (0x5678, Index (DerefOf (Index (C178, 0, )), 3, ))
                                                                        //  Local2 = C178[0,3]
            Store (DerefOf (Index (DerefOf (Index (C178, 0, )), 3, )), Local2)
            if (LNotEqual (Local2, 0x5678))
                {   Return (0x5678) }

            Store (DerefOf (Index (C179, 3, )), Local2) //  Local2 = C179[3]
            if (LNotEqual (Local2, 0x4190))
                {   Return (0x5678) }

            // Restore data to allow iterative execution
            Store (0x4190, Index (DerefOf (Index (C178, 0, )), 3, ))    //  C179[2] = 0x4190

            Return (C17C)
        }   //  C17B:   _BIF implementation

        Device (C154)
        {   //  C154:   Battery 0
            Name (_HID, "*PNP0C0A")     //  Control Method Battey ID
            Name (_UID, 0)                  //  first instance

            Method (_BIF)
            {   //  _BIF
                Return (C17B (48))
            }   //  _BIF
        }   //  C154:   Battery 0

        Device (IDX3)
        {
            Method (LCLB,, Serialized)
            {   //  LCLB control method: test Index(Local#) where Local# is buffer
                //  Local0 is index counter
                //  Local1 is buffer
                //  Local2 receives BUFR[Local0] via Deref(Index(Local1...))
                //  Local3 is Local1 or Local2 object type
                //  Local4 is return error code

                Name (BUFR, Buffer ()   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9})

                //  save PKG into Local1
                Store (BUFR, Local1)

                //  save Local2 object type value into Local3
                Store (ObjectType (Local1), Local3)

                //  validate Local1 is a Buffer
                If (LNotEqual (Local3, 3))      //  Buffer type is 3
                    {   Return (0x9F)   }


                Store (0, Local0)
                While (LLess (Local0, 5))
                {   //  While (Local0 < 5)
                    //  Local2 = Local1[Local0]
                    Store (DerefOf (Index (Local1, Local0, )), Local2)

                    //  save Local2 object type value into Local3
                    Store (ObjectType (Local2), Local3)

                    //  validate Local2 is a Number
                    If (LNotEqual (Local3, 1))      //  Number type is 1
                        {   Return (0x9E)   }

                    //  validate Local1[Local0] value == Local0
                    If (LNotEqual (Local0, Local2))
                    {   //  Local0 != Local2 == PKG[Local0]
                        //  Local4 = 0x90 + loop index (Local0)
                        Add (0x90, Local0, Local4)

                        //  return 0x90 + loop index
                        Return (Local4)
                    }

                    Increment (Local0)
                }   //  While (Local0 < 5)

                Store ("DerefOf(Index(LocalBuffer,,)) PASS", Debug)

                Return (0)  //  Pass
            }   //  LCLB control method: test Index(Local#) where Local# is buffer

            Method (LCLP,, Serialized)
            {   //  LCLP control method: test Index(Local#) where Local# is package
                //  Local0 is index counter
                //  Local1 is package
                //  Local2 receives PKG[Local0] via Deref(Index(Local1...))
                //  Local3 is Local1 or Local2 object type
                //  Local4 is return error code

                Name (PKG, Package ()   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9})

                //  save PKG into Local1
                Store (PKG, Local1)

                //  save Local2 object type value into Local3
                Store (ObjectType (Local1), Local3)

                //  validate Local1 is a Package
                If (LNotEqual (Local3, 4))      //  Package type is 4
                    {   Return (0x8F)   }


                Store (0, Local0)
                While (LLess (Local0, 5))
                {   //  While (Local0 < 5)
                    //  Local2 = Local1[Local0]
                    Store (DerefOf (Index (Local1, Local0, )), Local2)

                    //  save Local2 object type value into Local3
                    Store (ObjectType (Local2), Local3)

                    //  validate Local2 is a Number
                    If (LNotEqual (Local3, 1))      //  Number type is 1
                        {   Return (0x8E)   }

                    //  validate Local1[Local0] value == Local0
                    If (LNotEqual (Local0, Local2))
                    {   //  Local0 != Local2 == PKG[Local0]
                        //  Local4 = 0x80 + loop index (Local0)
                        Add (0x80, Local0, Local4)

                        //  return 0x80 + loop index
                        Return (Local4)
                    }

                    Increment (Local0)
                }   //  While (Local0 < 5)

                Store ("DerefOf(Index(LocalPackage,,)) PASS", Debug)

                Return (0)  //  Pass
            }   //  LCLP control method: test Index(Local#) where Local# is package

            Method (TEST)
            {

                Store ("++++++++ IndexOp3 Test", Debug)

                //  store _BIF package return value into Local0
                Store (\_SB.C154._BIF, Local0)

                //  save Local0 object type value into Local1
                Store (ObjectType (Local0), Local1)

                //  validate Local0 is a Package
                If (LNotEqual (Local1, 4))      //  Package type is 4
                {   //  failure: did not return a Package (type 4)
                    //  if Local0 is a Number, it contains an error code
                    If (LEqual (Local1, 1))     //  Number type is 1
                        {   Return (Local0) }   //  return Local0 error code
                    Else                                //  Local0 is not a Number
                        {   Return (1)  }           //  return default error code
                }   //  failure: did not return a Package (type 4)

                //  save LCLB control method return value into Local2
                Store (LCLB, Local2)
                If (LNotEqual (Local2, 0))
                    {   Return (Local2) }   //  return failure code

                //  save LCLP control method return value into Local2
                Store (LCLP, Local2)
                If (LNotEqual (Local2, 0))
                    {   Return (Local2) }   //  return failure code

                Return (0)  //  Pass
            }   //  TEST
        }   //  IDX3:   Test device name
    }   //  _SB system bus

//
// MTL developed test to exercise Indexes into buffers
//
    Device(IDX7)
    {

        Name (PKG4, Package() {
                0x2,
                "A short string",
                Buffer() {0xA, 0xB, 0xC, 0xD},
                0x1234,
                Package() {IDX7, 0x3}
                })

        //
        // Generic Test method
        //
        // This test returns 0xE (14) - ObjectType = Buffer Field
        Method(TST1,, Serialized)
        {
            Name (DEST, Buffer ()                           //  62 characters plus NULL
                {"Destination buffer that is longer than the short source buffer"})

            //  verify object type returned by Index(Buffer,Element,)
            Store (Index (DEST, 2, ), Local1)
            Store (ObjectType (Local1), Local2)
            If (LEqual(Local2, 14))
            {
                Return(0)
            }
            Else
            {
                Return(0x1)
            }

        }

        Method(TST2,, Serialized)
        {
            Name (BUF0, Buffer() {0x1, 0x2, 0x3, 0x4, 0x5})
            Store(0x55, Index(BUF0, 2))
            Store(DerefOf(Index(BUF0, 2)), Local0)
            If (LEqual(Local0, 0x55))
            {
                Return(0)
            }
            Else
            {
                Return(0x2)
            }


        }

        Method(TST3,, Serialized)
        {
            Name (BUF1, Buffer() {0x1, 0x2, 0x3, 0x4, 0x5})
            Store(Index(BUF1, 1), Local0)
            Store(DerefOf(Local0), Local1)
            If (LEqual(Local1, 0x2))
            {
                Return(0)
            }
            Else
            {
                Return(0x3)
            }

        }

        Method(TST4)
        {
            // Index (PKG4, 0) is a Number
            Store (Index (PKG4, 0), Local0)
            Store (ObjectType(Local0), Local1)
            If (LEqual(Local1, 0x1))
            {
                Return(0)
            }
            Else
            {
                Return(0x4)
            }

        }

        Method(TST5)
        {
            // Index (PKG4, 1) is a String
            Store (Index (PKG4, 1), Local0)
            Store (ObjectType(Local0), Local1)
            If (LEqual(Local1, 0x2))
            {
                Return(0)
            }
            Else
            {
                Return(0x5)
            }

        }

        Method(TST6)
        {
            // Index (PKG4, 2) is a Buffer
            Store (Index (PKG4, 2), Local0)
            Store (ObjectType(Local0), Local1)
            If (LEqual(Local1, 0x3))
            {
                Return(0)
            }
            Else
            {
                Return(0x6)
            }

        }

        Method(TST7)
        {
            // Index (PKG4, 3) is a Number
            Store (Index (PKG4, 3), Local0)
            Store (ObjectType(Local0), Local1)
            If (LEqual(Local1, 0x1))
            {
                Return(0)
            }
            Else
            {
                Return(0x7)
            }

        }

        Method(TST8)
        {
            // Index (PKG4, 4) is a Package
            Store (Index (PKG4, 4), Local0)
            Store (ObjectType(Local0), Local1)
            If (LEqual(Local1, 0x4))
            {
                Return(0)
            }
            Else
            {
                Return(0x8)
            }

        }

        Method(TST9)
        {
            // DerefOf (Index (PKG4, 0)) is a Number
            Store (DerefOf (Index (PKG4, 0)), Local0)
            If (LEqual(Local0, 0x2))
            {
                Return(0)
            }
            Else
            {
                Return(0x9)
            }

        }

        Method(TSTA)
        {
            // DerefOf (Index (PKG4, 1)) is a String
            Store (DerefOf (Index (PKG4, 1)), Local0)
            Store (SizeOf(Local0), Local1)
            If (LEqual(Local1, 0xE))
            {
                Return(0)
            }
            Else
            {
                Return(0xA)
            }

        }

        Method(TSTB)
        {
            // DerefOf (Index (PKG4, 2)) is a Buffer
            Store (DerefOf (Index (PKG4, 2)), Local0)
            Store (SizeOf(Local0), Local1)
            If (LEqual(Local1, 0x4))
            {
                Return(0)
            }
            Else
            {
                Return(0xB)
            }

        }

        Method(TSTC)
        {
            // DerefOf (Index (PKG4, 3)) is a Number
            Store (DerefOf (Index (PKG4, 3)), Local0)
            If (LEqual(Local0, 0x1234))
            {
                Return(0)
            }
            Else
            {
                Return(0xC)
            }

        }

        Method(TSTD)
        {
            // DerefOf (Index (PKG4, 4)) is a Package
            Store (DerefOf (Index (PKG4, 4)), Local0)
            Store (SizeOf(Local0), Local1)
            If (LEqual(Local1, 0x2))
            {
                Return(0)
            }
            Else
            {
                Return(0xD)
            }

        }

        Method(TSTE)
        {
            // DerefOf (Index (PKG4, 2)) is a Buffer
            Store (DerefOf (Index (PKG4, 2)), Local0)
            // DerefOf (Index (Local0, 1)) is a Number
            Store (DerefOf (Index (Local0, 1)), Local1)
            If (LEqual(Local1, 0xB))
            {
                Return(0)
            }
            Else
            {
                Return(0xE)
            }

        }

        Method (TSTF,, Serialized)
        {
            Name (SRCB, Buffer (12) {}) //  12 characters
            Store ("Short Buffer", SRCB)

            Name (DEST, Buffer ()                       //  62 characters plus NULL
                {"Destination buffer that is longer than the short source buffer"})

            //  overwrite DEST contents, starting at buffer position 2
            Store (SRCB, Index (DEST, 2))

            //
            //  The DEST buffer element should be replaced with the last element of
            //      the SRCB element (i.e. 's'->'r')
            Store (DerefOf (Index (DEST, 2)), Local0)

            If (LNotEqual (Local0, 0x72))       //  'r'
            {
                //  DEST element does not match the value from SRCB
                Return(Or(Local0, 0x1000))
            }

            Return(0)
        }

        Method (TSTG,, Serialized)
        {

            Name (SRCB, Buffer (12) {}) //  12 characters
            Store ("Short Buffer", SRCB)

            Name (DEST, Buffer ()                       //  62 characters plus NULL
                {"Destination buffer that is longer than the short source buffer"})

            //  overwrite DEST contents, starting at buffer position 2
            Store (SRCB, Index (DEST, 2))

            //
            // The next element of DEST should be unchanged
            //
            Store (DerefOf (Index (DEST, 3)), Local0)

            If (LNotEqual (Local0, 0x74))       //  't'
            {
                //  DEST has been changed
                Return(Or(Local0, 0x2000))
            }

            //
            // The next element of DEST should be unchanged
            //
            Store (DerefOf (Index (DEST, 4)), Local0)

            If (LNotEqual (Local0, 0x69))       //  'i'
            {
                //  DEST has been changed
                Return(Or(Local0, 0x2100))
            }

            //
            // The next element of DEST should be unchanged
            //
            Store (DerefOf (Index (DEST, 5)), Local0)

            If (LNotEqual (Local0, 0x6E))       //  'n'
            {
                //  DEST has been changed
                Return(Or(Local0, 0x2200))
            }

            //
            // The next element of DEST should be unchanged
            //
            Store (DerefOf (Index (DEST, 6)), Local0)

            If (LNotEqual (Local0, 0x61))       //  'a'
            {
                //  DEST has been changed
                Return(Or(Local0, 0x2300))
            }

            //
            // The next element of DEST should be unchanged
            //
            Store (DerefOf (Index (DEST, 7)), Local0)

            If (LNotEqual (Local0, 0x74))       //  't'
            {
                //  DEST has been changed
                Return(Or(Local0, 0x2400))
            }

            //
            //  Verify DEST elements beyond end of SRCB buffer copy
            //  have not been changed
            Store (DerefOf (Index (DEST, 14)), Local0)

            If (LNotEqual (Local0, 0x66))       // 'f'
            {
                //  DEST has been changed
                Return(Or(Local0, 0x2400))
            }

            Return(0)
        }

        //
        // This test shows that MS ACPI.SYS stores only the lower 8-bits of a 32-bit
        //  number into the index'ed buffer
        //
        Method (TSTH,, Serialized)
        {
            // Create a Destination Buffer
            Name (DBUF, Buffer () {"abcdefghijklmnopqrstuvwxyz"})

            // Store a number > UINT8 into an index of the buffer
            Store (0x12345678, Index(DBUF, 2))

            // Check the results
            Store (DerefOf (Index (DBUF, 2)), Local0)
            If (LNotEqual (Local0, 0x78))   // 0x78
            {
                Return(Or(Local0, 0x3000))
            }

            Store (DerefOf (Index (DBUF, 3)), Local0)
            If (LNotEqual (Local0, 0x64))   // 'd'
            {
                Return(Or(Local0, 0x3100))
            }

            Store (DerefOf (Index (DBUF, 4)), Local0)
            If (LNotEqual (Local0, 0x65))   // 'e'
            {
                Return(Or(Local0, 0x3200))
            }

            Store (DerefOf (Index (DBUF, 5)), Local0)
            If (LNotEqual (Local0, 0x66))   // 'f'
            {
                Return(Or(Local0, 0x3300))
            }

            Return(0)
        }

        Method (TSTI,, Serialized)
        {
            // Create a Destination Buffer
            Name (DBUF, Buffer () {"abcdefghijklmnopqrstuvwxyz"})

            // Store a String into an index of the buffer
            Store ("ABCDEFGH", Index(DBUF, 2))

            // Check the results
            Store (DerefOf (Index (DBUF, 2)), Local0)
            If (LNotEqual (Local0, 0x48))   // 'H'
            {
                Return(Or(Local0, 0x4000))
            }

            Store (DerefOf (Index (DBUF, 3)), Local0)
            If (LNotEqual (Local0, 0x64))   // 'd'
            {
                Return(Or(Local0, 0x4100))
            }

            Store (DerefOf (Index (DBUF, 4)), Local0)
            If (LNotEqual (Local0, 0x65))   // 'e'
            {
                Return(Or(Local0, 0x4200))
            }

            Store (DerefOf (Index (DBUF, 5)), Local0)
            If (LNotEqual (Local0, 0x66))   // 'f'
            {
                Return(Or(Local0, 0x4300))
            }

            Return(0)
        }

        Method(TSTJ,, Serialized)
        {
            // Create a Destination Buffer
            Name (DBUF, Buffer () {"abcdefghijklmnopqrstuvwxyz"})

            // Store a number > UINT8 into an index of the buffer
            Store (0x1234, Index(DBUF, 2))

            // Check the results
            Store (DerefOf (Index (DBUF, 2)), Local0)
            If (LNotEqual (Local0, 0x34))   // 0x34
            {
                Return(Or(Local0, 0x3000))
            }

            Store (DerefOf (Index (DBUF, 3)), Local0)
            If (LNotEqual (Local0, 0x64))   // 'd'
            {
                Return(Or(Local0, 0x3100))
            }

            Store (DerefOf (Index (DBUF, 4)), Local0)
            If (LNotEqual (Local0, 0x65))   // 'e'
            {
                Return(Or(Local0, 0x3200))
            }

            Store (DerefOf (Index (DBUF, 5)), Local0)
            If (LNotEqual (Local0, 0x66))   // 'f'
            {
                Return(Or(Local0, 0x3300))
            }

            Return(0)
        }

        Method(TSTK,, Serialized)
        {
            // Create a Destination Buffer
            Name (DBUF, Buffer () {"abcdefghijklmnopqrstuvwxyz"})

            // Store a number > UINT8 into an index of the buffer
            Store (0x123456, Index(DBUF, 2))

            // Check the results
            Store (DerefOf (Index (DBUF, 2)), Local0)
            If (LNotEqual (Local0, 0x56))   // 0x56
            {
                Return(Or(Local0, 0x3000))
            }

            Store (DerefOf (Index (DBUF, 3)), Local0)
            If (LNotEqual (Local0, 0x64))   // 'd'
            {
                Return(Or(Local0, 0x3100))
            }

            Store (DerefOf (Index (DBUF, 4)), Local0)
            If (LNotEqual (Local0, 0x65))   // 'e'
            {
                Return(Or(Local0, 0x3200))
            }

            Store (DerefOf (Index (DBUF, 5)), Local0)
            If (LNotEqual (Local0, 0x66))   // 'f'
            {
                Return(Or(Local0, 0x3300))
            }

            Return(0)
        }

        Method(TSTL,, Serialized)
        {
            // Create a Destination Buffer
            Name (DBUF, Buffer () {"abcdefghijklmnopqrstuvwxyz"})

            // Store a number > UINT8 into an index of the buffer
            Store (0x12, Index(DBUF, 2))

            // Check the results
            Store (DerefOf (Index (DBUF, 2)), Local0)
            If (LNotEqual (Local0, 0x12))   // 0x12
            {
                Return(Or(Local0, 0x3000))
            }

            Store (DerefOf (Index (DBUF, 3)), Local0)
            If (LNotEqual (Local0, 0x64))   // 'd'
            {
                Return(Or(Local0, 0x3100))
            }

            Store (DerefOf (Index (DBUF, 4)), Local0)
            If (LNotEqual (Local0, 0x65))   // 'e'
            {
                Return(Or(Local0, 0x3200))
            }

            Store (DerefOf (Index (DBUF, 5)), Local0)
            If (LNotEqual (Local0, 0x66))   // 'f'
            {
                Return(Or(Local0, 0x3300))
            }

            Return(0)
        }

        Method(TEST)
        {
            Store ("++++++++ IndexOp7 Test", Debug)

            Store(TST1(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TST2(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TST3(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TST4(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TST5(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TST6(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TST7(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TST8(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TST9(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TSTA(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TSTB(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TSTC(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TSTD(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TSTE(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

    /* No longer ACPI compliant */
    /*
            Store(TSTF(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }
    */

            Store(TSTG(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TSTH(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

    /* No longer ACPI compliant */
    /*
            Store(TSTI(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }
    */
            Store(TSTJ(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TSTK(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Store(TSTL(), Local0)
            if (LGreater (Local0, 0))
            {
                Return(Local0)
            }

            Return(Local0)

        }

    } // Device(IDX7)

//
// test MatchOp.asl
//
//  MatchOp test cases that utilize nested DerefOf(Index(...)) to validate
//  MatchOp, DerefOfOp, and IndexOp of nested packages.
//
    Device (MTCH)
    {

        Method (TEST,, Serialized)
        {
            Store ("++++++++ MatchOp Test", Debug)

            Name (TIM0, Package ()
                {
                    Package ()  {0x78, 0xB4, 0xF0, 0x0384},
                    Package ()  {0x23, 0x21, 0x10, 0},
                    Package ()  {0x0B, 9, 4, 0},
                    Package ()  {0x70, 0x49, 0x36, 0x27, 0x19},
                    Package ()  {0, 1, 2, 1, 2},
                    Package ()  {0, 0, 0, 1, 1},
                    Package ()  {4, 3, 2, 0},
                    Package ()  {2, 1, 0, 0}
                })  //  TIM0

            Name (TMD0, Buffer (20) {0xFF, 0xFF, 0xFF, 0xFF })
            CreateDWordField (TMD0, 0, PIO0)    //  0xFFFFFFFF
            CreateDWordField (TMD0, 4, DMA0)
            CreateDWordField (TMD0, 8, PIO1)
            CreateDWordField (TMD0, 12, DMA1)
            CreateDWordField (TMD0, 16, CHNF)


            //  validate PIO0 value
            Store (PIO0, Local3)

            //  save Local3 object type value into Local2
            Store (ObjectType (Local3), Local2)

            //  validate Local3 is a Number
            If (LNotEqual (Local2, 1))  //  Number type is 1
                {   Return (2)  }   //  failure

            //  validate Local3 Number value
            If (LNotEqual (Local3, 0xFFFFFFFF)) //  Number value 0xFFFFFFFF
                {   Return (3)  }   //  failure

            Store ("DWordField PASS", Debug)


            Store (0, Local5)
            Store (Match (DerefOf (Index (TIM0, 1, )), MLE, Local5, MTR, 0, 0), Local6)

            //  save Local6 object type value into Local2
            Store (ObjectType (Local6), Local2)

            //  validate Local6 is a Number
            If (LNotEqual (Local2, 1))  //  Number type is 1
                {   Return (4)  }   //  failure

            Store ("Match(DerefOf(Index(TIM0,1)),... PASS", Debug)


            //  validate following produces a nested package to validate
            //  that MatchOp did not corrupt SearchPackage (TIM0)
            Store (DerefOf (Index (TIM0, 1, )), Local4)

            //  save Local4 object type value into Local2
            Store (ObjectType (Local4), Local2)

            //  validate Local4 is a Package
            If (LNotEqual (Local2, 4))  //  Package type is 4
                {   Return (5)  }   //  failure

            Store ("DerefOf(Index(TIM0,1)),... PASS", Debug)


            And (Match (DerefOf (Index (TIM0, 0, )), MGE, PIO0, MTR, 0, 0), 3, Local0)

            //  save Local0 object type value into Local2
            Store (ObjectType (Local0), Local2)

            //  validate Local0 is a Number
            If (LNotEqual (Local2, 1))  //  Number type is 1
                {   Return (6)  }   //  failure

            //  validate Local0 Number value
            If (LNotEqual (Local0, 3))  //  Number value 3
                {   Return (7)  }   //  failure

            Store ("And(Match(DerefOf(Index(TIM0,0)),... PASS", Debug)


            //  again, validate following produces a nested package
            Store (DerefOf (Index (TIM0, 1, )), Local4)

            //  save Local4 object type value into Local2
            Store (ObjectType (Local4), Local2)

            //  validate Local4 is a Package
            If (LNotEqual (Local2, 4))  //  Package type is 4
                {   Return (8)  }   //  failure

            Store ("DerefOf(Index(TIM0,1)),... PASS again", Debug)


            //  again, validate following produces a nested package
            Store (DerefOf (Index (TIM0, 1, )), Local4)

            //  save Local4 object type value into Local2
            Store (ObjectType (Local4), Local2)

            //  validate Local4 is a Package
            If (LNotEqual (Local2, 4))  //  Package type is 4
                {   Return (9)  }   //  failure

            Store ("DerefOf(Index(TIM0,1)),... PASS again", Debug)


            //  test nested DerefOf(Index) operators
            Store (DerefOf (Index (DerefOf (Index (TIM0, 1, )), Local0, )), Local1)

            //  save Local1 object type value into Local2
            Store (ObjectType (Local1), Local2)

            //  validate Local1 is a Number
            If (LNotEqual (Local2, 1))  //  Number type is 1
                {   Return (10) }   //  failure

            //  zero indicates pass, non-zero is an error code
            If (LNotEqual (Local1, 0))
                {   Return (11) }   //  failure

            Store ("DerefOf(Index(DerefOf(Index(TIM0,1)),... PASS", Debug)


            //  again, validate following produces a nested package
            Store (DerefOf (Index (TIM0, 1, )), Local4)

            //  save Local4 object type value into Local2
            Store (ObjectType (Local4), Local2)

            //  validate Local4 is a Package
            If (LNotEqual (Local2, 4))  //  Package type is 4
                {   Return (12) }   //  failure

            Store ("DerefOf(Index(TIM0,1)),... PASS again", Debug)


            //  retest nested DerefOf(Index) operators
            Store (DerefOf (Index (DerefOf (Index (TIM0, 1, )), Local0, )), Local1)

            //  save Local1 object type value into Local2
            Store (ObjectType (Local1), Local2)

            //  validate Local1 is a Number
            If (LNotEqual (Local2, 1))  //  Number type is 1
                {   Return (13) }   //  failure

            //  zero indicates pass, non-zero is an error code
            If (LNotEqual (Local1, 0))
                {   Return (14) }   //  failure

            Store ("DerefOf(Index(DerefOf(Index(TIM0,1)),... PASS again", Debug)


            //  again, validate following produces a nested package
            Store (DerefOf (Index (TIM0, 1, )), Local4)

            //  save Local4 object type value into Local2
            Store (ObjectType (Local4), Local2)

            //  validate Local4 is a Package
            If (LNotEqual (Local2, 4))  //  Package type is 4
                {   Return (15) }   //  failure

            Store ("DerefOf(Index(TIM0,1)),... PASS again", Debug)


            Return (0)  //  pass
        }   //  TEST
    }   // MTCH

//
// test WhileBrk.asl
//
//  This code tests the Break term and While term
//
//  Syntax of Break term
//      BreakTerm := Break
//  The break operation causes the current package execution to complete.
//
//  Syntax of While Term
//      WhileTerm   := While(
//          Predicate   //TermArg=>Integer
//      ) {TermList}
//  Predicate is evaluated as an integer.
//  If the integer is non-zero, the list of terms in TermList is executed.
//  The operation repeats until the Predicate evaluates to zero.
//
// MTL NOTE: This test has been modified to reflect ACPI 2.0 break
// NOTE: This test, when run under the MS ACPI.SYS grinds the system to
//  a halt.
//
    Device (WHLB)
    {
        Name (CNT0, 0)
        Name (CNT1, 0)

        Method (TEST)
        {
            //  Check Break statement nested in If nested in While nested in
            //  While only exits inner-most While loop
            Store (0, CNT0)

            While (LLess (CNT0, 4))
            {
                Store (0, CNT1)
                While (LLess (CNT1, 10))
                {
                    if (LEqual (CNT1, 1))
                    {
                        Break       //  exit encompassing loop
                    }

                    Increment (CNT1)
                }

                If (LNotEqual (CNT1, 1))
                {
                    //  failure
                    Return (7)
                }

                Increment (CNT0)
            }

            //  Verify Break only exited inner-most While loop

            If (LNotEqual (CNT0, 4))
            {
                //  failure
                Return (8)
            }

            Store ("While/While/If/Break PASS", Debug)

            Store ("++++++++ WhileBrk Test", Debug)

            //  Check Break statement nested in While
            Store (0, CNT0)

            While (LLess (CNT0, 10))
            {
                Break       //  exit encompassing package
                Increment (CNT0)
            }

            If (LNotEqual (CNT0, 0))    //  instruction after Break executed
            {
                Return (4)
            }


            Store (0, CNT0)

            //  Test While Term
            While (LLess (CNT0, 10))
            {
                Increment (CNT0)
            }

            //  Check if the while loop was executed until the condition is satisfied.
            If (LNotEqual (CNT0, 10))
            {
                Return (1)
            }


            //  While loop in a reverse order
            While (LGreater (CNT0, 0))
            {
                Decrement (CNT0)
            }

            //  Check if the while loop was executed until the condition is satisfied.
            If (LNotEqual (CNT0, 0))
            {
                Return (2)
            }


            Store ("While/Break PASS", Debug)


            //  Check Break statement nested in If nested in While
            Store (0, CNT0)

            While (LLess (CNT0, 10))
            {
                if (LEqual (CNT0, 5))
                {
                    Break       //  exit encompassing Package (If)

                    //  if we execute the next instruction,
                    //  Break did not exit the loop
                    Store (20, CNT0)    //  exit While loop with value larger
                                            //  than above
                }

                Increment (CNT0)    //  check if Break exited both If and While
            }   //  While

            If (LGreater (CNT0, 19))
            {   //  instruction after Break inside IfOp executed
                Return (5)
            }

            //
            // Break will exit out of the while loop, therefore
            //  the CNT0 counter should still Increment until 5
            //
            If (LNotEqual (CNT0, 5))
            {   //  instruction after Break inside WhileOp executed
                Return (6)
            }
            Store ("While/If/Break PASS", Debug)


            //  All the conditions passed
            Return (0)
        }   //  TEST
    }   //  WHLB


//
// test IndexOp2.asl
//
//  Additional IndexOp test cases to support ACPICMB (control method battery
//  test) on Toshiba Portege 7020CT. Test cases include appropriate bit
//  shifting of Field elements and reading Field elements greater than 64 bits.
//
// MTL NOTE: This test has been modified slightly from the original test
//  to take into account ACPI specification limitations.
//
    Scope (\_SB)    //  System Bus
    {   //  _SB system bus

        Device (MEM)
        {   //  MEM
            Name (_HID, 0x010CD041)
            Name (_STA, 0x0F)

            OperationRegion (SMEM, SystemMemory, 0x800000, 0x100)
            Field (SMEM, AnyAcc, NoLock, Preserve)
            {   //  Field:  SMEM overlay using 32-bit field elements
                SMD0,   32, //  32-bits
                SMD1,   32,     //  32-bits
                SMD2,   32,     //  32-bits
                SMD3,   32  //  32-bits
            }   //  Field:  SMEM overlay using 32-bit field elements
            Field (SMEM, AnyAcc, NoLock, Preserve)
            {   //  Field:  SMEM overlay using greater than 32-bit field elements
                SME0,   69, //  larger than an integer (32 or 64)
                SME1,   97  //  larger than an integer
            }   //  Field:  SMEM overlay using greater than 32-bit field elements

            OperationRegion (SRAM, SystemMemory, 0x100B0000, 0xF000)
            Field (SRAM, AnyAcc, NoLock, Preserve)
            {   //  Field:  SRAM overlay
                    ,   0x34000,    //  skip
                IEAX,   0x20,
                IEBX,   0x20,
                IECX,   0x20,
                IEDX,   0x20,
                IESI,   0x20,
                IEDI,   0x20,
                IEBP,   0x20,
                    ,   0x20,
                OEAX,   0x20,
                OEBX,   0x20,
                OECX,   0x20,
                OEDX,   0x20,
                OESI,   0x20,
                OEDI,   0x20,
                OEBP,   0x20,
                    ,   0x618,  //  skip
                ACST,   1,
                BES1,   1,
                BES2,   1,
                    ,   5,          //  skip
                BMN1,   0x68,
                BSN1,   0x58,
                BTP1,   0x48,
                BPU1,   0x20,
                BDC1,   0x20,
                BLF1,   0x20,
                BTC1,   0x20,
                BDV1,   0x20,
                BST1,   0x20,
                BPR1,   0x20,
                BRC1,   0x20,
                BPV1,   0x20,
                    ,   0x20,
                BCW1,   0x20,
                BCL1,   0x20,
                BG11,   0x20,
                BG21,   0x20,
                BOI1,   0x20,
                    ,   0x530,  //  skip
                BMN2,   0x68,
                BSN2,   0x58,
                BTP2,   0x48,
                BPU2,   0x20,
                BDC2,   0x20,
                BLF2,   0x20,
                BTC2,   0x20,
                BDV2,   0x20,
                BST2,   0x20,
                BPR2,   0x20,
                BRC2,   0x20,
                BPV2,   0x20,
                    ,   0x20,
                BCW2,   0x20,
                BCL2,   0x20,
                BG12,   0x20,
                BG22,   0x20,
                BOI2,   0x20,
                    ,   0x518,  //  skip
                AC01,   0x10,
                AC11,   0x10,
                PSV1,   0x10,
                CRT1,   0x10,
                TMP1,   0x10,
                AST1,   0x10,
                AC21,   0x10,
                AC31,   0x10,
                AC02,   0x10,
                AC12,   0x10,
                PSV2,   0x10,
                CRT2,   0x10,
                TMP2,   0x10,
                AST2,   0x10,
                AC22,   0x10,
                AC32,   0x10,
                AC03,   0x10,
                AC13,   0x10,
                PSV3,   0x10,
                CRT3,   0x10,
                TMP3,   0x10,
                AST3,   0x10,
                AC23,   0x10,
                AC33,   0x10,
                    ,   0x80,       //  skip
                TMPF,   0x10,
                    ,   0x570,  //  skip
                FANH,   1,
                FANL,   7,
                TF11,   1,
                TF21,   1,
                TF31,   1,
                    ,   1,
                TF10,   1,
                TF20,   1,
                TF30,   1,
                    ,   1,
                TP11,   1,
                TP21,   1,
                TP31,   1,
                    ,   0x6D,   //  109
                GP50,   1,
                GP51,   1,
                GP52,   1,
                GP53,   1,
                    ,   4,
                GP60,   1,
                GP61,   1,
                GP62,   1,
                GP63,   1,
                GP64,   1,
                GP65,   1,
                GP66,   1,
                    ,   1,
                GP70,   1,
                GP71,   1,
                GP72,   1,
                GP73,   1,
                GP74,   1,
                GP75,   1,
                GP76,   1,
                    ,   1,
                WED0,   1,
                WED1,   1,
                WED2,   1,
                WED3,   1,
                WED4,   1,
                    ,   3,
                SBL0,   1,
                SBL1,   1,
                SBL2,   1,
                SBL3,   1,
                    ,   4,
                LIDS,   1,
                VALF,   1,
                    ,   2,
                DCKI,   1,
                DCKF,   1,
                BT1F,   1,
                BT2F,   1,
                    ,   0x7D0,  //  skip
                HKCD,   8,
                    ,   8,
                DLID,   0x20,
                DSRN,   0x20,
                    ,   0x20,
                BDID,   0x20,
                DSPW,   1,
                VGAF,   1,
                VWE0,   1,
                VWE1,   1,
                PPSC,   1,
                SPSC,   1,
                EWLD,   1,
                EWPS,   1,
                    ,   0x1768, //  skip
                PRES,   0x8000
            }   //  Field:  SRAM overlay
        }   //  MEM

        Device (BAT1)
        {   //  BAT1
            Name (_HID, EISAID ("PNP0C0A"))     //  Control Method Battey ID
            Name (_UID, 1)
            Name (_PCL, Package (1) {\_SB})

            Method (_STA)
            {   //  _STA
                If (\_SB.MEM.BES1)
                    {   Return (0x1F)   }   //  battery present
                Else
                    {   Return (0x0F)   }   //  battery not present
            }   //  _STA

            Method (_BIF,, Serialized)
            {   //  _BIF
                Name (BUFR, Package (13)    {})

                Store (\_SB.MEM.BPU1, Index (BUFR, 0))
                Store (\_SB.MEM.BDC1, Index (BUFR, 1))
                Store (\_SB.MEM.BLF1, Index (BUFR, 2))
                Store (\_SB.MEM.BTC1, Index (BUFR, 3))
                Store (\_SB.MEM.BDV1, Index (BUFR, 4))
                Store (\_SB.MEM.BCW1, Index (BUFR, 5))
                Store (\_SB.MEM.BCL1, Index (BUFR, 6))
                Store (\_SB.MEM.BG11, Index (BUFR, 7))
                Store (\_SB.MEM.BG21, Index (BUFR, 8))
                Store (\_SB.MEM.BMN1, Index (BUFR, 9))
                Store (\_SB.MEM.BSN1, Index (BUFR, 10))
                Store (\_SB.MEM.BTP1, Index (BUFR, 11))
                Store (\_SB.MEM.BOI1, Index (BUFR, 12))

                Return (BUFR)
            }   //  _BIF
        }   //  BAT1

        Device (IDX2)
        {
            Method (B2IB,, Serialized)
            {   //  B2IB:   store from Buffer into Index'ed Buffer

                Name (SRCB, Buffer ()   {"Short Buffer"})   //  12 characters plus NULL

                Name (DEST, Buffer ()                           //  62 characters plus NULL
                    {"Destination buffer that is longer than the short source buffer"})


                //  verify object type returned by Index(Buffer,Element,)

                Store (Index (DEST, 2, ), Local1)
                Store (ObjectType (Local1), Local2)

                If (LNotEqual (Local2, 14))     //  Buffer Field is type 14
                {
                    //  Local2 indicates Local1 is not a Buffer Field

                    Return (0x61)
                }

                //  verify object type and value returned by DerefOf(Index(Buffer,Element,))
                //  should return Number containing element value

                Store (DerefOf (Local1), Local3)
                Store (ObjectType (Local3), Local4)

                If (LNotEqual (Local4, 1))          //  Number is type 1
                {
                    //  Local2 indicates Local1 is not a Number
                    Return (0x62)
                }
                Else
                {
                    If (LNotEqual (Local3, 0x73))       //  expect 's' element from DEST
                    {
                        Return (0x63)
                    }
                }

                Store ("DerefOf(Index(Buffer,,)) PASS", Debug)


                //
                // The following sections have been rewritten because storing into
                // an Indexed buffer only changes one byte - the FIRST byte of the
                // buffer is written to the source index. This is the ONLY byte
                // written -- as per ACPI 2.0
                //
                // Overwrite DEST contents, at buffer position 2 [only]

                Store (SRCB, Index (DEST, 2, ))

                //
                // Check that the next byte is not changed
                //
                Store (DerefOf (Index (DEST, 3, )), Local0)
                If (LNotEqual (Local0, 0x74))       //  't'
                {
                    //  DEST element is not matching original value
                    If (LEqual (Local0, 0x68))
                    {
                        //  DEST element was altered to 'h'
                        Return (0x68)
                    }
                    Else
                    {
                        // DEST element is an unknown value
                        Return (0x69)
                    }
                }

                //
                // Check that the elements beyond the SRCB buffer copy
                //  have not been altered.
                //
                Store (DerefOf (Index (DEST, 14)), Local0)

                //
                // This should be an 'f'.
                //
                If (LNotEqual (Local0, 0x66))
                {
                    //  DEST element was zero'd by buffer copy
                    If (LEqual (Local0, 0))
                    {
                        //  DEST element is zero
                        Return (0x6A)
                    }
                    Else
                    {
                        //  DEST element is unknown value
                        Return (0x6B)
                    }
                }

                Store ("Store(SRCB,Index(Buffer,,)) PASS", Debug)

                //
                //  verify altering SRCB does NOT alter DEST
                //
                Store (0x6A, Index (SRCB, 1))   //  SRCB = "Sjort Buffer"

                Store (DerefOf (Index (SRCB, 1)), Local0)

                If (LNotEqual (Local0, 0x6A))       //  'j'
                {
                    //  SRCB element is unaltered
                    Return (0x71)
                }

                Store (DerefOf (Index (DEST, 3)), Local0) // DEST = "Destination buffer that...

                If (LNotEqual (Local0, 0x74))       //  't'
                {
                    //  DEST element is altered
                    If (LEqual (Local0, 0x6A))  //  'j'
                    {
                        //  SRCB change altered DEST element
                        Return (0x72)
                    }
                    Else
                    {
                        //  DEST element is unknown value
                        Return (0x73)
                    }
                }

                //  verify altering DEST does NOT alter SRCB

                Store (0x6B, Index (DEST, 4, )) //  DEST = "DeSkination buffer..."

                Store (DerefOf (Index (DEST, 4, )), Local0)

                If (LNotEqual (Local0, 0x6B))       //  'k'
                {
                    //  DEST element is unaltered
                    Return (0x74)
                }

                Store (DerefOf (Index (SRCB, 2, )), Local0)

                If (LNotEqual (Local0, 0x6F))       //  'o'
                {   //  SRC element is altered
                    If (LEqual (Local0, 0x6B))  //  'k'
                    {
                        //  DEST change altered SRCB element
                        Return (0x75)
                    }
                    Else
                    {
                        //  SRCB element is unknown value
                        Return (0x76)
                    }
                }

                Store ("SRCB and DEST independent PASS", Debug)


                // verify string can be written to Index target/destination
                // Only FIRST byte is written

                Store ("New Buff", Index (DEST, 2, ))   //  DEST = "DeNkination buffer..."

                Store (DerefOf (Index (DEST, 2, )), Local0)

                If (LNotEqual (Local0, 0x4E))       //  'N'
                {
                    //  DEST element is unaltered
                    Return (0x81)
                }

                Store (DerefOf (Index (DEST, 6, )), Local0)

                If (LNotEqual (Local0, 0x61))       //  'a'
                {
                    //  DEST element is unaltered
                    Return (0x82)
                }

                Store (DerefOf (Index (DEST, 10, )), Local0)

                If (LNotEqual (Local0, 0x6E))       //  'n'
                {
                    //  DEST element is unaltered
                    Return (0x83)
                }

                Store ("Store(String,Index) PASS", Debug)


                Return (0)  //  pass
            }   //  B2IB:   store from Buffer into Index'ed Buffer

            Method (FB2P,, Serialized)
            {   //  FB2P:   store from Field Buffer into Index'ed Package
                Name (DEST, Package (2) {})

                //  initialize memory using 32-bit field elements
                Store (0x01234567, \_SB.MEM.SMD0)
                Store (0x89ABCDEF, \_SB.MEM.SMD1)
                Store (0xFEDCBA98, \_SB.MEM.SMD2)
                Store (0x76543210, \_SB.MEM.SMD3)

                //  move greater than 64-bit buffers into DEST package
                Store (\_SB.MEM.SME0, Index (DEST, 0))
                Store (\_SB.MEM.SME1, Index (DEST, 1))

                //  validate DEST contents
                Store (DerefOf (Index (DEST, 0, )), Local0)
                Store (DerefOf (Index (DEST, 1, )), Local1)

                //  verify Local0 and Local1 are Buffers
                Store (ObjectType (Local0), Local2)
                if (LNotEqual (Local2, 3))  //  Buffer type is 3
                {
                    Return (0x11)
                }

                Store (ObjectType (Local1), Local3)
                if (LNotEqual (Local3, 3))  //  Buffer type is 3
                {
                    Return (0x12)
                }

                //  validate DEST buffer contents
                Store (DerefOf (Index (DerefOf (Index (DEST, 0)), 0)), Local4)
                If (LNotEqual (Local4, 0x67))
                {
                    Return (0x13)
                }

                Store (DerefOf (Index (DerefOf (Index (DEST, 0)), 1)), Local4)
                If (LNotEqual (Local4, 0x45))
                {
                    Return (0x14)
                }

                Store (DerefOf (Index (DerefOf (Index (DEST, 0)), 4)), Local4)
                If (LNotEqual (Local4, 0xEF))
                {
                    Return (0x15)
                }

                Store (DerefOf (Index (DerefOf (Index (DEST, 0, )), 5, )), Local4)
                If (LNotEqual (Local4, 0xCD))
                {
                    Return (0x16)
                }

                Store ("Store(Mem,PkgElement) PASS", Debug)


                //  validate changing source \_SB.MEM.SMD* does not impact DEST
                Store (0x12345678, \_SB.MEM.SMD0)

                Store (DerefOf (Index (DerefOf (Index (DEST, 0, )), 0, )), Local5)
                If (LNotEqual (Local5, 0x67))
                {
                    Return (0x21)
                }

                Store (DerefOf (Index (DerefOf (Index (DEST, 0, )), 1, )), Local5)
                If (LNotEqual (Local5, 0x45))
                {
                    Return (0x22)
                }

                //  validate changing DEST does not impact source \_SB.MEM.SMD*
                Store (0x30, Index (DerefOf (Index (DEST, 0)), 0))

                Store (DerefOf(Index (DerefOf (Index (DEST, 0)), 0)), Local5)
                If (LNotEqual (Local5, 0x30))
                {
                    Return (0x23)
                }

                //
                // This section was modified from the original iPCO code because
                //  it attempted to compare two buffers. This is not allowed until
                //  ACPI v2.0, so the test has been modified to just check the
                //  changed \_SB.MEM.SMD0
                //
                Store (\_SB.MEM.SMD0, Local5)

                If(LNotEqual(Local5, 0x12345678))
                {
                    Return (0x24)
                }

                Store ("Mem and Pkg independent PASS", Debug)


                Return (0)
            }   //  FB2P:   store from Field Buffer into Index'ed Package

            Method (TEST)
            {
                Store ("++++++++ IndexOp2 Test", Debug)

                //  store _BIF package return value into Local0

                Store (\_SB.BAT1._BIF, Local0)

                //  save Local0 object type value into Local1
                Store (ObjectType (Local0), Local1)

                //  validate Local0 is a Package
                If (LNotEqual (Local1, 4))  //  Package type is 4
                {
                    //  failure
                    Return (2)
                }

                //  validate source and destination buffers are independent of each
                //  of each other (i.e., changing one's contents does not change
                //  other's contents) using B2IB (store from Buffer into Index'ed
                //  Buffer) and FB2P (store from Field Buffer into Index'ed Package)

                //  call B2IB (store from Buffer into Index'ed Buffer)
                Store (B2IB, Local2)    //  Local2 is B2IB return value

                //  save Local2 object type value into Local3
                Store (ObjectType (Local2), Local3)

                //  validate Local2 is a Number
                If (LNotEqual (Local3, 1))  //  Number type is 1
                {
                    //  failure
                    Return (4)
                }

                //  zero indicates pass, non-zero is an error code
                If (LNotEqual (Local2, 0))
                {
                    //  return B2IB error code
                    Return (Local2)
                }

                //  call FB2P (store from Field Buffer into Index'ed Package)
                Store (FB2P, Local2)    //  Local2 is FB2P return value

                //  save Local2 object type value into Local3
                Store (ObjectType (Local2), Local3)

                //  validate Local2 is a Number
                If (LNotEqual (Local3, 1))  //  Number type is 1
                {
                    //  failure
                    Return (5)
                }

                //  zero indicates pass, non-zero is an error code
                If (LNotEqual (Local2, 0))
                {
                    //  return FB2P error code
                    Return (Local2)
                }


                Return (0)
            }   //  TEST
        }   //  IDX2:   Test device name
    }   //  _SB system bus

//
// test SizeOf.asl
//
//  Test for SizeOf
//      test cases include following SizeOf arguments:
//          buffer, buffer field;
//          control method argument, control method local variable;
//          control method return values;
//          direct string, string;
//          package;
//          buffer, package, and string package elements
//
// MTL NOTE: This test has been modified to remove any SizeOf(Index(Buff,...
//  calls because it is not legal to perform a SizeOf operation on a Buffer Field.
//  This test has also been extended to test additional Package element sizes.
//
    Device (SIZO)
    {
        //  SAR0 control method validates SizeOf(Arg)
        //      SAR0 should only be called by SARG
        Method (SAR0, 2)
        //  Arg0    object to determine size of
        //  Arg1    expected Arg length
        {   //  SAR0:   SizeOf(Arg) test control method
            //  Local0  Arg0 length
            //  Local1  Local0 object type

            //  Store first string size (Arg0) into Local7
            Store (SizeOf (Arg0), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
                {   Return (0x21)   }

            //  If strings are not of equal size, return error code
            If (LNotEqual (Local0, Arg1))
                {   Return (0x22)   }

            Return (0)
        }   //  SAR0:   SizeOf(Arg) test control method

        Method (SARG,, Serialized)
        {   //  SARG:   SizeOf(Arg) test control method
            Name (BUFR, Buffer (12) {}) //  uninitialized Buffer
            Name (BUF1, Buffer() {0x01, 0x02, 0x03, 0x04, 0x05})
            Name (PKG0, Package (4) {}) //  uninitialized Package
            Name (STR0, "String")
            Name (PKG1, Package (4)
            {
                BUFR,
                "String2",
                STR0,
                PKG0
            })  //  PKG1

            Name (PKG2, Package (4)
            {
                Buffer (15) {},
                "String 1",
                Package (2) {}
            })  //  PKG2

            //  Namespace entry buffer reference
            Store (SAR0 (BUFR, 12), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x23)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=BUFR) PASS", Debug)


            //  Namespace entry package reference
            Store (SAR0 (PKG0, 4), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x24)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=PKG0) PASS", Debug)


            //  Namespace entry string reference
            Store (SAR0 (STR0, 6), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x25)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=STR0) PASS", Debug)


            //  direct string reference
            Store (SAR0 ("String", 6), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x26)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=String) PASS", Debug)

            Store (0x55, Index (BUF1, 2))

            /****************************************************
            //
            // This section is commented because it is illegal to
            //  perform a SizeOf operation on a Buffer Field
            //
            //  Namespace BufferField reference
            Store (SAR0 (Index (BUFR, 2, ), 10), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
                {   Return (0x27)   }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
                {   Return (Local0) }

            Store ("SizeOf(Arg=BufferField) PASS", Debug)
            ****************************************************/

            //  Namespace BufferPackageElement reference
            //
            Store (SAR0 (Index(PKG1, 0), 12), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x28)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=PackageBuffer NTE Reference Element) PASS", Debug)


            //  Namespace StringPackageElement reference
            Store (SAR0 (Index (PKG1, 1, ), 7), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x29)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=Package String Element) PASS", Debug)


            //  Namespace StringPackageElement reference
            Store (SAR0 (Index (PKG1, 2, ), 6), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x2A)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=Package String NTE Reference Element) PASS", Debug)


            //  Namespace PackagePackageElement reference
            Store (SAR0 (Index (PKG1, 3, ), 4), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x2B)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=Package Package NTE Reference Element) PASS", Debug)

            // Package Buffer Element
            Store (SAR0 (Index (PKG2, 0), 15), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x2B)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=Package Buffer Element) PASS", Debug)

            // Package String Element
            Store (SAR0 (Index (PKG2, 1), 8), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x2B)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=Package String Element) PASS", Debug)

            // Package Package Element
            Store (SAR0 (Index (PKG2, 2), 2), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x2B)
            }

            If (LNotEqual (Local0, 0))      //  Local0 is SAR0 return error code
            {
                Return (Local0)
            }

            Store ("SizeOf(Arg=Package Package Element) PASS", Debug)

            Store ("SizeOf(Arg) PASS", Debug)

            Return (0)
        }   //  SARG:   SizeOf(Arg) test control method

        Method (SBUF,, Serialized)
        {   //  SBUF:   SizeOf(Buffer) test control method
            Name (BUFR, Buffer (12) {})

            //  store size of BUFR buffer into Local0
            Store (SizeOf (BUFR), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x31)
            }

            If (LNotEqual (Local0, 12))     //  BUFR size is 12
            {
                Return (0x32)
            }

            Store ("SizeOf(BUFR) PASS", Debug)

            Return (0)
        }   //  SBUF:   SizeOf(Buffer) test control method


        /****************************************************
        //
        // This section is commented because it is illegal to
        //  perform a SizeOf operation on a Buffer Field
        //
        Method (SIND)
        {   //  SIND:   SizeOf(Index(,,)) test control method
            Name (BUFR, Buffer (12) {})

            //  store size of Index(BUFR,2,) buffer into Local0
            Store (SizeOf (Index (BUFR, 2, )), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x41)
            }

            If (LNotEqual (Local0, 10))     //  12 - 2 = 10
            {
                Return (0x42)
            }

            Store ("SizeOf(Index(BUFR,,)) PASS", Debug)

            //  TBD:    strings and packages

            Return (0)
        }   //  SIND:   SizeOf(Index(,,)) test control method
        ****************************************************/

        Method (SLOC,, Serialized)
        {   //  SLOC:   SizeOf(Local) test control method
            Name (BUFR, Buffer (12) {}) //  uninitialized Buffer
            Name (STR0, "String")
            Name (PKG0, Package (4) {}) //  uninitialized Package


            //  store BUFR Buffer into Local2
            Store (BUFR, Local2)

            //  store size of BUFR buffer into Local0
            Store (SizeOf (Local2), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x51)
            }

            If (LNotEqual (Local0, 12)) //  BUFR size is 12
            {
                Return (0x52)
            }

            Store ("SizeOf(Local2=Buffer) PASS", Debug)


            //  store STR0 string into Local2
            Store (STR0, Local2)

            //  store size of STR0 buffer into Local0
            Store (SizeOf (Local2), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x53)
            }

            If (LNotEqual (Local0, 6))      //  STR0 size is 6
            {
                Return (0x54)
            }

            Store ("SizeOf(Local2=String) PASS", Debug)


            //  store PKG0 Package into Local2
            Store (PKG0, Local2)

            //  store size of PKG0 buffer into Local0
            Store (SizeOf (Local2), Local0)

            //  save Local0 object type value into Local1
            Store (ObjectType (Local0), Local1)

            //  validate Local0 is a Number
            If (LNotEqual (Local1, 1))      //  Number type is 1
            {
                Return (0x55)
            }

            If (LNotEqual (Local0, 4))      //  PKG0 size is 4
            {
                Return (0x56)
            }

            Store ("SizeOf(Local2=Package) PASS", Debug)


            Return (0)
        }   //  SLOC:   SizeOf(Local) test control method

        Method (TEST)
        {
            Store ("++++++++ SizeOf Test", Debug)

            //  Store current operating system string into Local0
            Store (_OS, Local0)

            Store (SizeOf (_OS), Local3)

            //  save Local3 object type value into Local4
            Store (ObjectType (Local3), Local4)

            //  validate Local3 is a Number
            If (LNotEqual (Local4, 1))  //  Number type is 1
            {
                //  failure
                Return (0x61)
            }

            //  Store current operating system string into Local0
            //  This verifies above SizeOf(_OS) did not corrupt ACPI namespace
            Store (_OS, Local0)

            //  Store SARG [Validate SizeOf(Arg)] return value into Local1
            Store (SARG, Local1)

            //  save Local1 object type value into Local2
            Store (ObjectType (Local1), Local2)

            //  validate Local1 is a Number
            If (LNotEqual (Local2, 1))  //  Number type is 1
            {
                //  failure
                Return (0x62)
            }

            //  zero indicates pass, non-zero is an error code
            If (LNotEqual (Local1, 0))
            {
                //  return SARG error code
                Return (Local1)
            }


            //  Store SBUF [Validate SizeOf(Buffer)] return value into Local1
            Store (SBUF, Local1)

            //  save Local1 object type value into Local2
            Store (ObjectType (Local1), Local2)

            //  validate Local1 is a Number
            If (LNotEqual (Local2, 1))  //  Number type is 1
            {
                //  failure
                Return (0x63)
            }

            //  zero indicates pass, non-zero is an error code
            If (LNotEqual (Local1, 0))
            {
                //  return SBUF error code
                Return (Local1)
            }

            /****************************************************
            //
            // This section is commented because it is illegal to
            //  perform a SizeOf operation on a Buffer Field
            //
            //  Store SIND [verify SizeOf(Index(,,))] return value into Local1
            Store (SIND, Local1)

            //  save Local1 object type value into Local2
            Store (ObjectType (Local1), Local2)

            //  validate Local1 is a Number
            If (LNotEqual (Local2, 1))  //  Number type is 1
            {
                //  failure
                Return (0x64)
            }

            //  zero indicates pass, non-zero is an error code
            If (LNotEqual (Local1, 0))
            {
                //  return SARG error code
                Return (Local1)
            }
            ****************************************************/

            //  Store SLOC [verify SizeOf(Local)] return value into Local1
            Store (SLOC, Local1)

            //  save Local1 object type value into Local2
            Store (ObjectType (Local1), Local2)

            //  validate Local1 is a Number
            If (LNotEqual (Local2, 1))  //  Number type is 1
            {
                //  failure
                Return (0x65)
            }

            //  zero indicates pass, non-zero is an error code
            If (LNotEqual (Local1, 0))
            {
                //  return SLOC error code
                Return (Local1)
            }


            //  TBD:    SizeOf (METH) -- where METH control method returns
            //              buffer, BufferField, string, package, package element


            Return (0)
        }   //  TEST
    }   //  SIZO

//
// test SmiShare.asl
//
    Scope (\_SB)    //  System Bus
    {   //  _SB system bus
        //  Declare an OpRegion in Memory starting at offset 0x400000 that is 10 bytes long
        OperationRegion(RAM1, SystemMemory, 0x400000, 0xA)

        Field (RAM1, AnyAcc, NoLock, Preserve)
        {
            BI1T, 1,        // Create some bits in memory to access
            BI2T, 2,
            BI3T, 3,
            LST2, 2
        }   //  End Field RAM1

        Field (RAM1, WordAcc, NoLock, WriteAsZeros)
        {
            WRD, 16
        }   //  End 2nd Field RAM1

        Field (RAM1, ByteAcc, NoLock, WriteAsOnes)
        {
            BYTE, 8
        }   //  End 3rd Field RAM1

        Field (RAM1, ByteAcc, NoLock, Preserve)
        {
            SMIC, 8,
            SMID, 8
        }

        Device (MBIT)
        {
            Method (_INI)
            {
                Store (0, BI1T)
                Store (3, BI2T)
                Store (7, BI3T)
                Store (0, LST2)
            }   //  End _INI Method
        }   //  End Device MBIT

        Device (MWRD)
        {
            Method (_INI)
            {
                Store (0, WRD)
            }   //  End _INI Method
        }   //  End Device MWRD

        Device (MBYT)
        {
            Method (_INI)
            {
                Store (0, BYTE)
                Store (0xC, SMIC)
                Store (0xD, SMID)
            }   //  End _INI Method
        }   //  End Device MBYT

    /*
        //  Declare an OpRegion in Memory starting at offset 0x400000 that is 10 bytes long
        OperationRegion(\RAM1, SystemMemory, 0x400000, 0xA)

        Field (\RAM1, AnyAcc, NoLock, Preserve)
        {
            BI1T, 1,        // Create some bits in memory to access
            BI2T, 2,
            BI3T, 3,
            LST2, 2
        }   //  End Field RAM1

        Field (\RAM1, WordAcc, NoLock, WriteAsZeros)
        {
            WRD, 16
        }   //  End 2nd Field RAM1

        Field (\RAM1, ByteAcc, NoLock, WriteAsOnes)
        {
            BYTE, 8
        }   //  End 3rd Field RAM1

        Field (\RAM1, ByteAcc, NoLock, Preserve)
        {
            SMIC, 8,
            SMID, 8
        }
    */
        Method (SMIX)
        {
            Return (BYTE)
        }   //  End SMIX

        Method (EVNT)
        {
            Store (SMIX, Local0)

            Notify (\_SB_, 0x29)
            If (And (Local0, 0x01))
            {   Notify (\_SB_.SMIS, 0x21)}

            If (And (Local0, 0x02))
            {   Notify (\_SB_.SMIS, 0x22)}

            If (And (Local0, 0x04))
            {   Notify (\_SB_.SMIS, 0x24)}

            If (And (Local0, 0x08))
            {   Notify (\_SB_.SMIS, 0x28)}

        }   //  End Method EVNT

        Method (NTFY)
        {
            Notify (\_SB_, 1)
            Notify (\_TZ_.TZ1, 2)
            Notify (\_PR_.CPU0, 3)

            Notify (\_SB_, 0x81)
            Notify (\_TZ_.TZ1, 0x82)
            Notify (\_PR_.CPU0, 0x83)
        }

        Device (SMIS)
        {
            Method (BINK)
            {
                Store (0, Local0)               //  Zero out Local0

                If (LNotEqual (SMID, 0xD))
                {   Or (0x80, Local0, Local0)}

                If (LNotEqual (SMIC, 0xC))
                {   Or (0x40, Local0, Local0)}

                If (LNotEqual (BYTE, 0))
                {   Or (0x20, Local0, Local0)}

                If (LNotEqual (WRD, 0))
                {   Or (0x10, Local0, Local0)}

                If (LNotEqual (LST2, 0))
                {   Or (0x8, Local0, Local0)}

                If (LNotEqual (BI3T, 0x7))
                {   Or (0x4, Local0, Local0)}

                If (LNotEqual (BI2T, 0x3))
                {   Or (0x2, Local0, Local0)}

                If (LNotEqual (BI1T, 0))
                {   Or (0x1, Local0, Local0)}

                Return (Local0)
            }   //  End Method BINK

            Method (TEST)
            {
                Store ("++++++++ SmiShare Test", Debug)

                //  Expect EVNT to generate Notify value we just previously
                //  stored in BYTE

                Store (0x20, BYTE)
                EVNT ()
                Store (0x21, BYTE)
                EVNT ()
                Store (0x22, BYTE)
                EVNT ()
                Store (0x23, BYTE)
                EVNT ()

                NTFY ()
                Return (0)  //  pass
            }   //  End Method TEST
        }   //  Device SMIS

        Device(CNDT)
        {
            Method(TEST)
            {
                If (ECOK)
                {
                    return("Broken")
                }
                Else
                {
                    return("Works")
                }
            }

            Method(ECOK)
            {
                Return(0x0)
            }
        }

    }   //  _SB system bus


/* Test a very big buffer */

    Name(WQAB, Buffer(6756)
    {
        0x46,0x4F,0x4D,0x42,0x01,0x00,0x00,0x00,
        0x54,0x1A,0x00,0x00,0xBA,0xAD,0x00,0x00,
        0x44,0x53,0x00,0x01,0x1A,0x7D,0xDA,0x54,
        0x98,0xBD,0x92,0x00,0x01,0x06,0x18,0x42,
        0x10,0x47,0x10,0x92,0x46,0x62,0x02,0x89,
        0x80,0x90,0x18,0x18,0x14,0x81,0x85,0x00,
        0x49,0x02,0x88,0xC4,0x41,0xE1,0x20,0xD4,
        0x9F,0x40,0x7E,0x05,0x20,0x74,0x28,0x40,
        0xA6,0x00,0x83,0x02,0x9C,0x22,0x88,0xA0,
        0x57,0x01,0x36,0x05,0x98,0x14,0x60,0x51,
        0x80,0x76,0x01,0x96,0x05,0xE8,0x16,0x20,
        0x1D,0x96,0x88,0x04,0x47,0x89,0x01,0x47,
        0xE9,0xC4,0x16,0x6E,0xD8,0xE0,0x85,0xA2,
        0x68,0x06,0x51,0x12,0x94,0x8B,0x20,0x5D,
        0x10,0x52,0x2E,0xC0,0x37,0x82,0x06,0x10,
        0xA5,0x77,0x01,0xB6,0x05,0x98,0x86,0x27,
        0xD2,0x20,0xE4,0x60,0x08,0x54,0xCE,0x80,
        0x20,0x69,0x44,0x21,0x1E,0xA7,0x44,0x08,
        0x0A,0x84,0x90,0xD4,0xF1,0xA0,0xA0,0x71,
        0x88,0xAD,0xCE,0x46,0x93,0xA9,0x74,0x7E,
        0x48,0x82,0x70,0xC6,0x2A,0x7E,0x3A,0x9A,
        0xD0,0xD9,0x9C,0x60,0xE7,0x18,0x72,0x3C,
        0x48,0xF4,0x20,0xB8,0x00,0x0F,0x1C,0x2C,
        0x34,0x84,0x22,0x6B,0x80,0xC1,0x8C,0xDD,
        0x63,0xB1,0x0B,0x4E,0x0A,0xEC,0x61,0xB3,
        0x01,0x19,0xA2,0x24,0x38,0xD4,0x11,0xC0,
        0x12,0x05,0x98,0x1F,0x87,0x0C,0x0F,0x95,
        0x8C,0x25,0x24,0x1B,0xAB,0x87,0xC2,0xA5,
        0x40,0x68,0x6C,0x27,0xED,0x19,0x45,0x2C,
        0x79,0x4A,0x82,0x49,0xE0,0x51,0x44,0x36,
        0x1A,0x27,0x28,0x1B,0x1A,0x25,0x03,0x42,
        0x9E,0x05,0x58,0x07,0x26,0x04,0x76,0x2F,
        0xC0,0x9A,0x00,0x73,0xB3,0x90,0xB1,0xB9,
        0xE8,0xFF,0x0F,0x71,0xB0,0x31,0xDA,0x9A,
        0xAE,0x90,0xC2,0xC4,0x88,0x12,0x2C,0x5E,
        0xC5,0xC3,0x10,0xCA,0x93,0x42,0xA8,0x48,
        0x95,0xA1,0x68,0xB4,0x51,0x2A,0x14,0xE0,
        0x4C,0x80,0x30,0x5C,0x1D,0x03,0x82,0x46,
        0x88,0x15,0x29,0x56,0xFB,0x83,0x20,0xF1,
        0x2D,0x40,0x54,0x01,0xA2,0x48,0xA3,0x41,
        0x9D,0x03,0x3C,0x5C,0x0F,0xF5,0xF0,0x3D,
        0xF6,0x93,0x0C,0x72,0x90,0x67,0xF1,0xA8,
        0x70,0x9C,0x06,0x49,0xE0,0x0B,0x80,0x4F,
        0x08,0x1E,0x38,0xDE,0x35,0xA0,0x66,0x7C,
        0xBC,0x4C,0x10,0x1C,0x6A,0x88,0x1E,0x68,
        0xB8,0x13,0x38,0x44,0x06,0xE8,0x49,0x3D,
        0x52,0x60,0x07,0x77,0x32,0xEF,0x01,0xAF,
        0x0A,0xCD,0x5E,0x12,0x08,0xC1,0xF1,0xF8,
        0x7E,0xC0,0x26,0x9C,0xC0,0xF2,0x07,0x81,
        0x1A,0x99,0xA1,0x3D,0xCA,0xD3,0x8A,0x19,
        0xF2,0x31,0xC1,0x04,0x16,0x0B,0x21,0x05,
        0x10,0x1A,0x0F,0xF8,0x6F,0x00,0x8F,0x17,
        0xBE,0x12,0xC4,0xF6,0x80,0x12,0x0C,0x0B,
        0x21,0x23,0xAB,0xF0,0x78,0xE8,0x28,0x7C,
        0x95,0x38,0x9C,0xD3,0x8A,0x67,0x82,0xE1,
        0x20,0xF4,0x05,0x90,0x00,0x51,0xE7,0x0C,
        0xD4,0x61,0xC1,0xE7,0x04,0x76,0x33,0x38,
        0x83,0x47,0x00,0x8F,0xE4,0x84,0xFC,0x2B,
        0xF1,0xC0,0xE0,0x03,0xE2,0xEF,0x1F,0xA7,
        0xEC,0x11,0x9C,0xA9,0x01,0x7D,0x1C,0xF0,
        0xFF,0x7F,0x28,0x7C,0x88,0x1E,0xDF,0x29,
        0x1F,0xAF,0x4F,0x17,0x96,0x35,0x4E,0xE8,
        0x77,0x08,0x9F,0x38,0x7C,0x64,0x71,0x44,
        0x08,0x39,0x39,0x05,0xA0,0x81,0x4F,0xF7,
        0xEC,0x22,0x9C,0xAE,0x27,0xE5,0x40,0xC3,
        0xA0,0xE3,0x04,0xC7,0x79,0x00,0x1C,0xE3,
        0x84,0x7F,0x2E,0x80,0x3F,0x40,0x7E,0xCA,
        0x78,0xC5,0x48,0xE0,0x98,0x23,0x44,0x9F,
        0x6B,0x3C,0x42,0x2C,0xFC,0x53,0x45,0xE1,
        0x03,0x21,0x63,0x04,0x17,0xA0,0xC7,0x08,
        0x7C,0x03,0x8E,0x11,0x7D,0x94,0xE0,0xEA,
        0x0F,0x1A,0x74,0x80,0xB8,0xFF,0xFF,0x00,
        0xE1,0x83,0x7A,0x80,0xC0,0x37,0xFA,0xD1,
        0x03,0x3D,0x2E,0x8B,0x3E,0x0F,0xC8,0xF8,
        0x89,0x46,0xF3,0xE2,0xA7,0x03,0x7E,0xF8,
        0x00,0x0F,0xA8,0x87,0x84,0x03,0xC5,0x4C,
        0x9B,0x83,0x3E,0xBB,0x1C,0x3A,0x76,0xB8,
        0xE0,0x3F,0x81,0x80,0x4B,0xDE,0x21,0x0C,
        0x14,0x23,0xC6,0x9F,0x83,0x7C,0x0A,0x03,
        0xFF,0xFF,0xFF,0x14,0x06,0xFE,0xE1,0xF0,
        0x20,0x4F,0x07,0x9F,0xB6,0xA8,0x74,0x18,
        0xD4,0x81,0x0B,0xB0,0x32,0x89,0x08,0xCF,
        0x12,0xB5,0x41,0xE8,0xD4,0xF0,0x36,0xF1,
        0xB6,0xE5,0x5B,0x40,0x9C,0xD3,0xEC,0xED,
        0xC0,0x45,0x30,0x22,0xD4,0x0C,0x45,0x4E,
        0x5A,0x11,0x63,0x44,0x79,0xDC,0x32,0xCA,
        0xDB,0xD6,0x0B,0x40,0xBC,0x13,0x7B,0xDE,
        0x32,0x46,0xF0,0xC8,0x0F,0x5C,0x2C,0xC6,
        0xEA,0xF5,0x5F,0xF3,0x81,0x0B,0x70,0xF6,
        0xFF,0x3F,0x70,0x01,0x1C,0x0A,0x7A,0x18,
        0x42,0x0F,0xC3,0x53,0x39,0x97,0x87,0xC8,
        0x53,0x89,0x18,0x35,0x4C,0xD4,0x67,0x28,
        0xDF,0x2D,0x7C,0x20,0x02,0xDF,0x99,0x0B,
        0xF8,0xFD,0xFF,0x0F,0x44,0x70,0x8E,0x29,
        0xB8,0x33,0x0D,0x78,0x7C,0xCE,0x40,0x20,
        0xA7,0xE2,0x43,0x0D,0x60,0x41,0xF4,0x13,
        0xC2,0x27,0x1A,0x2A,0x13,0x06,0x75,0xA8,
        0x01,0xAC,0x5C,0x61,0x9E,0x46,0xCF,0xF9,
        0x59,0xC6,0xA7,0x1A,0x1F,0x4A,0x8D,0x63,
        0x88,0x97,0x99,0x87,0x1A,0x1F,0x0B,0x5E,
        0x49,0x7D,0xA8,0x31,0x54,0x9C,0x87,0x1A,
        0x0F,0x37,0x50,0xD4,0x37,0x9B,0x67,0x1B,
        0xA3,0xC7,0xF7,0x0D,0xD5,0x10,0x0F,0x35,
        0x4C,0xF2,0x4A,0x35,0x16,0x1F,0x6A,0xC0,
        0xF1,0xFF,0x3F,0xD4,0x00,0xFC,0xFF,0xFF,
        0x1F,0x6A,0x00,0x47,0x47,0x03,0x38,0x47,
        0x46,0xDC,0xD1,0x00,0x5C,0x87,0x52,0xE0,
        0x70,0x34,0x00,0x1E,0x47,0x21,0x30,0x5F,
        0x68,0x7C,0x14,0x02,0x16,0xFF,0xFF,0xA3,
        0x10,0xF8,0x65,0x9F,0x83,0x50,0x42,0x8F,
        0x42,0x80,0xA0,0xDB,0xCF,0x53,0xC4,0xB3,
        0x8F,0x2F,0x3F,0x0F,0x04,0x11,0x5E,0xF3,
        0x7D,0x0A,0xF2,0x21,0xDF,0x47,0x21,0x06,
        0x63,0x28,0x5F,0x83,0x7C,0x14,0x62,0x50,
        0xAF,0x41,0xBE,0xEF,0x1B,0xE4,0xF1,0x22,
        0x48,0xEC,0x67,0x02,0x1F,0x85,0x98,0xE8,
        0xA3,0x10,0xA0,0xF0,0xFF,0x7F,0x14,0x02,
        0xF8,0xFF,0xFF,0x3F,0x0A,0x01,0xCE,0x02,
        0x1C,0x0D,0x40,0x37,0xAD,0x47,0x21,0xF0,
        0xDE,0x59,0x4E,0xFB,0x04,0x7C,0x16,0x02,
        0xCC,0xFE,0xFF,0xCF,0x42,0xC0,0xEC,0x28,
        0x74,0x14,0x67,0xF9,0x2A,0xF4,0x04,0xF0,
        0x02,0x10,0x23,0xCC,0x3B,0xD0,0x4B,0x26,
        0xBB,0x8B,0x1B,0xE7,0xC9,0xE5,0x2C,0x9E,
        0xC4,0x7D,0x09,0xF2,0x81,0xE2,0x59,0xC8,
        0x50,0xA7,0x1B,0xF4,0x8D,0xDC,0x03,0x8B,
        0x19,0x3F,0xC4,0xF3,0x90,0x21,0x9E,0x85,
        0x00,0x76,0xFD,0xFF,0xCF,0x42,0x00,0xFF,
        0xFF,0xFF,0x47,0x03,0xF8,0x2F,0x00,0x9F,
        0x85,0x80,0xE7,0x09,0xE0,0x41,0xDB,0x67,
        0x21,0x80,0x33,0x87,0xCB,0xF3,0x7F,0x05,
        0x3A,0x96,0xF7,0x08,0xCF,0xFA,0x24,0x5F,
        0x2F,0x3D,0xD3,0x87,0x82,0x67,0x21,0x86,
        0x75,0x18,0x3E,0x0B,0x31,0x88,0x17,0x4D,
        0x43,0xBC,0x70,0xFA,0x30,0xE0,0xFF,0x3F,
        0x5E,0xE0,0x57,0x4E,0x03,0x05,0x09,0xF4,
        0x2C,0x04,0x30,0xFE,0xFF,0x7F,0x16,0x02,
        0xC8,0xB8,0x46,0x9D,0x85,0x80,0xE5,0x6D,
        0xE5,0x19,0xDB,0xA7,0x95,0x04,0xFF,0xFF,
        0x67,0x21,0xC0,0x41,0x2E,0x23,0x07,0x21,
        0x4C,0xC4,0x87,0x83,0x8F,0x99,0x80,0x9E,
        0x29,0xBE,0xB8,0x1B,0xE3,0x09,0xE0,0x45,
        0xE2,0x31,0x93,0x1D,0x35,0x0D,0xF3,0x2C,
        0x64,0xBC,0xB3,0x78,0x0D,0x78,0x82,0xF7,
        0xE4,0x9F,0x85,0x18,0xD8,0x61,0x05,0x7B,
        0x14,0x32,0xA8,0xC1,0x63,0x87,0x08,0x13,
        0xE8,0x59,0x88,0xC5,0x7D,0xAE,0xE8,0x3C,
        0xE1,0xB3,0x10,0xF0,0xFE,0xFF,0x9F,0x25,
        0xE0,0x5E,0x0D,0x9E,0x85,0x00,0x13,0x87,
        0x0D,0x9F,0x35,0xC0,0x33,0x7C,0x8F,0xEA,
        0x1C,0x1E,0x8F,0x81,0x7F,0x56,0x1D,0xE7,
        0x04,0x96,0x7B,0xD1,0xB2,0x71,0xA0,0xA1,
        0x23,0xB2,0x3A,0x20,0x8D,0x0D,0x73,0x29,
        0x89,0x7C,0x72,0x6C,0xD4,0x56,0x04,0xA7,
        0x33,0x93,0x4F,0x00,0xD6,0x42,0x21,0x05,
        0x34,0x1A,0x8B,0xE1,0x9D,0xF9,0xE8,0x44,
        0x41,0x0C,0xE8,0xE3,0x90,0x6D,0x1C,0x0A,
        0x50,0x7B,0xD1,0x14,0xC8,0x39,0x07,0xA3,
        0x7F,0x76,0x74,0x36,0xBE,0x13,0x70,0x0D,
        0x10,0x3A,0x25,0x18,0xDA,0x6A,0x04,0xFC,
        0xFF,0x67,0x89,0x01,0x33,0xFE,0x53,0x8C,
        0x09,0x7C,0x8E,0xC1,0x1F,0x0C,0xF0,0x03,
        0x7F,0x31,0xA8,0xFA,0x5E,0xA0,0xFB,0x82,
        0xD5,0xDD,0x64,0x20,0xCC,0xC8,0x04,0xF5,
        0x9D,0x0E,0x40,0x01,0xE4,0x0B,0x81,0xCF,
        0x51,0x0F,0x05,0x6C,0x22,0x21,0xC2,0x44,
        0x33,0x3A,0x62,0xC2,0xA8,0xE8,0x13,0xA6,
        0x20,0x9E,0xB0,0x63,0x4D,0x18,0x3D,0x13,
        0x5F,0x74,0xD8,0x88,0x31,0x21,0xAE,0x1E,
        0xD0,0x26,0x18,0xD4,0x97,0x22,0x58,0x43,
        0xE6,0x63,0xF1,0x05,0x02,0x37,0x65,0x30,
        0xCE,0x89,0x5D,0x13,0x7C,0xD9,0xC1,0xCD,
        0x19,0x8C,0xF0,0x98,0xBB,0x18,0xBF,0x3A,
        0x79,0x74,0xFC,0xA0,0xE0,0x1B,0x0E,0xC3,
        0x7E,0x32,0xF3,0x8C,0xDE,0xCB,0x7C,0x8D,
        0xC3,0xC0,0x7A,0xBC,0x1C,0xD6,0x68,0x61,
        0x0F,0xED,0x3D,0xC4,0xFF,0xFF,0x43,0x8C,
        0xCF,0x13,0xC6,0x08,0xEB,0xDB,0x0B,0x38,
        0xEE,0x59,0xF0,0xEF,0x1A,0xE0,0xB9,0x84,
        0xF8,0xAE,0x01,0x30,0xF0,0xFF,0x7F,0xD7,
        0x00,0x4E,0xD7,0x04,0xDF,0x35,0x80,0xF7,
        0xD0,0x7D,0xD7,0x00,0xAE,0xD9,0xEF,0x1A,
        0xA8,0x63,0x80,0x15,0xDE,0x35,0xA0,0x5D,
        0xD9,0xDE,0xD7,0x9E,0xB0,0xAC,0xE9,0xB2,
        0x81,0x52,0x73,0xD9,0x00,0x14,0xFC,0xFF,
        0x2F,0x1B,0x80,0x01,0x29,0x13,0x46,0x85,
        0x9F,0x30,0x05,0xF1,0x84,0x1D,0xEC,0xB2,
        0x01,0x8A,0x18,0x97,0x0D,0xD0,0x8F,0xED,
        0x65,0x03,0x18,0xDC,0x13,0xF8,0x6D,0x03,
        0x78,0x43,0xFA,0xB6,0x01,0xD6,0xFF,0xFF,
        0x6D,0x03,0xAC,0xF9,0x6F,0x1B,0x28,0x0E,
        0xAB,0xBC,0x6D,0x40,0x3C,0xC9,0x33,0x02,
        0xAB,0xBA,0x6E,0xA0,0xF4,0x5C,0x37,0x00,
        0x12,0x88,0x99,0x30,0x2A,0xFE,0x84,0x29,
        0x88,0x27,0xEC,0x68,0xD7,0x0D,0x50,0x04,
        0xB9,0x6E,0x80,0x7E,0x5E,0x09,0xFE,0xFF,
        0xAF,0x1B,0xC0,0xE0,0xA2,0x80,0xB9,0x6F,
        0x00,0x6F,0x58,0x7E,0xDF,0x00,0x7C,0xDC,
        0xC4,0x31,0xF7,0x0D,0xC0,0xCC,0xFF,0xFF,
        0xBE,0x01,0xB0,0xE7,0xA2,0x80,0xBB,0x6F,
        0x00,0xEF,0x8B,0xB4,0xEF,0x1B,0x60,0xFE,
        0xFF,0xDF,0x37,0xC0,0x28,0x6D,0xFD,0x1E,
        0x1C,0x3D,0x21,0x78,0x7C,0xB8,0xFB,0xA5,
        0xC7,0xE7,0xBB,0x39,0x38,0x06,0x79,0x8C,
        0x87,0x76,0xC0,0xAF,0xEF,0x9E,0x98,0xEF,
        0xE6,0xC0,0xFF,0x4C,0x70,0x3C,0x18,0x68,
        0x1C,0x62,0xAB,0x97,0x06,0x72,0x34,0x38,
        0x3F,0xDC,0x19,0x81,0x61,0x15,0x7F,0xF2,
        0x47,0x38,0xC7,0xD0,0xD9,0xE1,0x20,0xB1,
        0x83,0xE0,0xC1,0x56,0x6D,0x02,0x85,0x86,
        0x50,0x14,0x18,0x14,0x8B,0x0F,0x18,0xF8,
        0x61,0xB3,0xB3,0x00,0x93,0x04,0x87,0x3A,
        0x02,0xF8,0x3E,0xD1,0xFC,0x38,0x74,0x37,
        0x38,0x54,0x8F,0xE5,0xA1,0x80,0x9E,0x01,
        0x71,0xC7,0x0C,0x32,0x69,0xCF,0x28,0xE2,
        0x53,0xC2,0x29,0x85,0x49,0xE0,0xF3,0x03,
        0x43,0xE3,0x04,0xAF,0x0D,0xA1,0xF9,0xFF,
        0xFF,0xA4,0xC0,0x3C,0xDF,0x31,0x04,0x6C,
        0x02,0xBB,0xBF,0x64,0xC8,0xDA,0xC0,0x75,
        0x4B,0x32,0x44,0x6F,0x38,0xB2,0x85,0xA2,
        0xE9,0x44,0x79,0xDF,0x88,0x62,0x67,0x08,
        0xC2,0x88,0x12,0x2C,0xC8,0xA3,0x42,0xAC,
        0x28,0x2F,0x05,0x46,0x88,0x18,0xE2,0x95,
        0x23,0xD0,0x09,0x87,0x0F,0xF2,0xD8,0x14,
        0xA7,0xFD,0x41,0x90,0x58,0x4F,0x02,0x8D,
        0xC5,0x91,0x46,0x83,0x3A,0x07,0x78,0xB8,
        0x3E,0xC4,0x78,0xF8,0x0F,0x21,0x06,0x39,
        0xC8,0x73,0x7B,0x54,0x38,0x4E,0x5F,0x25,
        0x4C,0xF0,0x02,0xE0,0x83,0x0A,0x1C,0xD7,
        0x80,0x9A,0xF1,0x33,0x06,0x58,0x8E,0xE3,
        0x3E,0xA9,0xC0,0x1D,0x8F,0xEF,0x07,0x6C,
        0xC2,0x09,0x2C,0x7F,0x10,0xA8,0xE3,0x0C,
        0x9F,0xE7,0x0B,0x8B,0x21,0x1F,0x13,0x4C,
        0x60,0xB1,0x27,0x1B,0x3A,0x1E,0xF0,0xDF,
        0x63,0x1E,0x2F,0x7C,0x32,0xF1,0x7C,0x4D,
        0x30,0x22,0x84,0x9C,0x8C,0x07,0x7D,0x87,
        0xC0,0x5C,0x6F,0xD8,0xB9,0x85,0x8B,0x3A,
        0x68,0xA0,0x4E,0x0B,0x3E,0x28,0xB0,0x9B,
        0x11,0xE6,0xB8,0xCE,0xCF,0x2A,0x60,0xF8,
        0xFF,0x9F,0x55,0x60,0x8F,0x10,0xFE,0xED,
        0xC1,0xF3,0xF2,0x95,0xE1,0xD5,0x21,0x81,
        0x43,0x8E,0x10,0x3D,0x2E,0x8F,0x10,0x73,
        0x3E,0xC2,0x0C,0x11,0x5C,0x67,0x01,0x70,
        0x0C,0x11,0xF8,0x1C,0x70,0xC0,0x71,0x69,
        0xE2,0x03,0xF5,0x01,0x07,0x70,0x70,0x4D,
        0xC3,0x1D,0x70,0xC0,0x71,0x16,0x60,0xFF,
        0xFF,0xC3,0x0D,0x2C,0x49,0x26,0x0E,0x23,
        0x18,0x11,0x30,0x28,0x02,0x02,0xA4,0xB3,
        0x80,0x0F,0x29,0x00,0x1F,0xAE,0x0C,0x0F,
        0x29,0xD8,0x93,0x86,0x07,0x8E,0x1B,0x85,
        0x07,0x8D,0x0B,0x30,0x68,0x7A,0xE2,0x80,
        0x7F,0x4C,0xF0,0x19,0x05,0x1C,0xE3,0x06,
        0xDF,0x2A,0x0C,0xFC,0xFF,0x3F,0x30,0xCC,
        0xE1,0xC2,0x63,0x39,0x8A,0xA0,0x07,0x1E,
        0xD4,0xF7,0x8C,0x33,0xF7,0x24,0x8F,0xD1,
        0x51,0x0F,0x27,0xF4,0xE4,0x85,0x3B,0x57,
        0xF9,0x0A,0x71,0x14,0x18,0xB8,0x77,0x29,
        0x8F,0xCF,0x17,0x2B,0xC3,0x63,0x46,0xFB,
        0x1E,0x72,0xD6,0x11,0x02,0xE2,0x2F,0x75,
        0x6C,0xC0,0x60,0x39,0x18,0x00,0x87,0x01,
        0xE3,0x13,0x0D,0x58,0x67,0x1B,0x3C,0xF4,
        0x69,0x31,0xC4,0xE3,0x0B,0xFB,0x56,0x61,
        0x82,0xEA,0x41,0x75,0x12,0xF4,0xD0,0xC0,
        0x01,0xE8,0xA1,0xC1,0x3F,0xB9,0x90,0xFB,
        0x2B,0x1D,0x82,0xB5,0xE2,0x69,0xDE,0x47,
        0x1E,0xF3,0xDC,0xA2,0xBC,0x0D,0x3C,0x07,
        0xF0,0xD3,0x82,0x87,0xE3,0x63,0x81,0xC7,
        0xE9,0x4B,0x58,0x82,0xF7,0x1A,0x9F,0x6C,
        0x1E,0x5C,0x58,0xB2,0x21,0xA0,0x06,0xEB,
        0x21,0x60,0xA6,0x9A,0xC0,0x49,0x46,0x80,
        0xCA,0x00,0xA1,0x1B,0xCB,0xE9,0x3E,0x8B,
        0x84,0x38,0xCD,0x47,0x99,0xC7,0x02,0x8F,
        0xF5,0xC1,0xC0,0xFF,0x7F,0xCD,0x23,0xD4,
        0x7D,0xCD,0x33,0x7B,0x3A,0xC0,0xAC,0x22,
        0xDC,0x7B,0xCE,0x1B,0x86,0xD1,0x9E,0x2D,
        0x7C,0xCD,0x78,0xD6,0x34,0x42,0x38,0x76,
        0x83,0xF3,0x48,0x8C,0xF0,0x82,0xC0,0x4E,
        0x0C,0x0F,0x30,0xC6,0x39,0x79,0xC3,0xFA,
        0xC2,0xCB,0x40,0x83,0x19,0xDB,0x97,0x01,
        0x36,0x2A,0xDF,0x88,0xC0,0x97,0xFC,0x62,
        0x00,0x65,0x16,0xBE,0x9E,0xF8,0xA0,0xC4,
        0x2E,0x06,0x2C,0xE5,0xC5,0x00,0x54,0x37,
        0x0C,0x5F,0x0C,0xE0,0x5F,0x89,0x5E,0x0C,
        0xC0,0x70,0x71,0xF2,0x3D,0xC0,0x1E,0xEE,
        0xA3,0x74,0x9C,0xBE,0xFD,0xBD,0x19,0xF8,
        0x6C,0xC0,0x60,0x3C,0xC3,0x30,0xC6,0x08,
        0xE3,0x51,0x86,0x31,0xC1,0xDC,0xB7,0x03,
        0xE8,0x39,0x87,0x81,0x4A,0x78,0x3B,0x80,
        0x72,0x0E,0xE8,0xF2,0x68,0x42,0x4F,0x01,
        0x4F,0x07,0x3E,0x29,0x1A,0xA2,0xAF,0xB1,
        0x0A,0x26,0x50,0xC4,0x07,0x0D,0x3E,0xB5,
        0x28,0x3E,0x15,0x78,0x2D,0xCF,0x4E,0xE1,
        0xE2,0x9C,0x89,0xA7,0x6A,0x38,0x03,0xBD,
        0xE6,0x86,0x63,0xFF,0x7F,0x38,0xFC,0xA9,
        0xE0,0x35,0x80,0x1D,0x24,0x3D,0x2D,0x23,
        0xC2,0x38,0xA4,0x3C,0x32,0xF8,0xB6,0x18,
        0xC7,0x90,0x0F,0x91,0xBE,0x13,0x18,0xF2,
        0x21,0xEF,0x79,0xC7,0xC0,0xAF,0x08,0x71,
        0x9E,0xB2,0x7C,0x67,0xF0,0x65,0x01,0x7C,
        0x91,0x2E,0x0B,0x68,0x68,0x9F,0x64,0x7C,
        0x41,0x30,0xEC,0x89,0xB3,0x00,0x77,0x05,
        0x50,0x81,0xFA,0xAE,0x00,0xFF,0x42,0xF0,
        0xAE,0x00,0x86,0x79,0xF9,0x56,0xC0,0x35,
        0x1D,0x4A,0xD0,0x67,0x12,0x5F,0x17,0x70,
        0x53,0x64,0xA9,0x8E,0x0A,0xD0,0x53,0x4C,
        0x02,0x75,0x47,0xF7,0x51,0x01,0xC6,0x4D,
        0xD9,0x07,0x54,0x76,0x5A,0x60,0x67,0x21,
        0x76,0x1D,0xC1,0x5D,0x49,0x18,0xCA,0xB3,
        0x81,0x2F,0x59,0xFC,0x70,0x00,0x03,0xDC,
        0xB3,0x38,0xC4,0x08,0xB1,0xD9,0x81,0xEB,
        0x75,0xD2,0x70,0x2F,0x44,0xEC,0xFF,0x7F,
        0x32,0x00,0xE3,0x51,0x1B,0x1C,0x27,0x9D,
        0xF0,0x91,0x9E,0x59,0xF8,0x49,0x19,0x30,
        0x71,0xF2,0x03,0xE3,0xC9,0x1A,0xC6,0x00,
        0xB8,0xBC,0x57,0x95,0x81,0xFC,0x43,0x90,
        0x20,0x18,0xD4,0x29,0x19,0x38,0x1C,0xC5,
        0x70,0xA7,0x64,0x78,0x50,0xF8,0xC3,0x00,
        0xE6,0x46,0xE8,0x7B,0x82,0xA1,0xDE,0x93,
        0x0E,0xE3,0x91,0xD0,0x04,0x3E,0x2D,0xC3,
        0xFA,0xFF,0x9F,0x96,0x81,0xD5,0xB1,0xDD,
        0x43,0xF6,0x59,0x01,0x77,0x76,0x80,0x3B,
        0x3D,0x7E,0x7A,0x00,0x9C,0x00,0x3D,0x3D,
        0x80,0xED,0xBC,0x01,0xF7,0x40,0x80,0x38,
        0xFE,0xA3,0x82,0x5F,0x59,0x28,0x1C,0x3F,
        0xB6,0xF3,0x63,0x09,0xEE,0x70,0xE0,0x23,
        0x83,0x0F,0x90,0xB8,0xA1,0xF8,0x50,0x81,
        0x3C,0x0B,0x80,0x62,0xF4,0x6C,0x04,0xEC,
        0x06,0xF3,0xD2,0x12,0xE5,0xFF,0xFF,0xDE,
        0xC0,0x4E,0x29,0xB8,0x83,0x00,0xF8,0x8E,
        0x01,0xE0,0x1D,0x0C,0x97,0x35,0x66,0x94,
        0x10,0x18,0x8D,0x19,0x77,0x08,0xE1,0x27,
        0x02,0xDC,0x98,0x3D,0x6E,0x8F,0x19,0x77,
        0x9C,0xE5,0xA3,0x7A,0xCA,0x08,0xE5,0x03,
        0x07,0x3B,0x67,0xBC,0x11,0xF0,0xA1,0x03,
        0x8F,0x03,0x0C,0xEE,0x48,0x01,0xC6,0xCB,
        0x01,0x1B,0x3B,0xB8,0x83,0x90,0x53,0x20,
        0x4B,0x87,0xD1,0xD8,0x71,0xB2,0x81,0x74,
        0x8C,0xF1,0x21,0xD7,0x63,0xC7,0x0D,0xD6,
        0x63,0xC7,0x1D,0x5F,0xB0,0xFF,0xFF,0xE3,
        0x0B,0x18,0xC6,0xC0,0xC5,0x0F,0x03,0x7D,
        0xF3,0xF3,0xE8,0x0C,0xEE,0x61,0xFB,0x04,
        0x13,0xE3,0xF9,0x25,0xC4,0x23,0xCC,0x8B,
        0x4B,0x84,0xA3,0x08,0xF2,0xE6,0x12,0xE7,
        0xD5,0x20,0xCC,0x63,0x4B,0x94,0x10,0x11,
        0x0E,0x26,0xCE,0x13,0x8C,0x11,0x0E,0x3C,
        0x8A,0x21,0x22,0x9C,0x40,0x88,0x93,0x3E,
        0xD9,0x20,0xE1,0x63,0x84,0x8D,0xF6,0x04,
        0xC3,0xC7,0xC2,0xCF,0x2B,0x1E,0x3C,0x3F,
        0xAD,0xF9,0x2E,0xE8,0xC9,0x9C,0xE3,0x43,
        0x96,0xA7,0xF6,0x38,0xE9,0xC3,0x2C,0x6E,
        0x50,0x0F,0x8E,0xEC,0xAE,0xE3,0xE3,0x35,
        0xF6,0x14,0xE4,0x21,0xF0,0x13,0x81,0x2F,
        0x88,0x9E,0xAC,0xEF,0x7A,0xEC,0x5E,0x66,
        0x8C,0xEA,0xA7,0x80,0x3A,0xA6,0x9C,0xC1,
        0x2B,0x04,0xBB,0xE7,0xF9,0x90,0xED,0xBB,
        0x24,0x1B,0x05,0xEE,0x90,0xE0,0x33,0x12,
        0x3F,0x55,0x78,0x18,0x1E,0x05,0x8C,0x19,
        0xBC,0x23,0x1C,0x5A,0x88,0x03,0x7E,0xDF,
        0x65,0x43,0x8D,0x71,0x7A,0x3E,0x7F,0xB0,
        0x41,0xC0,0x87,0x3A,0x54,0x0F,0xF3,0xA8,
        0x5E,0x0A,0x19,0xCE,0xD9,0xC1,0x1D,0x04,
        0xF6,0xF8,0xE1,0x41,0xF0,0x9B,0x25,0x1F,
        0x04,0x3B,0xDF,0xBC,0xC1,0x19,0xE4,0xFF,
        0x7F,0x0C,0xB0,0xCF,0x54,0x3E,0x9A,0x20,
        0x8E,0x80,0xE8,0xF3,0x87,0xC7,0xF0,0x26,
        0xC7,0x87,0x83,0x3D,0x7A,0xE0,0x4E,0x22,
        0x70,0x8F,0x5D,0x07,0xED,0x6B,0x9C,0x2F,
        0x5A,0x30,0xEE,0x7B,0xCF,0x22,0xE0,0xC7,
        0x78,0x6C,0x01,0xC7,0xA1,0x04,0xDC,0xC1,
        0x8E,0x6B,0x1C,0x42,0x51,0x60,0x74,0x28,
        0xC1,0xC5,0x00,0x12,0x8C,0x63,0x9C,0xD1,
        0xD0,0x97,0x48,0x1F,0xD2,0xE0,0x0C,0x1A,
        0xF6,0x3C,0x9F,0x50,0xB8,0x3D,0x01,0x8A,
        0x4E,0x28,0x20,0xC3,0x7D,0x06,0xC1,0x9E,
        0x10,0xF8,0x19,0x84,0xFD,0xFF,0x0F,0x8E,
        0x1E,0xF7,0x7B,0xA3,0x4F,0x8D,0x6C,0xEE,
        0x0F,0x01,0x27,0x70,0xEE,0xEC,0xD4,0x8C,
        0x3B,0x33,0x60,0xCF,0x1F,0x1E,0x02,0x3F,
        0x17,0x78,0xF8,0x1E,0x02,0x7E,0xF0,0x0F,
        0xCC,0x06,0x07,0xE3,0x29,0xC2,0xD7,0x0E,
        0x0E,0xCE,0x4F,0x03,0x06,0xE7,0xAF,0x50,
        0x9F,0xE7,0x19,0x38,0xF6,0xD4,0xEB,0x7B,
        0x87,0xE7,0xEB,0x43,0x05,0xFE,0xA6,0xE7,
        0x43,0x05,0x38,0x0E,0x0F,0xFC,0xB0,0xC2,
        0x86,0xF0,0x28,0x80,0x3F,0xB5,0xF8,0xF8,
        0x17,0xE7,0x29,0x82,0xDD,0x46,0xB0,0x87,
        0x0B,0xC0,0x51,0xB4,0xB3,0x18,0x2A,0xCC,
        0x59,0x8C,0xFC,0xFF,0xCF,0x51,0xA8,0xB3,
        0x18,0x3D,0x5C,0x00,0x2E,0x04,0x1F,0x0F,
        0x40,0x73,0x10,0x78,0x5C,0xF0,0x85,0xE0,
        0x48,0x0E,0xE4,0xE9,0x00,0xF0,0x19,0x4A,
        0xC3,0xA1,0x09,0x13,0x03,0x06,0x75,0x3E,
        0xF0,0x09,0xC5,0xC7,0x0E,0x7E,0x36,0xF0,
        0x8D,0xDC,0x43,0xE5,0xA7,0x66,0x5F,0xF2,
        0x11,0xE0,0x02,0x75,0xA0,0x61,0xA0,0x46,
        0xE4,0x23,0xD2,0xFF,0xFF,0xB9,0x0D,0x1B,
        0x60,0x68,0xF4,0x1C,0x0E,0xE3,0x80,0xEB,
        0x73,0x38,0x76,0x40,0x3E,0x87,0xC3,0x3F,
        0x47,0xC3,0x1F,0x1B,0x3B,0xDD,0xF3,0x81,
        0xC1,0xBA,0x7E,0x63,0x06,0x06,0xB6,0x6F,
        0x91,0x07,0x06,0x1C,0x51,0xCF,0xC6,0x57,
        0x08,0x0F,0x0C,0x6C,0x80,0x1E,0x18,0xF0,
        0x89,0x05,0x21,0x27,0x03,0x43,0x9D,0x32,
        0x8C,0x1C,0xF3,0x89,0xC3,0xC3,0xF0,0xA1,
        0x22,0xEA,0x33,0xC0,0x23,0x1E,0x1B,0x1B,
        0xFB,0xFF,0x8F,0x0D,0x2C,0xC7,0x16,0x8F,
        0x0D,0xFC,0x47,0x78,0xFC,0xD8,0xE0,0x8C,
        0xE5,0xD1,0xC4,0x97,0x99,0x23,0x3B,0x8D,
        0x33,0x7B,0x0D,0xF1,0xD1,0xEE,0xF1,0xDB,
        0x63,0x03,0x97,0x85,0xB1,0x01,0xA5,0x90,
        0x63,0x43,0x1F,0x52,0x7C,0x0A,0xB0,0x71,
        0x54,0x32,0x0F,0x1F,0xAF,0x7C,0x62,0x38,
        0xBA,0x20,0x6F,0xE8,0xBE,0x5C,0xF8,0x48,
        0x63,0x30,0x5F,0x5A,0x7C,0x06,0xE5,0x43,
        0x04,0xD7,0x57,0xC5,0x43,0x04,0x3E,0xA1,
        0x86,0x88,0x1E,0xCF,0xFF,0xFF,0x11,0xCC,
        0x43,0x64,0x43,0x03,0xAF,0x87,0xA1,0x01,
        0xA5,0x98,0xC0,0x5E,0x85,0x87,0x46,0x4F,
        0x3F,0x3E,0x04,0x30,0x08,0xDF,0x06,0xD8,
        0x55,0xC0,0x57,0x21,0x83,0x24,0x18,0xE7,
        0x64,0x41,0x07,0x07,0x8E,0x21,0x79,0x70,
        0xF0,0x07,0xE3,0x21,0x70,0x60,0xCF,0xE0,
        0xB9,0xE8,0x31,0xD8,0xA7,0x1D,0x9F,0x4A,
        0xC0,0x77,0xE6,0x04,0xC7,0xE9,0x1D,0x7B,
        0x29,0xF0,0x08,0x1E,0xAD,0x3C,0x02,0x7E,
        0xB4,0x02,0x66,0xFF,0xFF,0xA3,0x15,0x30,
        0x09,0x7A,0xE6,0xA4,0x03,0x77,0x34,0x18,
        0xD4,0xD1,0x0A,0x5C,0x11,0xC0,0x75,0xDC,
        0xF0,0xD1,0x02,0xCE,0x50,0x0F,0xDA,0x07,
        0x65,0xCF,0xDA,0x97,0x21,0x76,0xB4,0x00,
        0x97,0x89,0x43,0x08,0xD0,0x04,0x3E,0x89,
        0x67,0xEF,0x43,0x03,0xB3,0x8A,0xA1,0x01,
        0xA5,0xA3,0x01,0xEE,0x44,0x81,0xFD,0xFF,
        0x9F,0x28,0x60,0xDE,0x30,0x70,0x07,0x0A,
        0xC0,0xCD,0xE9,0xDB,0xE3,0xE2,0xD0,0x38,
        0xC4,0xE7,0xA7,0x73,0xF6,0xD1,0xE8,0x4C,
        0x71,0x67,0x11,0x30,0x9C,0x7D,0x11,0x8F,
        0x18,0x03,0xF9,0x81,0x21,0x59,0x30,0x28,
        0x16,0x0F,0xC5,0x07,0x03,0x0E,0xEC,0x23,
        0x02,0x3B,0x17,0xB0,0x73,0xAD,0xE1,0xF8,
        0x59,0xC0,0xA7,0x84,0xB7,0xA6,0x17,0x7B,
        0x9F,0xD7,0x7D,0xD6,0x08,0xC9,0xCE,0xF4,
        0x3E,0x89,0xE2,0x0E,0xA2,0x70,0x4E,0x9F,
        0xE0,0x22,0xF0,0x65,0xDF,0xA3,0xE0,0xA7,
        0x07,0xCF,0xF1,0x8D,0xC1,0xA7,0x07,0xE6,
        0x7E,0xF8,0x9A,0xF1,0x33,0xC3,0xE3,0x43,
        0x88,0x27,0xE2,0xDA,0xA6,0x20,0x5B,0x18,
        0x42,0x09,0xF4,0xFF,0x8F,0x10,0xE5,0x6D,
        0x20,0xCA,0x29,0x44,0x88,0x12,0xA4,0xB1,
        0xC9,0x0B,0x35,0xCA,0xD9,0x45,0x6E,0x6D,
        0xF6,0x82,0x0B,0x14,0x2A,0x66,0x9C,0x28,
        0xEF,0x10,0xB1,0xDA,0x1F,0x04,0x91,0xF4,
        0x32,0xD0,0x71,0xC9,0x91,0x0E,0x7D,0xE8,
        0x61,0xFB,0x04,0x8C,0x3F,0x48,0xE2,0xAE,
        0x2A,0x3E,0x28,0xF8,0x00,0x80,0x77,0x09,
        0xA8,0x5B,0x9D,0xC7,0xED,0xF3,0x06,0xF8,
        0xAF,0x17,0x58,0x82,0xF2,0x07,0x81,0x1A,
        0x99,0xA1,0x3D,0xCC,0xB7,0x19,0x43,0xBE,
        0x07,0x1C,0x16,0x3B,0x27,0xF9,0xF0,0x08,
        0x1C,0x8E,0x01,0x4F,0x1B,0xBE,0x51,0x7B,
        0xBE,0x3E,0x62,0x01,0x8E,0xFE,0xFF,0x47,
        0x2C,0x30,0x9D,0xDF,0x7D,0x82,0x01,0xC7,
        0xCD,0x82,0x9F,0x61,0x00,0x67,0x40,0xCF,
        0x30,0x60,0x1F,0x2A,0x6E,0x08,0x5C,0xEE,
        0x8A,0x28,0x90,0x05,0xC2,0xA0,0x0E,0xFD,
        0xE4,0x08,0x42,0xCF,0x9C,0x70,0x86,0x72,
        0xB2,0xBD,0x5F,0x1D,0xC8,0x2D,0xC2,0x43,
        0x3D,0x8B,0xC7,0x04,0x76,0xDA,0x02,0x36,
        0xFF,0xFF,0xE3,0x29,0xB0,0x98,0xF7,0xD3,
        0x69,0x84,0x63,0x03,0xFB,0x71,0x0B,0x38,
        0x1D,0xCC,0xE0,0xDC,0x7F,0xD8,0x2D,0x1A,
        0x37,0x34,0xB0,0x0D,0xCC,0x43,0x03,0x3E,
        0x27,0x47,0x30,0x9E,0x98,0xF8,0x55,0xE2,
        0xE1,0x89,0x1F,0x43,0xC0,0xFA,0xFF,0x3F,
        0x99,0x01,0xF6,0x84,0x1E,0xCB,0x50,0xD2,
        0x4E,0x66,0x80,0xC0,0xFB,0xD8,0x3B,0xC3,
        0x4B,0x83,0xE7,0x74,0xD2,0xCF,0x62,0x3E,
        0x99,0x19,0x21,0x0A,0xBB,0x8F,0x19,0xAD,
        0x37,0x14,0xCD,0x3C,0xE8,0x3B,0x99,0x51,
        0x62,0x46,0x6A,0x0E,0x4C,0x48,0x11,0x0F,
        0x27,0x4A,0x88,0x60,0xAF,0x13,0x6F,0x67,
        0x4F,0x66,0x4C,0xD6,0xC9,0x0C,0x24,0xFF,
        0xFF,0x93,0x19,0x98,0x5C,0x9F,0xCC,0x80,
        0xCA,0x39,0x0A,0x7F,0x32,0x03,0x78,0x74,
        0xC0,0xC2,0x9D,0xCC,0xC0,0xF2,0xFF,0x3F,
        0xC4,0x00,0xCE,0xC7,0x0A,0x63,0x0C,0x3C,
        0xDA,0xC1,0x0C,0x15,0xE6,0x6C,0x86,0x0E,
        0x72,0x08,0xA1,0xC1,0x0E,0x21,0x50,0xE6,
        0x72,0xA0,0xA7,0xF0,0x9A,0xE0,0x73,0x14,
        0xD8,0x0F,0x67,0xC0,0xE1,0xD4,0x80,0x0F,
        0x74,0xE2,0x42,0x8F,0xC2,0x23,0x0E,0x58,
        0xFD,0xC0,0xC8,0xFF,0xFF,0x64,0x06,0x18,
        0x78,0x6A,0xF8,0x40,0x82,0x63,0x31,0xEA,
        0x1B,0xC4,0x21,0xBE,0x8D,0xF8,0xE8,0xFE,
        0x6A,0xE2,0x4B,0x00,0xE6,0x42,0xE2,0xD3,
        0x09,0xB3,0x70,0x38,0x03,0x5A,0x43,0x60,
        0x57,0x26,0xCF,0x9C,0x0F,0xE1,0x6C,0x3C,
        0x7A,0xDC,0xE9,0x04,0xDE,0x38,0x7C,0x3A,
        0x01,0x5E,0x07,0x0C,0xCC,0x0C,0xC2,0x3F,
        0x84,0xB0,0x21,0x9C,0xAA,0xC7,0x70,0xEE,
        0xAF,0x38,0x3E,0x9D,0x80,0xF3,0xFF,0x7F,
        0x62,0x03,0x0C,0x0A,0x7E,0x32,0xF8,0xB8,
        0x46,0x25,0xC2,0xA0,0x8E,0xE6,0x80,0x7B,
        0x98,0x27,0x36,0x26,0x6F,0xC5,0x1A,0x8B,
        0x4F,0x6C,0x30,0xFF,0xFF,0x27,0x36,0x80,
        0xD1,0x87,0x20,0xB0,0xFD,0xFF,0x0F,0x41,
        0x60,0x1C,0xA0,0x0F,0x41,0x80,0x9B,0xD3,
        0x09,0xEE,0xC4,0x07,0xB6,0x63,0x10,0x60,
        0x6D,0xE8,0x3E,0x06,0x81,0xF9,0xFF,0x3F,
        0x5A,0x98,0xA3,0xE0,0xC2,0x8E,0x7C,0x28,
        0x29,0xA7,0x3E,0xB4,0x0C,0x20,0x69,0x38,
        0xC9,0x01,0x9D,0xD3,0x3D,0x70,0x92,0x75,
        0xEA,0x40,0x8F,0xC7,0xA0,0xAF,0x1C,0xBE,
        0x12,0xF0,0x23,0x07,0x93,0x00,0xAA,0x41,
        0xFA,0xCC,0x07,0x9C,0x8E,0x1C,0xE0,0x38,
        0x26,0x05,0xC6,0xDE,0x0E,0xDE,0x22,0x3D,
        0x89,0xA7,0xA1,0xE3,0x0C,0x51,0x38,0x26,
        0x39,0x18,0x44,0x7A,0x95,0x62,0x03,0x7C,
        0xAB,0xF1,0xD9,0xC8,0x07,0x10,0x78,0xE3,
        0xF6,0xD8,0x61,0xFF,0xFF,0x0F,0x75,0xC0,
        0x01,0xE2,0xA4,0xF8,0x21,0xC3,0x98,0x67,
        0xC5,0x0F,0x75,0x80,0xF5,0x18,0x27,0x3A,
        0x94,0xF0,0x43,0x1D,0x20,0xE8,0xFF,0x7F,
        0xA8,0x03,0x86,0x38,0x6F,0x24,0xD1,0x1E,
        0xEA,0x98,0xE8,0x43,0x1D,0x40,0xC8,0xFF,
        0xFF,0xA1,0x0E,0x18,0x9E,0x87,0x00,0xAE,
        0x9C,0xEF,0xC0,0x7C,0x22,0x02,0xEF,0xFF,
        0xFF,0x7C,0x07,0xB8,0x1B,0x2D,0xCC,0x51,
        0x70,0x41,0xAF,0x0E,0x03,0x51,0x09,0x30,
        0x28,0x02,0xC7,0x5F,0x9B,0x60,0x1C,0xEA,
        0x7C,0x87,0x3E,0x2F,0x78,0xD8,0x4F,0x05,
        0x9E,0xC4,0xA9,0xFA,0x5A,0x70,0x14,0x4F,
        0x00,0x3E,0xE1,0x01,0xFF,0xA1,0xC1,0x9A,
        0x44,0xF1,0x43,0x03,0xF5,0x11,0xE4,0xFF,
        0x7F,0x68,0xC0,0x28,0xEA,0xF9,0x06,0x7D,
        0xCC,0xF2,0xD9,0x20,0xE6,0x0B,0x48,0x84,
        0x07,0x10,0x5F,0x1F,0xD8,0x71,0xD2,0x67,
        0xA0,0x40,0x51,0xDE,0x37,0xF8,0x09,0x07,
        0x5C,0x83,0xF3,0x09,0x07,0xBC,0x87,0x23,
        0x1F,0x4B,0xC0,0x77,0xD0,0x84,0x73,0x81,
        0xF1,0x8D,0x8D,0x9D,0x06,0xC0,0x76,0x00,
        0x06,0xDF,0x69,0x00,0x1C,0xC7,0x24,0x7E,
        0x3A,0x04,0x13,0xCC,0xC1,0xBC,0x34,0xFB,
        0xFF,0xEF,0xFD,0x94,0x43,0xCF,0x86,0x80,
        0x75,0x49,0x07,0x43,0x94,0x88,0xB3,0x21,
        0x20,0xFD,0xFF,0x7F,0x36,0xC4,0x20,0xC4,
        0x09,0xFC,0x12,0xD1,0xDC,0xD9,0x90,0xAE,
        0xD8,0x67,0x43,0x80,0xE1,0xFF,0xFF,0x23,
        0x00,0xF6,0x7C,0x04,0x38,0x3D,0x64,0x83,
        0xE7,0x14,0x08,0xE3,0xE4,0x03,0x38,0xFE,
        0xFF,0x8F,0x15,0xE6,0x18,0x78,0xEA,0x97,
        0x9B,0x8F,0x03,0x54,0xD4,0x2B,0xC2,0x30,
        0x94,0xC5,0x87,0x05,0x1F,0x11,0xF8,0x61,
        0xC1,0x23,0xA8,0x78,0x9C,0xF4,0x74,0xE3,
        0x33,0x21,0x3B,0x24,0x38,0xFC,0x20,0xE9,
        0x41,0x13,0x3C,0xE7,0x23,0x78,0xB7,0x1E,
        0x38,0xA7,0x02,0xC0,0x4D,0xAE,0x27,0xA3,
        0x4E,0x17,0x0E,0x70,0x8E,0x92,0x8D,0x63,
        0x08,0xE5,0x70,0xCC,0xB7,0x87,0xA6,0xC9,
        0x4E,0x56,0x30,0x63,0x41,0xEA,0x24,0xE0,
        0x01,0x38,0x10,0x8C,0xB4,0x93,0x68,0x34,
        0x86,0xB3,0x5A,0x18,0xC1,0x19,0xC4,0xC7,
        0x11,0xE7,0x3A,0x19,0xA1,0x3F,0x07,0x3E,
        0x15,0x61,0x82,0xDC,0x4B,0xE8,0xBC,0x7D,
        0x37,0xE0,0x57,0x61,0x8F,0xC5,0xFF,0x7F,
        0x60,0xDF,0x4E,0xC0,0x31,0x17,0xAB,0x01,
        0x45,0x0D,0xC0,0x68,0x98,0x53,0xC0,0x53,
        0x09,0xB8,0x82,0xCD,0x0D,0x7D,0x61,0xB1,
        0xD6,0xA9,0xE8,0x14,0xF4,0x3E,0x70,0x70,
        0xC0,0x63,0xF6,0x1E,0x1C,0x2C,0x34,0x0F,
        0x0E,0x6C,0xD9,0x06,0x87,0x56,0x72,0x17,
        0x21,0x87,0x0F,0xFC,0xEC,0x80,0x03,0xA0,
        0x67,0x07,0x0B,0xC9,0xB3,0x03,0x9B,0xBE,
        0xB3,0x08,0x28,0x70,0xFE,0xFF,0x11,0xDE,
        0x3B,0x7C,0x6E,0x79,0xF6,0x60,0x63,0x78,
        0x74,0x31,0x9A,0xD1,0xB9,0xA6,0xDB,0x04,
        0x4A,0xC5,0x6D,0x82,0x82,0xF8,0x06,0xE0,
        0x84,0x34,0xBA,0x75,0xE2,0x66,0x62,0xFC,
        0x47,0x0C,0x1F,0x11,0x0E,0xE9,0x6C,0x4D,
        0x30,0x0F,0xA4,0x9E,0x81,0xBE,0xB3,0xE1,
        0x67,0x1F,0xF2,0xC1,0xC5,0xD3,0xF0,0xF5,
        0x86,0xDC,0x3B,0xE8,0xB4,0x7D,0x66,0xC0,
        0x1C,0x74,0x7D,0x9D,0x7A,0x83,0x27,0x57,
        0x09,0xEA,0xE1,0x02,0x42,0x2F,0x34,0xBE,
        0xDC,0x25,0x78,0xE0,0xF4,0xE9,0xEE,0xBD,
        0x84,0x9D,0xF1,0x12,0xBC,0xE0,0x25,0x98,
        0x77,0x10,0xA8,0x51,0x79,0x10,0x98,0xAB,
        0x3C,0xCB,0x37,0x06,0x54,0xB2,0x8B,0x16,
        0x3D,0xC3,0xBC,0xC3,0xF8,0x92,0xE0,0xEB,
        0x87,0xCF,0x2D,0x5E,0xC0,0xEB,0x16,0x0C,
        0x82,0x67,0xA0,0x57,0x17,0xDF,0xD9,0x0D,
        0xFC,0x2A,0xF0,0x46,0x13,0x22,0x98,0x61,
        0x0F,0xFF,0xDD,0xDD,0xA8,0xBE,0xE9,0x18,
        0xEB,0x75,0xC4,0x23,0xE5,0xC7,0x96,0x03,
        0x8A,0xF4,0xF2,0xE6,0x09,0xF8,0x2C,0xE3,
        0x53,0xDD,0x49,0xF9,0x7A,0x68,0xF4,0x57,
        0x08,0x1F,0x7E,0x8C,0xEC,0x73,0x0E,0x3B,
        0xDF,0xB1,0x41,0x71,0xC4,0x07,0x86,0x97,
        0x1A,0x4F,0x85,0x9D,0xBB,0x60,0x1C,0x1C,
        0xD8,0xB1,0x08,0x73,0x7C,0x05,0xD7,0xC9,
        0xE6,0xFF,0xFF,0xE4,0x00,0x6E,0x78,0xCC,
        0xC1,0xD7,0xE7,0x0D,0xDF,0x0C,0x3C,0x2E,
        0x7E,0xE4,0xF0,0x49,0xE3,0xA5,0xD3,0xD8,
        0xA7,0xE9,0xA3,0xD1,0xCB,0x9B,0x4F,0x2F,
        0x18,0x58,0x5F,0x1A,0x38,0xAC,0xD1,0xC2,
        0x3E,0x06,0x9C,0xB9,0x2F,0x44,0xB8,0xC3,
        0x23,0x58,0x00,0xF1,0xB7,0x92,0x47,0x0E,
        0x4F,0xC0,0x80,0x4C,0xD3,0xBA,0x74,0x20,
        0xE2,0xA7,0x3C,0x2B,0x5F,0x99,0x2E,0x43,
        0x0C,0xE3,0xA9,0xF2,0xF1,0xC3,0xB3,0xF1,
        0x51,0xC0,0xC7,0x28,0xCF,0xFC,0x8C,0x22,
        0xBD,0x32,0x10,0x50,0x9D,0x88,0xB8,0x42,
        0x18,0x89,0xA1,0xD1,0x9D,0x83,0xC7,0x1F,
        0x22,0x05,0x31,0xA0,0x6F,0x2E,0xC0,0xF4,
        0x4C,0x04,0x5C,0xFE,0xFF,0x37,0x17,0x80,
        0xFF,0xFF,0xFF,0x9B,0x0B,0xE0,0xE6,0xFE,
        0xE0,0x9B,0x0B,0x70,0x8D,0xB4,0x2A,0x7A,
        0x61,0x77,0x08,0x18,0xD4,0x9D,0x1D,0x70,
        0x78,0x2B,0x78,0x67,0x87,0xF5,0xFF,0xBF,
        0xB3,0xC3,0xC3,0x8C,0x13,0xE5,0x85,0x21,
        0xC6,0x3B,0x3B,0x0B,0xF0,0x26,0xD0,0x51,
        0xC6,0x77,0x76,0x80,0x1F,0x67,0xD8,0x77,
        0x69,0xF0,0x5E,0x75,0x81,0xF5,0xFF,0xFF,
        0xAA,0x0B,0x3C,0x04,0xDF,0xA7,0x41,0x3E,
        0x5E,0x30,0x8C,0x83,0x2B,0x27,0xA1,0xC7,
        0x02,0x6B,0x85,0x41,0xDD,0xA9,0xC1,0xA5,
        0x09,0x5C,0x17,0x5F,0x1F,0x6A,0x7C,0xA4,
        0xC5,0x9F,0x2F,0x70,0x01,0x86,0x4C,0x4F,
        0x65,0x30,0xAE,0x29,0x3E,0x95,0x61,0xEE,
        0x0E,0x1E,0x90,0x8F,0x18,0xC0,0x67,0x15,
        0x1E,0x18,0xEE,0xB4,0xE0,0x9B,0x92,0x41,
        0xCF,0x31,0xA8,0x8F,0x3C,0x27,0xEF,0x7B,
        0xC2,0xE3,0x84,0xA3,0x9E,0x83,0xE8,0xD8,
        0xC0,0x71,0xDC,0xC0,0xFD,0xFF,0xC7,0x06,
        0xEF,0x70,0x83,0x3B,0xE8,0xF8,0x62,0x70,
        0x5C,0x18,0xB8,0xE7,0x02,0x0F,0xC3,0x37,
        0x1D,0x8F,0x08,0x33,0xFE,0xD7,0x3F,0x23,
        0x04,0xC4,0x5F,0x8C,0xD8,0x80,0xC1,0x78,
        0x6B,0xF3,0xF5,0x0D,0x37,0x60,0x5F,0x1D,
        0x7C,0xC1,0xF0,0x09,0xCC,0xE8,0x2F,0x30,
        0x4F,0x62,0x3E,0x36,0x90,0x0B,0x1C,0x1D,
        0x30,0x38,0x00,0x3D,0x60,0xF8,0x87,0x8B,
        0x77,0x39,0x30,0x5C,0x05,0x7D,0x5C,0xF0,
        0xB1,0xC7,0x8A,0xEE,0x72,0xE8,0x9B,0x9C,
        0x61,0xE2,0x18,0xE2,0x0D,0x8C,0xDD,0x25,
        0xC8,0x61,0x0E,0xEA,0x5D,0xC2,0x73,0xE0,
        0x67,0x0B,0x9F,0xE0,0x7C,0xF3,0x09,0x71,
        0xAA,0x8F,0x56,0xEF,0x01,0x3E,0x7A,0xBC,
        0x77,0xF9,0xEC,0xC4,0x2E,0x02,0x3E,0x72,
        0x19,0xC7,0xD3,0xF4,0x15,0xD0,0x43,0x36,
        0xD8,0xAB,0x86,0x4F,0x60,0x3E,0xBA,0xE1,
        0x8E,0x51,0x9E,0x89,0xA7,0xEF,0x3B,0x08,
        0x3B,0x92,0x1C,0x75,0xA8,0x6B,0x7A,0x44,
        0xF9,0xFF,0x9F,0xD0,0x81,0xF8,0xD6,0x06,
        0xCE,0x68,0xF7,0x0F,0xF4,0x36,0x3D,0x32,
        0xCC,0xD1,0x00,0xD6,0x25,0x04,0x5C,0x77,
        0x0C,0x5F,0x42,0x80,0x4F,0xD0,0x4B,0x04,
        0xFA,0x9A,0xE1,0xD1,0x3D,0x02,0x60,0xAE,
        0x18,0xEC,0x58,0xE0,0xC3,0x86,0xAF,0x01,
        0xEC,0x5E,0xE0,0x30,0xF7,0x08,0x50,0x81,
        0x7A,0x78,0xF0,0xD5,0xDE,0x23,0x40,0x71,
        0xB2,0xF4,0xA1,0xC1,0x03,0xB5,0xAA,0x33,
        0x26,0x94,0x23,0x26,0x3F,0x9B,0xF9,0x26,
        0x81,0xB9,0x5D,0xFA,0x26,0x01,0x37,0xCF,
        0x2C,0x50,0x49,0x20,0xF4,0xFF,0xBF,0x49,
        0xC0,0x85,0xE9,0xF2,0x32,0x43,0xE7,0x7F,
        0xE0,0xBE,0xD5,0x79,0x84,0x3E,0x44,0x30,
        0x94,0xF7,0x3C,0x9F,0xC2,0xF8,0x19,0xC2,
        0x07,0x4C,0x76,0xA6,0xE0,0x67,0x4D,0xDC,
        0x1D,0xC0,0x28,0x6F,0x9E,0x9E,0x00,0x3B,
        0x7F,0x1A,0xF9,0xDD,0xE0,0x5D,0xC0,0xD3,
        0xF7,0xBD,0x88,0x9F,0x28,0xC0,0x17,0xEC,
        0x4E,0x07,0x05,0xFA,0x84,0x3C,0x22,0xA3,
        0xFA,0x88,0xC0,0x2F,0x49,0x60,0x3C,0x92,
        0xF8,0x40,0x01,0x84,0xEE,0x05,0xA8,0xD3,
        0x07,0x47,0x3D,0xE3,0x17,0x54,0x63,0xBE,
        0x5B,0x3D,0xC2,0x79,0x72,0x98,0xCB,0x01,
        0x8B,0x73,0x4D,0x02,0xD5,0x71,0x97,0x8F,
        0x0E,0xEE,0xB5,0x15,0xFB,0xFF,0x27,0x38,
        0xB8,0x77,0x96,0x77,0x3E,0x43,0x79,0x90,
        0xE0,0xBB,0xB6,0x82,0xE3,0xAA,0x06,0xE3,
        0xD8,0xC2,0x2F,0x79,0x80,0x9D,0x61,0x71,
        0xC1,0x7F,0x0F,0x03,0x51,0x89,0x30,0x28,
        0x02,0xCB,0xBB,0xB7,0x52,0xF8,0x43,0x06,
        0xE3,0x4D,0x81,0x4F,0x1A,0x3B,0x6A,0xE0,
        0xFB,0xFF,0x1F,0x35,0xD8,0x86,0x8A,0xBB,
        0x29,0x82,0x75,0xAA,0x98,0x21,0xF0,0x60,
        0x0F,0x00,0x9F,0xAF,0x7C,0x06,0x50,0x14,
        0x18,0xD4,0xA1,0x1D,0xCE,0x6D,0x18,0x70,
        0x30,0x62,0xDC,0xA5,0x10,0xEE,0x94,0xDF,
        0x51,0x62,0x3F,0x97,0xB3,0xE9,0xE2,0xAE,
        0xE6,0x3E,0x9D,0xB0,0x0B,0x32,0x8C,0xB3,
        0xC0,0x23,0xC0,0xAB,0x39,0xBF,0x20,0x3F,
        0x17,0xBF,0x10,0x3C,0x26,0x85,0x78,0x53,
        0x7A,0x25,0x36,0xC6,0x93,0x71,0x73,0xB7,
        0x62,0x72,0xDE,0x79,0x41,0x36,0xC6,0xD1,
        0x44,0x8C,0x72,0x6E,0x0F,0x03,0x91,0x5F,
        0x90,0x7D,0x3F,0x79,0x21,0x88,0x18,0xCD,
        0x10,0x41,0x9F,0x97,0x8D,0x15,0x28,0xDE,
        0x0B,0x32,0x13,0xF8,0x56,0xD0,0xC1,0xC5,
        0x17,0x64,0xEC,0xFF,0xFF,0x82,0x0C,0x30,
        0xE2,0x64,0x04,0xF8,0x3C,0x71,0xE0,0xCE,
        0x35,0x30,0xFE,0xFF,0x97,0x6A,0xD8,0x27,
        0x1B,0xC0,0xD9,0xD0,0x7D,0xB2,0x01,0xF7,
        0x68,0xE1,0x1D,0x4D,0x10,0x27,0x1B,0x0A,
        0xE4,0xE0,0xEB,0xA2,0x70,0x3C,0xF4,0x49,
        0x84,0x1E,0x9D,0x7C,0x94,0xC4,0x9D,0x19,
        0x3C,0x91,0x77,0x16,0x8F,0xE2,0x65,0xD0,
        0xF7,0x82,0x13,0x79,0x7D,0xB0,0x9C,0x63,
        0x24,0xA8,0x46,0xE2,0xE3,0x03,0xFC,0xEB,
        0x8B,0x8F,0x91,0xF0,0xF9,0xFC,0xC3,0xF2,
        0x60,0x0C,0xF9,0xFF,0x7F,0x8A,0xC4,0x80,
        0x3C,0xBB,0x3C,0x86,0xF0,0x0B,0x24,0xDC,
        0xD3,0xCC,0x01,0x60,0x64,0x5D,0x1E,0xD1,
        0x67,0x47,0x8E,0x11,0xD7,0x17,0x45,0x5F,
        0x81,0x7D,0x10,0x38,0x9F,0xE7,0x44,0xB0,
        0x8E,0x9A,0x1F,0x6D,0xF8,0xF8,0x39,0xF8,
        0x5B,0xC1,0x03,0xA5,0x8F,0x45,0x21,0x1E,
        0x91,0xF8,0x39,0x11,0x5C,0x26,0xCE,0x89,
        0x40,0xE2,0xD0,0x0B,0xE3,0xB4,0x80,0x1B,
        0x88,0xCF,0x94,0xD8,0x29,0x9F,0x08,0x3B,
        0x97,0x60,0x46,0x07,0xAE,0xCB,0xBD,0x47,
        0x07,0xFE,0x93,0x00,0x1E,0xEB,0xFF,0xFF,
        0x78,0x07,0xBE,0x93,0xBA,0xEF,0x26,0xBE,
        0xC8,0xF8,0x50,0xF4,0x7C,0x07,0xF8,0x0F,
        0x77,0xB8,0x43,0xC5,0x39,0xDF,0x01,0xD2,
        0xFE,0xFF,0xE7,0x3B,0x60,0x79,0xB6,0x7E,
        0xBE,0x03,0xBB,0xC8,0xF3,0x1D,0x40,0xAC,
        0xFF,0xFF,0xF9,0x0E,0xB0,0x73,0x46,0xC3,
        0x9D,0xEF,0xC0,0x76,0xB4,0x01,0xCC,0x4D,
        0xE3,0xD1,0x06,0xDC,0xC3,0x85,0x3D,0x0C,
        0xAE,0xD0,0xA6,0x4F,0x8D,0x46,0xAD,0x1A,
        0x94,0xA9,0x51,0xE6,0xFF,0xDF,0xA0,0x56,
        0x9F,0x4A,0x8D,0x19,0xCB,0x0E,0xA5,0x80,
        0x8F,0x0A,0x8D,0xCD,0xF2,0x28,0x04,0x62,
        0x31,0xAF,0x06,0x81,0x38,0x2C,0x08,0x8D,
        0xF4,0xCA,0x11,0x88,0x25,0x3F,0xFB,0x05,
        0x62,0xB9,0x6F,0x06,0x81,0x38,0xE0,0x1B,
        0x4C,0xE0,0xE4,0x61,0x25,0x70,0xF2,0x6E,
        0x10,0x88,0x23,0x83,0x50,0xA1,0x3A,0x40,
        0x58,0x4C,0x10,0x1A,0xCA,0x07,0x08,0x93,
        0xFE,0x48,0x10,0x20,0x31,0x02,0xC2,0xC2,
        0xBD,0xBF,0x04,0x62,0x69,0xEF,0x09,0x81,
        0x58,0x88,0x15,0x10,0x16,0x17,0x84,0x86,
        0xD3,0x02,0xC2,0x24,0x99,0x01,0x61,0x81,
        0x40,0xA8,0x7C,0x35,0x20,0x4C,0xA4,0x1B,
        0x40,0xBA,0x7A,0x81,0x38,0x88,0x1E,0x10,
        0x26,0xC3,0x0F,0x08,0x0B,0x0D,0x42,0xA3,
        0x3D,0x30,0x04,0x48,0x0C,0x81,0xB0,0xF8,
        0x8E,0x40,0x98,0xF8,0x57,0x91,0x40,0x9C,
        0xDF,0x12,0xC4,0x4D,0x69,0x88,0x35,0x01,
        0x31,0x0D,0x9E,0x80,0x98,0x22,0x10,0x01,
        0x39,0xF6,0xD3,0x43,0x40,0xD6,0x60,0x0A,
        0x88,0x45,0x07,0x11,0x90,0x85,0xA8,0x02,
        0x62,0x79,0x5D,0x01,0xB1,0xF0,0x20,0x02,
        0x72,0xE6,0x97,0x9F,0x80,0xAC,0xE0,0xA5,
        0xF3,0x10,0xC0,0xDE,0x10,0x81,0x48,0x72,
        0x10,0x01,0x39,0xB0,0x2F,0x20,0x16,0x1F,
        0x44,0x40,0xCE,0xFA,0x28,0x14,0x90,0x83,
        0x83,0x68,0x10,0xE4,0x6B,0x26,0x20,0xA7,
        0x07,0x11,0x10,0xF9,0x04,0x05,0x21,0x6A,
        0xBD,0x81,0x30,0x3D,0x8F,0x42,0x0D,0x85,
        0x80,0x50,0xE5,0xEA,0xCE,0x31,0x2C,0x07,
        0x08,0xCD,0x05,0x22,0x30,0xAB,0x70,0x07,
        0xC4,0x54,0x81,0x08,0xC8,0x09,0x80,0xC8,
        0xFF,0x9F,0x60,0x2A,0x10,0x9A,0x12,0x8C,
        0xEA,0x92,0x07,0xC4,0x12,0x80,0xD0,0x54,
        0x20,0x34,0x25,0x88,0x00,0xAD,0xCA,0x1E,
        0x10,0x53,0x0A,0x42,0x95,0x83,0xD0,0x74,
        0x20,0x54,0xB6,0xBE,0xC3,0x02,0x05,0x11,
        0x90,0xA3,0x83,0x50,0xE1,0xFE,0x40,0x98,
        0xDE,0x97,0x86,0x00,0x9D,0x0E,0x44,0x40,
        0x4E,0x0C,0x42,0x15,0x7C,0x32,0x82,0x10,
        0xB1,0x20,0x54,0xC1,0x27,0x23,0x28,0xD1,
        0xF2,0xB2,0x13,0x90,0xF5,0x81,0x50,0xBD,
        0x20,0x02,0x73,0x36,0x20,0x9A,0x17,0x84,
        0xE6,0x07,0xA3,0x5A,0x8D,0x02,0x31,0xFD,
        0x20,0x34,0x0F,0x88,0xC0,0xAC,0xE0,0xF9,
        0x71,0xC0,0x0C,0x84,0xAA,0x04,0x11,0x98,
        0x73,0x01,0xD1,0xAC,0x20,0x34,0x3B,0x18,
        0xD5,0xFE,0x0F,0xD1,0x00,0x08,0x08,0xCD,
        0x07,0xA2,0xC3,0x00,0x79,0x96,0x09,0xC8,
        0x1A,0x41,0xA8,0x66,0x10,0x81,0x39,0x27,
        0x10,0xCD,0x0E,0x42,0x95,0xFD,0x4D,0x82,
        0x91,0x8C,0x0F,0xD0,0x40,0x24,0x37,0x08,
        0xD5,0xF1,0x0C,0x0A,0x46,0x74,0x83,0x08,
        0xC8,0x59,0x40,0x68,0x36,0x30,0x9A,0x4C,
        0xED,0x91,0x80,0xBA,0x05,0x61,0xE9,0x41,
        0x68,0x3A,0xBB,0x83,0xA7,0x20,0x54,0x81,
        0x5E,0x30,0xA6,0x19,0x44,0x87,0x05,0x02,
        0x42,0x73,0x81,0x51,0x1D,0xAF,0x96,0x40,
        0x44,0x1B,0x08,0xD5,0x0A,0xA2,0x81,0x93,
        0x1F,0x53,0x10,0x92,0x14,0x84,0xFC,0xFF,
        0x07,0xAA,0xC7,0x9C,0x40,0xAC,0xFA,0x5B,
        0x25,0x50,0x27,0x01,0xA1,0xC9,0x40,0x74,
        0x7C,0x20,0x0F,0xB8,0x83,0x64,0x20,0x54,
        0x29,0x88,0xC0,0xAC,0xF4,0x63,0xA4,0x23,
        0x05,0x51,0x7D,0xBC,0xA0,0x20,0x34,0xD1,
        0x3B,0x2C,0x08,0x7B,0xB8,0x69,0xA8,0xE4,
        0x59,0xA5,0xA1,0x12,0x10,0x9A,0x0D,0x44,
        0xC7,0x04,0xF2,0xAA,0x79,0x4C,0x60,0x20,
        0x54,0x2F,0x08,0xCD,0x01,0x42,0x13,0x83,
        0x08,0xD4,0xA9,0xBF,0x37,0x1A,0x2A,0xF9,
        0x5B,0x09,0xC4,0xCA,0x5E,0x69,0x02,0xB1,
        0xDE,0xA7,0x4E,0x20,0xE6,0x1D,0x98,0xA9,
        0x05,0xA1,0xEA,0x41,0x04,0xE6,0xB4,0x40,
        0x54,0x81,0x78,0x10,0xA6,0x08,0x44,0x60,
        0x4E,0x02,0x44,0xD3,0x81,0xD0,0xEC,0x60,
        0x54,0xE7,0xA3,0x4D,0x40,0xD6,0x0E,0x42,
        0xB3,0x80,0x08,0xCC,0x59,0x1E,0x69,0x02,
        0xB1,0x92,0x2F,0x9D,0x0E,0x24,0x04,0x84,
        0x26,0xD3,0x7F,0x68,0xA1,0x05,0x80,0x99,
        0x84,0x04,0x20,0x4C,0x16,0x88,0x0E,0x27,
        0xD6,0x08,0x22,0x40,0xC7,0x01,0xA3,0xD1,
        0x40,0x68,0x5C,0x40,0x9A,0x1D,0x90,0x2A,
        0x6D,0x00,0xC6,0x54,0x83,0xD0,0x24,0x20,
        0x02,0x74,0x2C,0x10,0x01,0x5A,0x74,0x04,
        0x30,0x16,0x01,0x84,0x46,0x05,0xA1,0xC9,
        0x2A,0x80,0xB2,0x9C,0x20,0x1A,0x20,0xC9,
        0x30,0x60,0x0A,0x42,0x33,0x81,0xD0,0x8C,
        0x20,0x54,0x7C,0x07,0x10,0x16,0x04,0x84,
        0x86,0x03,0xD1,0x00,0xFE,0xFF,0x8F,0x0C,
        0x02,0xD1,0x00,0x9C,0x23,0xC4,0x61,0x85,
        0x82,0xD0,0xF4,0x20,0x34,0x6C,0x09,0x50,
        0x16,0x1D,0x44,0xC7,0x23,0x92,0x02,0x8C,
        0x05,0x02,0xA1,0x31,0x41,0x68,0x6C,0x10,
        0x1A,0x29,0x06,0x28,0x13,0x54,0xE3,0x50,
        0x44,0x7B,0x80,0x31,0x99,0x20,0x54,0x36,
        0x88,0xC0,0x1C,0x14,0x88,0x86,0x07,0xA1,
        0x62,0x82,0x00,0x52,0x10,0x01,0x12,0x20,
        0x1A,0x1E,0x84,0x8A,0x29,0x32,0x74,0x0A,
        0x42,0x55,0x24,0x39,0x9A,0x50,0x10,0x1D,
        0x4D,0x08,0x08,0xCD,0x07,0x46,0x75,0x35,
        0x39,0x6E,0x50,0x10,0xAA,0x1D,0x84,0x06,
        0x05,0xA1,0x39,0xA2,0x80,0xB2,0xEC,0x20,
        0x02,0xB2,0x9E,0x2A,0x87,0x0A,0x0A,0x22,
        0x30,0xA7,0x02,0xA2,0x49,0x41,0xA8,0x8E,
        0x2C,0x47,0x0A,0x9A,0x06,0x84,0x25,0x06,
        0xA1,0xC9,0xDA,0x80,0xB0,0x0C,0x75,0x0E,
        0x24,0x14,0x84,0xE6,0x04,0xA1,0x4A,0xF2,
        0x0C,0x8F,0x82,0xE8,0x38,0x42,0x80,0x68,
        0x7A,0x10,0xAA,0xA6,0xCF,0x00,0x28,0x88,
        0x06,0x40,0x40,0x68,0x4E,0x30,0xAA,0xA8,
        0xD1,0xD1,0x84,0x82,0x50,0xDD,0x2F,0x4E,
        0x81,0xF8,0xFF,0x0F,
    })  // END MBUF

} //end DefinitionBlock
