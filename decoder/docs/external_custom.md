Attaching External Custom Decoders    {#custom_decoders}
==================================

@brief A description of the C API external decoder interface.

Introduction
------------

An external custom decoder is one which decodes a CoreSight trace byte stream from a source other
than an ARM core which cannot be decoded by the standard built-in decoders within the library.

An example of this may be a trace stream from a DSP device.

The external decoder API allows a suitable decoder to be attached to the library and used in the 
same way as the built-in decoders. This means that the external decoder can be created and destroyed
using the decode tree API, and will integrate seamlessly with any ARM processor decoders that are part
of the same tree.

An external decoder will be required to use three standard structures:-

- `ocsd_extern_dcd_fact_t` : This is a decoder "factory" that allows the creation of the custom decoders.
- `ocsd_extern_dcd_inst_t` : This structure provides decoder data to the library for a single decoder instance.
- `ocsd_extern_dcd_cb_fns` : This structure provides a set of callback functions allowing the decoder to use library functionality in the same way as built-in decoders.

These structures consist of data and function pointers to allow integration with the library infrastructure.

Registering A Decoder
---------------------

A single API function is provided to allow a decoder to be registered with the library by name. 

    ocsd_err_t ocsd_register_custom_decoder(const char *name, ocsd_extern_dcd_fact_t *p_dcd_fact);

This registers the custom decoder with the library using the supplied name and factory structure.
As part of the registration function the custom decoder will be assigned a protocol ID which may be used in 
API functions requiring this parameter.

Once registered, the standard API functions used with the built-in decoders will work with the custom decoder.

The Factory Structure
---------------------
This structure contains the interface that is registered with the library to allow the creation of custom decoder instances.

The mandatory functions that must be provided include:
- `fnCreateCustomDecoder`  : Creates a decoder. This function will fill in a `ocsd_extern_dcd_inst_t` structure for the decoder instance.
- `fnDestroyCustomDecoder` : Destroys the decoder. Takes the `decoder_handle` attribute of the instance structure.
- `fnGetCSIDFromConfig`    : Extracts the CoreSight Trace ID from the decoder configuration structure. 
                             May be called before the create function. The CSID is used as part of the creation process to 
                             attach the decoder to the correct trace byte stream.

`fnPacketToString` : This optional function will provide a human readable string from a protocol specific packet structure.

`protocol_id` : This is filled in when the decoder type is registered with the library. Used in some API 
                calls to specify the decoder protocol type.



Creating a Custom Decoder Instance
----------------------------------

Once the custom decoder factory has been registered with the library then using the decoder uses the standard creation API:-

`ocsd_dt_create_decoder(const dcd_tree_handle_t handle, const char *decoder_name, const int create_flags,
                                             const void *decoder_cfg, unsigned char *pCSID)`

                                             
This creates a decoder by type name in the current decode tree and attaches it to the trace data stream associated with a CoreSight trace ID extracted from 
the trace configuration. 

To create a custom decoder instance simply use the custom name and a pointer to the custom configuration structure.                                             

Calling this on a custom decoder name will result in a call to the factor function `fnCreateCustomDecoder` function:-
`ocsd_err_t CreateCustomDecoder(const int create_flags, const void *decoder_cfg, const ocsd_extern_dcd_cb_fns *p_lib_callbacks, ocsd_extern_dcd_inst_t *p_decoder_inst)`

This will first require that the `ocsd_extern_dcd_inst_t` structure is populated.

There is are two mandatory function calls in this structure that may be called by the library 

   `fnTraceDataIn` : the decoder must provide this as this is called by the library to provide the 
                     raw trace data to the decoder.
                     
    `fn_update_pkt_mon` : Allows the library to communicate when packet sink / packet monitor interfaces are attached to the decoder and in use.
    
The decoder creation process will also fill in the additional information to allow the library to correctly call back into the custom decoder using the `decoder_handle` parameter.

Secondly the library will provide a structure of callback functions - `ocsd_extern_dcd_cb_fns` - that the decoder can use to access standard library functionality.
This includes the standard error and message logging functions, the memory access and ARM instruction decode functions, plus the current output sink for generic 
trace elements generated by the decoder. The decoder is not required to use these functions - indeed the ARM instruction decode will not be useful to none ARM 
architecture decoders, but should where possible use these functions if being used as part of a combined ARM / custom decoder tree. This will simplify client 
use of the external decoders.

The `create_flags` parameter will describe the expected operational mode for the decoder. The flags are:-
- `OCSD_CREATE_FLG_PACKET_PROC`  : Packet processing only - the decoder will split the incoming stream into protocol trace packets and output these.
- `OCSD_CREATE_FLG_FULL_DECODER` : Full decode - the decoder will split the incoming stream into protocol trace packets and further decode and analyse these to produce generic trace output which may describe the program flow. 

Finally the decoder creation function will interpret the custom configuration (`decoder_cfg`) and fill in the CoreSight Trace ID parameter `pCSID` 
for this decoder instance. Decoder configuration structures describe registers and parameters used in programming up the trace source. The only 
minimum requirement is that it is possible to extract a CoreSight trace ID from the configuration to allow the library to attach the correct byte 
stream to the decoder.


Example : The echo_test decoder
--------------------------------

The echo_test decoder is provided to both test the C-API interfaces provided for using custom decoders and as a worked example for using these interfaces.

This decoder is initialised and created by the `c_api_pkt_print_test` program when the `-extern` command line option is used. 

In order to use a custom decoder, the header files for that decoder must be included by the client as they are not part of the built-in provided by the standard library includes.

    #include "ext_dcd_echo_test_fact.h"     // provides the ext_echo_get_dcd_fact() fn
    #include "ext_dcd_echo_test.h"          // provides the echo_dcd_cfg_t config structure.

The `register_extern_decoder()` function in the test shows how simple the API is to use.

The implementation of the decoder provides an external function to get a factory structure.

    p_ext_fact = ext_echo_get_dcd_fact();
    
Assuming this returns a structure then the decoder is registered by name.

    if (p_ext_fact)
    {
        err = ocsd_register_custom_decoder(EXT_DCD_NAME, p_ext_fact);
    }

After this the test uses the same code path as the built in decoders when testing the custom decoder.
The test function `ocsd_err_t create_decoder_extern(dcd_tree_handle_t dcd_tree_h)` is called if the test parameters indicate a custom decoder is needed.
This populates the custom configuration structure specific to the echo_test decoder (`echo_dcd_cfg_t`), then passes this plus the decoder name to the same `create_generic_decoder()` function used when testing the built in decoders.


    static ocsd_err_t create_decoder_extern(dcd_tree_handle_t dcd_tree_h)
    {
        echo_dcd_cfg_t trace_cfg_ext;

        /* setup the custom configuration */
        trace_cfg_ext.cs_id = 0x010;
        if (test_trc_id_override != 0)
        {
            trace_cfg_ext.cs_id = (uint32_t)test_trc_id_override;
        }

        /* create an external decoder - no context needed as we have a single stream to a single handler. */
        return create_generic_decoder(dcd_tree_h, EXT_DCD_NAME, (void *)&trace_cfg_ext, 0);
    }

From the test program perspective, these are the only changes made to the test program to test this decoder.
The `create_generic_decoder()` then uses the normal C-API calls such as `ocsd_dt_create_decoder()` and `ocsd_dt_attach_packet_callback()` to hook the decoder into the decode tree infrastructure.
