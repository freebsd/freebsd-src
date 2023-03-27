ETE Test Snapshot.
------------------

This test snapshot is an ETMv4 sourced trace snapshot, with the device files altered
from ETMv4 to ETE. This is to test the infrastructure of creating and running an ETE
decoder, not the new packet types etc. 

Running this on the library may will cause errors on ERET packets unless the debug define 
is set to ignore EREI (ETE_TRACE_ERET_AS_IGNORE in trc_pkt_proc_etmv4i_impl.cpp)
