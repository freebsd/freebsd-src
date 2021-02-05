#
# Annapurna AL SoC support
#

# Annapurna Alpine drivers
device		al_ccu			# Alpine Cache Coherency Unit
device		al_nb_service		# Alpine North Bridge Service
device		al_iofic		# I/O Fabric Interrupt Controller
device		al_serdes		# Serializer/Deserializer
device		al_udma			# Universal DMA

# Bus drivers
device		al_pci			# Annapurna Alpine PCI-E

# Serial (COM) ports
device		uart_ns8250		# ns8250-type UART driver

# Ethernet NICs
device		al_eth			# Annapurna Alpine Ethernet NIC

# The `bpf' device enables the Berkeley Packet Filter.
# Be aware of the administrative consequences of enabling this!
# Note that 'bpf' is required for DHCP.
device		bpf		# Berkeley packet filter

options 	FDT
device		acpi
