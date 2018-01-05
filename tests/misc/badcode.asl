/*
 * badcode.asl
 *
 * This file contains examples of the extended error checking and
 * typechecking capabilities of the iASL compiler. Other ASL compilers
 * may ignore these errors completely. Note - this is not an exhaustive
 * list of errors detected by iASL, it shows many of the errors that
 * are not detected by other ASL compilers.
 *
 * To compile, use:
 * iasl badcode.asl
 *
 * Output:
 * Compilation complete. 45 Errors, 22 Warnings, 3 Remarks, 16 Optimizations
 *
 */
DefinitionBlock ("badcode.aml", "DSDT", 1, "Intel", "Example", 0x00000001)
{
    Name (INT1, 0)
    Name (BUF1, Buffer() {0,1,2,3})
    Event (EVT1)

    // Invalid SyncLevel in Mutex declaration

    Mutex (MTX1, 32)

    // Integer beyond the table integer size (32 bits)

    Name (BIG, 0x1234567887654321)

    // CPackage length does not match initializer list length

    Name (PKG1, Package(5) {0,1})

    // Inadvertent use of single backslash in a string

    Name (PATH, Buffer() {"\_SB_.PCI2._CRS"})

    // Invalid hex escape sequence

    Name (ESC1, "abcdefg\x00hijklmn")

    // Field access beyond region bounds

    OperationRegion (OPR1, SystemMemory, 0x2000, 6)
    Field (OPR1, DWordAcc, NoLock, Preserve)
    {
        Offset (4),
        FLD1, 8
    }

    // Some address spaces support only ByteAcc or BufferAcc

    OperationRegion (OPR2, EmbeddedControl, 0x4000, 8)
    Field (OPR2, DWordAcc, NoLock, Preserve)
    {
        FLD2, 8
    }
    OperationRegion (OPR3, SMBus, 0x8000, 16)
    Field (OPR3, WordAcc, NoLock, Preserve)
    {
        FLD3, 8
    }

    // Invalid SyncLevel in method declaration

    Method (MTH1, 0, NotSerialized, 32)
    {
        // Invalid arguments and uninitialized locals

        Store (Arg3, Local0)
        Store (Local1, Local2)

        // Parameter typechecking (MTX1 is invalid type)

        Subtract (MTX1, 4, Local3)

        // Various invalid parameters

        CreateField (BUF1, 0, Subtract (4, 4), FLD1)

        // Unchecked mutex and event timeouts

        Acquire (MTX1, 100)
        Wait (EVT1, 1)

        // Result from operation is not used - statement has no effect

        Add (INT1, 8)

        // Unreachable code

        Return (0)
        Store (5, INT1)
    }

    Method (MTH2)
    {
        // Switch with no Case statements

        Switch (ToInteger (INT1))
        {
            Default
            {
            }
        }

        if (LEqual (INT1, 0))
        {
            Return (INT1)
        }

        // Fallthrough exit path does not return a value
    }

    Method (MTH3)
    {
        // Method MTH2 above does not always return a value

        Store (MTH2 (), Local0)
    }

    // Method MTH4 does not explicitly return a value

    Method (MTH4) {}
    Method (MTH5) {Store (MTH4(), Local0)}

    // Invalid _HID values

    Device (H1)
    {
        Name (_HID, "*PNP0C0A")     // Illegal leading asterisk
    }
    Device (H2)
    {
        Name (_HID, "PNP")          // Too short, must be 7 or 8 chars
    }
    Device (H3)
    {
        Name (_HID, "MYDEVICE01")   // Too long, must be 7 or 8 chars
    }
    Device (H4)
    {
        Name (_HID, "acpi0001")     // non-hex chars must be uppercase
    }
    Device (H5)
    {
        Name (_HID, "PNP-123")      // HID must be alphanumeric
    }
    Device (H6)
    {
        Name (_HID, "")             // Illegal Null HID
        Name (_CID, "")             // Illegal Null CID
    }

    // Predefined Name typechecking

    Name (_PRW, 4)
    Name (_FDI, Buffer () {0})

    // Predefined Name argument count validation
    // and return value validation

    Method (_OSC, 5)
    {
    }

    // Predefined Names that must be implemented as control methods

    Name (_L01, 1)
    Name (_E02, 2)
    Name (_Q03, 3)
    Name (_ON,  0)
    Name (_INI, 1)
    Name (_PTP, 2)

    // GPE methods that cause type collision (L vs. E)

    Scope (\_GPE)
    {
        Method (_L1D)
        {
        }
        Method (_E1D)
        {
        }
    }

    // Predefined names that should not have a return value

    Method (_FDM, 1)
    {
        Return (Buffer(1){0x33})
    }
    Method (_Q22)
    {
        Return ("Unexpected Return Value")
    }

    // _REG must have a corresponding Operation Region declaration
    // within the same scope

    Device (EC)
    {
        Method (_REG, 2)
        {
        }
    }

    /*
     * Resource Descriptor error checking
     */
    Name (RSC1, ResourceTemplate ()
    {
        // Illegal nested StartDependent macros

        StartDependentFn (0, 0)
        {
            StartDependentFn (0, 0)
            {
            }
        }

        // Missing EndDependentFn macro
    })

    Name (RSC2, ResourceTemplate ()
    {
        // AddressMin is larger than AddressMax
        IO (Decode16,
            0x07D0,             // Range Minimum
            0x03E8,             // Range Maximum
            0x01,               // Alignment
            0x20,               // Length
            )

        // Length larger than Min/Max window size
        Memory32 (ReadOnly,
            0x00001000,         // Range Minimum
            0x00002000,         // Range Maximum
            0x00000004,         // Alignment
            0x00002000,         // Length
            )

        // Min and Max not multiples of alignment value
        Memory32 (ReadOnly,
            0x00001001,         // Range Minimum
            0x00002002,         // Range Maximum
            0x00000004,         // Alignment
            0x00000200,         // Length
            )

        // 10-bit ISA I/O address has a max of 0x3FF
        FixedIO (
            0xFFFF,             // Address
            0x20,               // Length
            )

        // Invalid AccessSize parameter
        Register (SystemIO,
            0x08,               // Bit Width
            0x00,               // Bit Offset
            0x0000000000000100, // Address
            0x05                // Access Size
            )

        // Invalid ResourceType (0xB0)
        QWordSpace (0xB0, ResourceConsumer, PosDecode, MinFixed, MaxFixed, 0xA5,
            0x0000,             // Granularity
            0xA000,             // Range Minimum
            0xBFFF,             // Range Maximum
            0x0000,             // Translation Offset
            0x2000,             // Length
            ,, )

        // AddressMin is larger than AddressMax
        WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
            0x0000,             // Granularity
            0x0200,             // Range Minimum
            0x0100,             // Range Maximum
            0x0000,             // Translation Offset
            0x0100,             // Length
            ,, , TypeStatic)

        // Length larger than Min/Max window size
        DWordSpace (0xC3, ResourceConsumer, PosDecode, MinFixed, MaxFixed, 0xA5,
            0x00000000,         // Granularity
            0x000C8000,         // Range Minimum
            0x000C9000,         // Range Maximum
            0x00000000,         // Translation Offset
            0x00001002,         // Length
            ,, )

        // Granularity must be (power-of-two -1)
        DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxNotFixed, NonCacheable, ReadWrite,
            0x00000010,
            0x40000000,
            0xFED9FFFF,
            0x00000000,
            0xBECA0000)

        // Address Min (with zero length) not on granularity boundary
        QWordIO (ResourceProducer, MinFixed, MaxNotFixed, PosDecode, EntireRange,
            0x0000000000000003, // Granularity
            0x0000000000000B02, // Range Minimum
            0x0000000000000C00, // Range Maximum
            0x0000000000000000, // Translation Offset
            0x0000000000000000, // Length
            ,, , TypeStatic)

        // Address Max (with zero length) not on (granularity boundary -1)
        QWordMemory (ResourceProducer, PosDecode, MinNotFixed, MaxFixed, Cacheable, ReadWrite,
            0x0000000000000001, // Granularity
            0x0000000000100000, // Range Minimum
            0x00000000002FFFFE, // Range Maximum
            0x0000000000000000, // Translation Offset
            0x0000000000000000, // Length
            ,, , AddressRangeMemory, TypeStatic)

        // Invalid combination: zero length, both Min and Max are fixed
        DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
            0x00000000,         // Granularity
            0x000C8000,         // Range Minimum
            0x000C8FFF,         // Range Maximum
            0x00000000,         // Translation Offset
            0x00000000,         // Length
            ,, )

        // Invalid combination: non-zero length, Min Fixed, Max not fixed
        DWordIO (ResourceProducer, MinFixed, MaxNotFixed, PosDecode, EntireRange,
            0x00000001,         // Granularity
            0x000C8000,         // Range Minimum
            0x000C8FFF,         // Range Maximum
            0x00000000,         // Translation Offset
            0x00000100,         // Length
            ,, )

        // Invalid combination: non-zero length, Min not Fixed, Max fixed
        DWordIO (ResourceProducer, MinNotFixed, MaxFixed, PosDecode, EntireRange,
            0x00000001,         // Granularity
            0x000C8000,         // Range Minimum
            0x000C8FFF,         // Range Maximum
            0x00000000,         // Translation Offset
            0x00000200,         // Length
            ,, )

        // Granularity must be zero if non-zero length, min/max fixed
        DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
            0x0000000F,         // Granularity
            0x000C8000,         // Range Minimum
            0x000C8FFF,         // Range Maximum
            0x00000000,         // Translation Offset
            0x00001000,         // Length
            ,, )

        // Null descriptor (intended to be modified at runtime) must
        // have a resource tag (to allow it to be modified at runtime)
        DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
            0x00000000,         // Granularity
            0x00000000,         // Range Minimum
            0x00000000,         // Range Maximum
            0x00000000,         // Translation Offset
            0x00000000,         // Length
            ,, )

        // Missing StartDependentFn macro

        EndDependentFn ()
    })

    // Test descriptor for CreateXxxxField operators in REM1 below

    Name (RSC3, ResourceTemplate ()
    {
        DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
            0x00000000,         // Granularity
            0x000C8000,         // Range Minimum
            0x000C8FFF,         // Range Maximum
            0x00000000,         // Translation Offset
            0x00001000,         // Length
            ,, DWI1)
    })

    Method (REM1)
    {
        // Tagged resource field larger than field being created

        CreateWordField (RSC3, \DWI1._LEN, LEN)
        CreateByteField (RSC3, \DWI1._MIN, MIN)
        CreateBitField (RSC3, \DWI1._RNG, RNG1)

        // Tagged resource field smaller than field being created

        CreateQWordField (RSC3, \DWI1._MAX, MAX)
        CreateBitField (RSC3, \DWI1._GRA, GRA)
        CreateField (RSC3, \DWI1._MIF, 5, MIF)
        CreateField (RSC3, \DWI1._RNG, 3, RNG2)
    }

    Method (L100)
    {
        /* Method Local is set but never used */

        Store (40, Local0)
    }
}

