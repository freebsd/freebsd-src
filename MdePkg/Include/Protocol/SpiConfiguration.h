/** @file
  This file defines the SPI Configuration Protocol.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    This Protocol was introduced in UEFI PI Specification 1.6.

**/

#ifndef __SPI_CONFIGURATION_PROTOCOL_H__
#define __SPI_CONFIGURATION_PROTOCOL_H__

///
/// Global ID for the SPI Configuration Protocol
///
#define EFI_SPI_CONFIGURATION_GUID  \
  { 0x85a6d3e6, 0xb65b, 0x4afc,     \
    { 0xb3, 0x8f, 0xc6, 0xd5, 0x4a, 0xf6, 0xdd, 0xc8 }}

///
/// Macros to easily specify frequencies in hertz, kilohertz and megahertz.
///
#define Hz(Frequency)   (Frequency)
#define KHz(Frequency)  (1000 * Hz (Frequency))
#define MHz(Frequency)  (1000 * KHz (Frequency))

typedef struct _EFI_SPI_PERIPHERAL EFI_SPI_PERIPHERAL;

/**
  Manipulate the chip select for a SPI device.

  This routine must be called at or below TPL_NOTIFY.
  Update the value of the chip select line for a SPI peripheral.
  The SPI bus layer calls this routine either in the board layer or in the SPI
  controller to manipulate the chip select pin at the start and end of a SPI
  transaction.

  @param[in] SpiPeripheral  The address of an EFI_SPI_PERIPHERAL data structure
                            describing the SPI peripheral whose chip select pin
                            is to be manipulated. The routine may access the
                            ChipSelectParameter field to gain sufficient
                            context to complete the operation.
  @param[in] PinValue       The value to be applied to the chip select line of
                            the SPI peripheral.

  @retval EFI_SUCCESS            The chip select was set successfully
  @retval EFI_NOT_READY          Support for the chip select is not properly
                                 initialized
  @retval EFI_INVALID_PARAMETER  The SpiPeripheral->ChipSelectParameter value
                                 is invalid

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SPI_CHIP_SELECT) (
  IN CONST EFI_SPI_PERIPHERAL  *SpiPeripheral,
  IN BOOLEAN                   PinValue
  );

/**
  Set up the clock generator to produce the correct clock frequency, phase and
  polarity for a SPI chip.

  This routine must be called at or below TPL_NOTIFY.
  This routine updates the clock generator to generate the correct frequency
  and polarity for the SPI clock.

  @param[in] SpiPeripheral  Pointer to a EFI_SPI_PERIPHERAL data structure from
                            which the routine can access the ClockParameter,
                            ClockPhase and ClockPolarity fields. The routine
                            also has access to the names for the SPI bus and
                            chip which can be used during debugging.
  @param[in] ClockHz        Pointer to the requested clock frequency. The clock
                            generator will choose a supported clock frequency
                            which is less then or equal to this value.
                            Specify zero to turn the clock generator off.
                            The actual clock frequency supported by the clock
                            generator will be returned.

  @retval EFI_SUCCESS      The clock was set up successfully
  @retval EFI_UNSUPPORTED  The SPI controller was not able to support the
                           frequency requested by CLockHz

**/
typedef EFI_STATUS
(EFIAPI *EFI_SPI_CLOCK) (
  IN CONST EFI_SPI_PERIPHERAL  *SpiPeripheral,
  IN UINT32                    *ClockHz
  );

///
/// The EFI_SPI_PART data structure provides a description of a SPI part which
/// is independent of the use on the board. This data is available directly
/// from the part's datasheet and may be provided by the vendor.
///
typedef struct _EFI_SPI_PART {
  ///
  /// A Unicode string specifying the SPI chip vendor.
  ///
  CONST CHAR16 *Vendor;

  ///
  /// A Unicode string specifying the SPI chip part number.
  ///
  CONST CHAR16 *PartNumber;

  ///
  /// The minimum SPI bus clock frequency used to access this chip. This value
  /// may be specified in the chip's datasheet. If not, use the value of zero.
  ///
  UINT32       MinClockHz;

  ///
  /// The maximum SPI bus clock frequency used to access this chip. This value
  /// is found in the chip's datasheet.
  ///
  UINT32       MaxClockHz;

  ///
  /// Specify the polarity of the chip select pin. This value can be found in
  /// the SPI chip's datasheet. Specify TRUE when a one asserts the chip select
  ///and FALSE when a zero asserts the chip select.
  ///
  BOOLEAN      ChipSelectPolarity;
} EFI_SPI_PART;

///
/// The EFI_SPI_BUS data structure provides the connection details between the
/// physical SPI bus and the EFI_SPI_HC_PROTOCOL instance which controls that
/// SPI bus. This data structure also describes the details of how the clock is
/// generated for that SPI bus. Finally this data structure provides the list
/// of physical SPI devices which are attached to the SPI bus.
///
typedef struct _EFI_SPI_BUS {
  ///
  /// A Unicode string describing the SPI bus
  ///
  CONST CHAR16                   *FriendlyName;

  ///
  /// Address of the first EFI_SPI_PERIPHERAL data structure connected to this
  /// bus. Specify NULL if there are no SPI peripherals connected to this bus.
  ///
  CONST EFI_SPI_PERIPHERAL       *Peripherallist;

  ///
  /// Address of an EFI_DEVICE_PATH_PROTOCOL data structure which uniquely
  /// describes the SPI controller.
  ///
  CONST EFI_DEVICE_PATH_PROTOCOL *ControllerPath;

  ///
  /// Address of the routine which controls the clock used by the SPI bus for
  /// this SPI peripheral. The SPI host co ntroller's clock routine is called
  /// when this value is set to NULL.
  ///
  EFI_SPI_CLOCK                  Clock;

  ///
  /// Address of a data structure containing the additional values which
  /// describe the necessary control for the clock. When Clock is NULL,
  /// the declaration for this data structure is provided by the vendor of the
  /// host's SPI controller driver. When Clock is not NULL, the declaration for
  /// this data structure is provided by the board layer.
  ///
  VOID                           *ClockParameter;
} EFI_SPI_BUS;

///
/// The EFI_SPI_PERIPHERAL data structure describes how a specific block of
/// logic which is connected to the SPI bus. This data structure also selects
/// which upper level driver is used to manipulate this SPI device.
/// The SpiPeripheraLDriverGuid is available from the vendor of the SPI
/// peripheral driver.
///
struct _EFI_SPI_PERIPHERAL {
  ///
  /// Address of the next EFI_SPI_PERIPHERAL data structure. Specify NULL if
  /// the current data structure is the last one on the SPI bus.
  ///
  CONST EFI_SPI_PERIPHERAL *NextSpiPeripheral;

  ///
  /// A unicode string describing the function of the SPI part.
  ///
  CONST CHAR16             *FriendlyName;

  ///
  /// Address of a GUID provided by the vendor of the SPI peripheral driver.
  /// Instead of using a " EFI_SPI_IO_PROTOCOL" GUID, the SPI bus driver uses
  /// this GUID to identify an EFI_SPI_IO_PROTOCOL data structure and to
  /// provide the connection points for the SPI peripheral drivers.
  /// This reduces the comparison logic in the SPI peripheral driver's
  /// Supported routine.
  ///
  CONST GUID               *SpiPeripheralDriverGuid;

  ///
  /// The address of an EFI_SPI_PART data structure which describes this chip.
  ///
  CONST EFI_SPI_PART       *SpiPart;

  ///
  /// The maximum clock frequency is specified in the EFI_SPI_P ART. When this
  /// this value is non-zero and less than the value in the EFI_SPI_PART then
  /// this value is used for the maximum clock frequency for the SPI part.
  ///
  UINT32                   MaxClockHz;

  ///
  /// Specify the idle value of the clock as found in the datasheet.
  /// Use zero (0) if the clock'S idle value is low or one (1) if the the
  /// clock's idle value is high.
  ///
  BOOLEAN                  ClockPolarity;

  ///
  /// Specify the clock delay after chip select. Specify zero (0) to delay an
  /// entire clock cycle or one (1) to delay only half a clock cycle.
  ///
  BOOLEAN                  ClockPhase;

  ///
  /// SPI peripheral attributes, select zero or more of:
  /// * SPI_PART_SUPPORTS_2_B1T_DATA_BUS_W1DTH - The SPI peripheral is wired to
  ///   support a 2-bit data bus
  /// * SPI_PART_SUPPORTS_4_B1T_DATA_BUS_W1DTH - The SPI peripheral is wired to
  ///   support a 4-bit data bus
  ///
  UINT32                   Attributes;

  ///
  /// Address of a vendor specific data structure containing additional board
  /// configuration details related to the SPI chip. The SPI peripheral layer
  /// uses this data structure when configuring the chip.
  ///
  CONST VOID               *ConfigurationData;

  ///
  /// The address of an EFI_SPI_BUS data structure which describes the SPI bus
  /// to which this chip is connected.
  ///
  CONST EFI_SPI_BUS        *SpiBus;

  ///
  /// Address of the routine which controls the chip select pin for this SPI
  /// peripheral. Call the SPI host controller's chip select routine when this
  /// value is set to NULL.
  ///
  EFI_SPI_CHIP_SELECT      ChipSelect;

  ///
  /// Address of a data structure containing the additional values which
  /// describe the necessary control for the chip select. When ChipSelect is
  /// NULL, the declaration for this data structure is provided by the vendor
  /// of the host's SPI controller driver. The vendor's documentation specifies
  /// the necessary values to use for the chip select pin selection and
  /// control. When Chipselect is not NULL, the declaration for this data
  /// structure is provided by the board layer.
  ///
  VOID                     *ChipSelectParameter;
};

///
/// Describe the details of the board's SPI busses to the SPI driver stack.
/// The board layer uses the EFI_SPI_CONFIGURATION_PROTOCOL to expose the data
/// tables which describe the board's SPI busses, The SPI bus layer uses these
/// tables to configure the clock, chip select and manage the SPI transactions
/// on the SPI controllers.
///
typedef struct _EFI_SPI_CONFIGURATION_PROTOCOL {
  ///
  /// The number of SPI busses on the board.
  ///
  UINT32                          BusCount;

  ///
  /// The address of an array of EFI_SPI_BUS data structure addresses.
  ///
  CONST EFI_SPI_BUS *CONST *CONST Buslist;
} EFI_SPI_CONFIGURATION_PROTOCOL;

extern EFI_GUID gEfiSpiConfigurationProtocolGuid;

#endif // __SPI_CONFIGURATION_PROTOCOL_H__
