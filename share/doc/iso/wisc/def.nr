.NC "Definitions"
.sh 1 "General Terms"
.ip "Kernel" 5
The source code or binary module for the Acis Operating System 
(also know as AOS and IBM/4.3).
.ip "User process" 5
An instance of a program that is 
running in unprivileged mode, in the unprivileged address space 
commonly know as "user address space", in other words, not
part of the kernel.
.ip "IPC" 5
Interprocess communication, the mechanism by which two different
user processes send messages to each other.
.ip "Unix, AOS" 5
ACIS Operating System, the IBM ACIS port of Berkeley Unix 4.3BSD.
.ip "PCB, pcb" 5
Protocol control block.  Each instance of a protocol machine
keeps status information, addresses, and in some cases queues
in a pcb for each connection or socket.
.ip "Domain" 5
In the Berkeley Unix environment, a domain is an abstract entity which
comprises a network architecture, addressing scheme, address format, 
network protocols, and transport protocols.
.sh 1 "Transport Layer Terms"
.ip "ISO 8073"
ISO Draft International Standard 8073, Transport Protocol Specification
.ip "TP" 5 
The collection of transport
classes that have been implemented in ARGO, classes 0 and 4.
Also means the ARGO implementation of TP. 
.ip "TP 0" 5 
Transport class 0.
.ip "TP 4" 5 
Transport class 4.
.ip "Transport entity" 5
Software or hardware that implements the elements of procedure
described in ISO 8073.
.ip "Transport user" 5
User process that make use of the services
provided by a transport entity.
.ip "Transport service interface" 5
The syntax and semantics of the set of procedures, functions, and system calls
that are invoked by a transport user, 
through which the services of the transport entity are delivered.
.ip "TPDU" 5
Transport protocol data unit, a packet that is
passed from one transport entity to another.
.ip "TSDU" 5
Transport service data unit, the logical unit of data that is
passed from a transport entity to a transport user, or from
a transport user to a transport entity.
.ip "CR TPDU" 5
Connection request TPDU. 
.ip "CC TPDU" 5
Connection confirm TPDU.
.ip "DR TPDU" 5
Disconnect request TPDU.
.ip "DC TPDU" 5
Disconnect confirm TPDU.
.ip "DT TPDU" 5
Normal data TPDU.
.ip "XPD TPDU" 5
Expedited data TPDU.
.ip "AK TPDU" 5
Normal data acknowledgment TPDU.
.ip "XAK TPDU" 5
Expedited data acknowledgment TPDU.
.ip "ER TPDU" 5
Error TPDU.
.sh 1 "Network Layer Terms"
.ip "ISO 8473"
ISO Draft International Standard 8473, connectionless network protocol.
.ip "CONS"
Connection Oriented Network Service.
.ip "COSNS"
Connection Oriented Sub-Network Service.
.ip "CLNS"
Connectionless Network Service.
.ip "CLNP"
Connectionless Network Protocol, or ISO 8473.
.ip "Network Entity"
Software or hardware that implements the elements of procedure described
in ISO 8473.
.ip "Network Service User"
Software components that make use of the services provided by a network
entity.
.ip "Network Service Provider"
Software components that provide the services of a network entity.
.ip "NSAP"
Network Service Access Point. The point at which the OSI network service
is made available to the network service user by the network service
provider.
.ip "NSAP address"
Information that the network service provider needs to identify an
NSAP. The source and destination address fields of a CLNP packet
are NSAP addresses.
.ip "ES"
End system. A system running the complete suite of OSI protocols which can
act as an end point for communication.
.ip "IS"
Intermediate system. A system running the OSI layers 1, 2, and 3 which
can act only a packet router.
.ip "SNPA"
The Subnetwork Point of Attachement is the point where a \fIreal\fR 
end or intermediate system is attached to a \fIreal\fR subnetwork.
.ip "SNPA address"
Information that a \fIreal\fR subnetwork need to identify a \fIreal\fR end 
or intermediate system. This is commonly referred to as the hardware address.
.ip "NPDU"
Network Protocol Data Unit. The unit of data which is exchanged between
network entities.
.ip "DT NPDU"
Normal data NPDU.
.ip "ER NPDU"
Error report NPDU.
.ip "Initial NPDU"
A NPDU carrying the whole of the user data from an N-UNITDATA request.
.ip "Derived NPDU"
a NPDU whose field ar identical to those of an initial NPDU, except that it
carries only a segment of the user data from an N-UNITDATA request.
.ip "Segment"
A distinct unit of data consisting of part or all of the user data provided
in the N-UNITDATA request and delivered in the N-UNITDATA indication.
.ip "Segmentation"
The act of generation two or more derived NPDUs from an initial or derived
NPDU.
.ip "Fragment"
A DoD Internet Protocol term with the same meaning as "segment". Used
synonymously with "segment."
.ip "Fragmentation"
A DoD Internet Protocol term with the same meaning as "segmentation". Used
synonymously with "segmentation."
.ip "Reassembly"
The act of regenerating an initial NPDU from two ore more derived NPDUs.
.ip "MTU"
Maximum transmission unit.  The maximum size of a packet that can be
transmitted on a medium or through a protocol.
For example, the MTU of the TP protocol is 8192 bytes, the MTU
of and Ethernet device is 1500 bytes, and the MTU of the OSI Network
service is 512 bytes.
.ip "Network interface"
The device used to attach a computer to a network, for example,
an Ethernet adapter, or a Token Ring adapter.
This unfortunate terminology is inherited from BSD Unix.
