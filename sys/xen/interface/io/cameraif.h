/******************************************************************************
 * cameraif.h
 *
 * Unified camera device I/O interface for Xen guest OSes.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (C) 2018-2019 EPAM Systems Inc.
 *
 * Author: Oleksandr Andrushchenko <oleksandr_andrushchenko@epam.com>
 */

#ifndef __XEN_PUBLIC_IO_CAMERAIF_H__
#define __XEN_PUBLIC_IO_CAMERAIF_H__

#include "ring.h"
#include "../grant_table.h"

/*
 ******************************************************************************
 *                           Protocol version
 ******************************************************************************
 */
#define XENCAMERA_PROTOCOL_VERSION     "1"

/*
 ******************************************************************************
 *                  Feature and Parameter Negotiation
 ******************************************************************************
 *
 * Front->back notifications: when enqueuing a new request, sending a
 * notification can be made conditional on xencamera_req (i.e., the generic
 * hold-off mechanism provided by the ring macros). Backends must set
 * xencamera_req appropriately (e.g., using RING_FINAL_CHECK_FOR_REQUESTS()).
 *
 * Back->front notifications: when enqueuing a new response, sending a
 * notification can be made conditional on xencamera_resp (i.e., the generic
 * hold-off mechanism provided by the ring macros). Frontends must set
 * xencamera_resp appropriately (e.g., using RING_FINAL_CHECK_FOR_RESPONSES()).
 *
 * The two halves of a para-virtual camera driver utilize nodes within
 * XenStore to communicate capabilities and to negotiate operating parameters.
 * This section enumerates these nodes which reside in the respective front and
 * backend portions of XenStore, following the XenBus convention.
 *
 * All data in XenStore is stored as strings. Nodes specifying numeric
 * values are encoded in decimal. Integer value ranges listed below are
 * expressed as fixed sized integer types capable of storing the conversion
 * of a properly formatted node string, without loss of information.
 *
 ******************************************************************************
 *                        Example configuration
 ******************************************************************************
 *
 * This is an example of backend and frontend configuration:
 *
 *--------------------------------- Backend -----------------------------------
 *
 * /local/domain/0/backend/vcamera/1/0/frontend-id = "1"
 * /local/domain/0/backend/vcamera/1/0/frontend = "/local/domain/1/device/vcamera/0"
 * /local/domain/0/backend/vcamera/1/0/state = "4"
 * /local/domain/0/backend/vcamera/1/0/versions = "1,2"
 *
 *--------------------------------- Frontend ----------------------------------
 *
 * /local/domain/1/device/vcamera/0/backend-id = "0"
 * /local/domain/1/device/vcamera/0/backend = "/local/domain/0/backend/vcamera/1"
 * /local/domain/1/device/vcamera/0/state = "4"
 * /local/domain/1/device/vcamera/0/version = "1"
 * /local/domain/1/device/vcamera/0/be-alloc = "1"
 *
 *---------------------------- Device 0 configuration -------------------------
 *
 * /local/domain/1/device/vcamera/0/max-buffers = "3"
 * /local/domain/1/device/vcamera/0/controls = "contrast,hue"
 * /local/domain/1/device/vcamera/0/formats/YUYV/640x480/frame-rates = "30/1,15/1"
 * /local/domain/1/device/vcamera/0/formats/YUYV/1920x1080/frame-rates = "15/2"
 * /local/domain/1/device/vcamera/0/formats/BGRA/640x480/frame-rates = "15/1,15/2"
 * /local/domain/1/device/vcamera/0/formats/BGRA/1200x720/frame-rates = "15/2"
 * /local/domain/1/device/vcamera/0/unique-id = "0"
 * /local/domain/1/device/vcamera/0/req-ring-ref = "2832"
 * /local/domain/1/device/vcamera/0/req-event-channel = "15"
 * /local/domain/1/device/vcamera/0/evt-ring-ref = "387"
 * /local/domain/1/device/vcamera/0/evt-event-channel = "16"
 *
 *---------------------------- Device 1 configuration -------------------------
 *
 * /local/domain/1/device/vcamera/1/max-buffers = "8"
 * /local/domain/1/device/vcamera/1/controls = "brightness,saturation,hue"
 * /local/domain/1/device/vcamera/1/formats/YUYV/640x480/frame-rates = "30/1,15/2"
 * /local/domain/1/device/vcamera/1/formats/YUYV/1920x1080/frame-rates = "15/2"
 * /local/domain/1/device/vcamera/1/unique-id = "1"
 * /local/domain/1/device/vcamera/1/req-ring-ref = "2833"
 * /local/domain/1/device/vcamera/1/req-event-channel = "17"
 * /local/domain/1/device/vcamera/1/evt-ring-ref = "388"
 * /local/domain/1/device/vcamera/1/evt-event-channel = "18"
 *
 ******************************************************************************
 *                            Backend XenBus Nodes
 ******************************************************************************
 *
 *----------------------------- Protocol version ------------------------------
 *
 * versions
 *      Values:         <string>
 *
 *      List of XENCAMERA_LIST_SEPARATOR separated protocol versions supported
 *      by the backend. For example "1,2,3".
 *
 ******************************************************************************
 *                            Frontend XenBus Nodes
 ******************************************************************************
 *
 *-------------------------------- Addressing ---------------------------------
 *
 * dom-id
 *      Values:         <uint16_t>
 *
 *      Domain identifier.
 *
 * dev-id
 *      Values:         <uint16_t>
 *
 *      Device identifier.
 *
 *      /local/domain/<dom-id>/device/vcamera/<dev-id>/...
 *
 *----------------------------- Protocol version ------------------------------
 *
 * version
 *      Values:         <string>
 *
 *      Protocol version, chosen among the ones supported by the backend.
 *
 *------------------------- Backend buffer allocation -------------------------
 *
 * be-alloc
 *      Values:         "0", "1"
 *
 *      If value is set to "1", then backend will be the buffer
 *      provider/allocator for this domain during XENCAMERA_OP_BUF_CREATE
 *      operation.
 *      If value is not "1" or omitted frontend must allocate buffers itself.
 *
 *------------------------------- Camera settings -----------------------------
 *
 * unique-id
 *      Values:         <string>
 *
 *      After device instance initialization each camera is assigned a
 *      unique ID, so it can be identified by the backend by this ID.
 *      This can be UUID or such.
 *
 * max-buffers
 *      Values:         <uint8_t>
 *
 *      Maximum number of camera buffers this frontend may use.
 *
 * controls
 *      Values:         <list of string>
 *
 *      List of supported camera controls separated by XENCAMERA_LIST_SEPARATOR.
 *      Camera controls are expressed as a list of string values w/o any
 *      ordering requirement.
 *
 * formats
 *      Values:         <format, char[7]>
 *
 *      Formats are organized as a set of directories one per each
 *      supported pixel format. The name of the directory is the
 *      corresponding FOURCC string label. The next level of
 *      the directory under <formats> represents supported resolutions.
 *      If the format represents a big-endian variant of a little
 *      endian format, then the "-BE" suffix must be added. E.g. 'AR15' vs
 *      'AR15-BE'.
 *      If FOURCC string label has spaces then those are only allowed to
 *      be at the end of the label and must be trimmed, for example
 *      'Y16' and 'Y16-BE' will be trimmed.
 *
 * resolution
 *      Values:         <width, uint32_t>x<height, uint32_t>
 *
 *      Resolutions are organized as a set of directories one per each
 *      supported resolution under corresponding <formats> directory.
 *      The name of the directory is the supported width and height
 *      of the camera resolution in pixels.
 *
 * frame-rates
 *      Values:         <numerator, uint32_t>/<denominator, uint32_t>
 *
 *      List of XENCAMERA_FRAME_RATE_SEPARATOR separated supported frame rates
 *      of the camera expressed as numerator and denominator of the
 *      corresponding frame rate.
 *
 *------------------- Camera Request Transport Parameters ---------------------
 *
 * This communication path is used to deliver requests from frontend to backend
 * and get the corresponding responses from backend to frontend,
 * set up per virtual camera device.
 *
 * req-event-channel
 *      Values:         <uint32_t>
 *
 *      The identifier of the Xen camera's control event channel
 *      used to signal activity in the ring buffer.
 *
 * req-ring-ref
 *      Values:         <uint32_t>
 *
 *      The Xen grant reference granting permission for the backend to map
 *      a sole page of camera's control ring buffer.
 *
 *-------------------- Camera Event Transport Parameters ----------------------
 *
 * This communication path is used to deliver asynchronous events from backend
 * to frontend, set up per virtual camera device.
 *
 * evt-event-channel
 *      Values:         <uint32_t>
 *
 *      The identifier of the Xen camera's event channel
 *      used to signal activity in the ring buffer.
 *
 * evt-ring-ref
 *      Values:         <uint32_t>
 *
 *      The Xen grant reference granting permission for the backend to map
 *      a sole page of camera's event ring buffer.
 */

/*
 ******************************************************************************
 *                               STATE DIAGRAMS
 ******************************************************************************
 *
 * Tool stack creates front and back state nodes with initial state
 * XenbusStateInitialising.
 * Tool stack creates and sets up frontend camera configuration
 * nodes per domain.
 *
 *-------------------------------- Normal flow --------------------------------
 *
 * Front                                Back
 * =================================    =====================================
 * XenbusStateInitialising              XenbusStateInitialising
 *                                       o Query backend device identification
 *                                         data.
 *                                       o Open and validate backend device.
 *                                                |
 *                                                |
 *                                                V
 *                                      XenbusStateInitWait
 *
 * o Query frontend configuration
 * o Allocate and initialize
 *   event channels per configured
 *   camera.
 * o Publish transport parameters
 *   that will be in effect during
 *   this connection.
 *              |
 *              |
 *              V
 * XenbusStateInitialised
 *
 *                                       o Query frontend transport parameters.
 *                                       o Connect to the event channels.
 *                                                |
 *                                                |
 *                                                V
 *                                      XenbusStateConnected
 *
 *  o Create and initialize OS
 *    virtual camera as per
 *    configuration.
 *              |
 *              |
 *              V
 * XenbusStateConnected
 *
 *                                      XenbusStateUnknown
 *                                      XenbusStateClosed
 *                                      XenbusStateClosing
 * o Remove virtual camera device
 * o Remove event channels
 *              |
 *              |
 *              V
 * XenbusStateClosed
 *
 *------------------------------- Recovery flow -------------------------------
 *
 * In case of frontend unrecoverable errors backend handles that as
 * if frontend goes into the XenbusStateClosed state.
 *
 * In case of backend unrecoverable errors frontend tries removing
 * the virtualized device. If this is possible at the moment of error,
 * then frontend goes into the XenbusStateInitialising state and is ready for
 * new connection with backend. If the virtualized device is still in use and
 * cannot be removed, then frontend goes into the XenbusStateReconfiguring state
 * until either the virtualized device is removed or backend initiates a new
 * connection. On the virtualized device removal frontend goes into the
 * XenbusStateInitialising state.
 *
 * Note on XenbusStateReconfiguring state of the frontend: if backend has
 * unrecoverable errors then frontend cannot send requests to the backend
 * and thus cannot provide functionality of the virtualized device anymore.
 * After backend is back to normal the virtualized device may still hold some
 * state: configuration in use, allocated buffers, client application state etc.
 * In most cases, this will require frontend to implement complex recovery
 * reconnect logic. Instead, by going into XenbusStateReconfiguring state,
 * frontend will make sure no new clients of the virtualized device are
 * accepted, allow existing client(s) to exit gracefully by signaling error
 * state etc.
 * Once all the clients are gone frontend can reinitialize the virtualized
 * device and get into XenbusStateInitialising state again signaling the
 * backend that a new connection can be made.
 *
 * There are multiple conditions possible under which frontend will go from
 * XenbusStateReconfiguring into XenbusStateInitialising, some of them are OS
 * specific. For example:
 * 1. The underlying OS framework may provide callbacks to signal that the last
 *    client of the virtualized device has gone and the device can be removed
 * 2. Frontend can schedule a deferred work (timer/tasklet/workqueue)
 *    to periodically check if this is the right time to re-try removal of
 *    the virtualized device.
 * 3. By any other means.
 *
 ******************************************************************************
 *                             REQUEST CODES
 ******************************************************************************
 */
#define XENCAMERA_OP_CONFIG_SET        0x00
#define XENCAMERA_OP_CONFIG_GET        0x01
#define XENCAMERA_OP_CONFIG_VALIDATE   0x02
#define XENCAMERA_OP_FRAME_RATE_SET    0x03
#define XENCAMERA_OP_BUF_GET_LAYOUT    0x04
#define XENCAMERA_OP_BUF_REQUEST       0x05
#define XENCAMERA_OP_BUF_CREATE        0x06
#define XENCAMERA_OP_BUF_DESTROY       0x07
#define XENCAMERA_OP_BUF_QUEUE         0x08
#define XENCAMERA_OP_BUF_DEQUEUE       0x09
#define XENCAMERA_OP_CTRL_ENUM         0x0a
#define XENCAMERA_OP_CTRL_SET          0x0b
#define XENCAMERA_OP_CTRL_GET          0x0c
#define XENCAMERA_OP_STREAM_START      0x0d
#define XENCAMERA_OP_STREAM_STOP       0x0e

#define XENCAMERA_CTRL_BRIGHTNESS      0
#define XENCAMERA_CTRL_CONTRAST        1
#define XENCAMERA_CTRL_SATURATION      2
#define XENCAMERA_CTRL_HUE             3

/* Number of supported controls. */
#define XENCAMERA_MAX_CTRL             4

/* Control is read-only. */
#define XENCAMERA_CTRL_FLG_RO          (1 << 0)
/* Control is write-only. */
#define XENCAMERA_CTRL_FLG_WO          (1 << 1)
/* Control's value is volatile. */
#define XENCAMERA_CTRL_FLG_VOLATILE    (1 << 2)

/* Supported color spaces. */
#define XENCAMERA_COLORSPACE_DEFAULT   0
#define XENCAMERA_COLORSPACE_SMPTE170M 1
#define XENCAMERA_COLORSPACE_REC709    2
#define XENCAMERA_COLORSPACE_SRGB      3
#define XENCAMERA_COLORSPACE_OPRGB     4
#define XENCAMERA_COLORSPACE_BT2020    5
#define XENCAMERA_COLORSPACE_DCI_P3    6

/* Color space transfer function. */
#define XENCAMERA_XFER_FUNC_DEFAULT    0
#define XENCAMERA_XFER_FUNC_709        1
#define XENCAMERA_XFER_FUNC_SRGB       2
#define XENCAMERA_XFER_FUNC_OPRGB      3
#define XENCAMERA_XFER_FUNC_NONE       4
#define XENCAMERA_XFER_FUNC_DCI_P3     5
#define XENCAMERA_XFER_FUNC_SMPTE2084  6

/* Color space Y’CbCr encoding. */
#define XENCAMERA_YCBCR_ENC_IGNORE           0
#define XENCAMERA_YCBCR_ENC_601              1
#define XENCAMERA_YCBCR_ENC_709              2
#define XENCAMERA_YCBCR_ENC_XV601            3
#define XENCAMERA_YCBCR_ENC_XV709            4
#define XENCAMERA_YCBCR_ENC_BT2020           5
#define XENCAMERA_YCBCR_ENC_BT2020_CONST_LUM 6

/* Quantization range. */
#define XENCAMERA_QUANTIZATION_DEFAULT       0
#define XENCAMERA_QUANTIZATION_FULL_RANGE    1
#define XENCAMERA_QUANTIZATION_LIM_RANGE     2

/*
 ******************************************************************************
 *                                 EVENT CODES
 ******************************************************************************
 */
#define XENCAMERA_EVT_FRAME_AVAIL      0x00
#define XENCAMERA_EVT_CTRL_CHANGE      0x01

/*
 ******************************************************************************
 *               XENSTORE FIELD AND PATH NAME STRINGS, HELPERS
 ******************************************************************************
 */
#define XENCAMERA_DRIVER_NAME          "vcamera"

#define XENCAMERA_LIST_SEPARATOR       ","
#define XENCAMERA_RESOLUTION_SEPARATOR "x"
#define XENCAMERA_FRACTION_SEPARATOR   "/"

#define XENCAMERA_FIELD_BE_VERSIONS    "versions"
#define XENCAMERA_FIELD_FE_VERSION     "version"
#define XENCAMERA_FIELD_REQ_RING_REF   "req-ring-ref"
#define XENCAMERA_FIELD_REQ_CHANNEL    "req-event-channel"
#define XENCAMERA_FIELD_EVT_RING_REF   "evt-ring-ref"
#define XENCAMERA_FIELD_EVT_CHANNEL    "evt-event-channel"
#define XENCAMERA_FIELD_MAX_BUFFERS    "max-buffers"
#define XENCAMERA_FIELD_CONTROLS       "controls"
#define XENCAMERA_FIELD_FORMATS        "formats"
#define XENCAMERA_FIELD_FRAME_RATES    "frame-rates"
#define XENCAMERA_FIELD_BE_ALLOC       "be-alloc"
#define XENCAMERA_FIELD_UNIQUE_ID      "unique-id"

#define XENCAMERA_CTRL_BRIGHTNESS_STR  "brightness"
#define XENCAMERA_CTRL_CONTRAST_STR    "contrast"
#define XENCAMERA_CTRL_SATURATION_STR  "saturation"
#define XENCAMERA_CTRL_HUE_STR         "hue"

#define XENCAMERA_FOURCC_BIGENDIAN_STR "-BE"

/* Maximum number of buffer planes supported. */
#define XENCAMERA_MAX_PLANE            4

/*
 ******************************************************************************
 *                          STATUS RETURN CODES
 ******************************************************************************
 *
 * Status return code is zero on success and -XEN_EXX on failure.
 *
 ******************************************************************************
 *                              Assumptions
 ******************************************************************************
 *
 * - usage of grant reference 0 as invalid grant reference:
 *   grant reference 0 is valid, but never exposed to a PV driver,
 *   because of the fact it is already in use/reserved by the PV console.
 * - all references in this document to page sizes must be treated
 *   as pages of size XEN_PAGE_SIZE unless otherwise noted.
 * - all FOURCC mappings used for configuration and messaging are
 *   Linux V4L2 ones: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/videodev2.h
 *   with the following exceptions:
 *     - characters are allowed in [0x20; 0x7f] range
 *     - when used for XenStore configuration entries the following
 *       are not allowed:
 *       - '/', '\', ' ' (space), '<', '>', ':', '"', '|', '?', '*'
 *       - if trailing spaces are part of the FOURCC code then those must be
 *         trimmed
 *
 *
 ******************************************************************************
 *       Description of the protocol between frontend and backend driver
 ******************************************************************************
 *
 * The two halves of a Para-virtual camera driver communicate with
 * each other using shared pages and event channels.
 * Shared page contains a ring with request/response packets.
 *
 * All reserved fields in the structures below must be 0.
 *
 * For all request/response/event packets:
 *   - frame rate parameter is represented as a pair of 4 octet long
 *     numerator and denominator:
 *       - frame_rate_numer - uint32_t, numerator of the frame rate
 *       - frame_rate_denom - uint32_t, denominator of the frame rate
 *     The corresponding frame rate (Hz) is calculated as:
 *       frame_rate = frame_rate_numer / frame_rate_denom
 *   - buffer index is a zero based index of the buffer. Must be less than
 *     the value of XENCAMERA_OP_CONFIG_SET.num_bufs response:
 *       - index - uint8_t, index of the buffer.
 *
 *
 *---------------------------------- Requests ---------------------------------
 *
 * All request packets have the same length (64 octets).
 * All request packets have common header:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |    operation   |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 *   id - uint16_t, private guest value, echoed in response.
 *   operation - uint8_t, operation code, XENCAMERA_OP_XXX.
 *
 *
 * Request to set/validate the configuration - request to set the
 * configuration/mode of the camera (XENCAMERA_OP_CONFIG_SET) or to
 * check if the configuration is valid and can be used
 * (XENCAMERA_OP_CONFIG_VALIDATE):
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_CONFIG_XXX |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |                            pixel format                           | 12
 * +----------------+----------------+----------------+----------------+
 * |                               width                               | 16
 * +----------------+----------------+----------------+----------------+
 * |                               height                              | 20
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 24
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * pixel_format - uint32_t, pixel format to be used, FOURCC code.
 * width - uint32_t, width in pixels.
 * height - uint32_t, height in pixels.
 *
 * See response format for this request.
 *
 * Notes:
 *  - the only difference between XENCAMERA_OP_CONFIG_VALIDATE and
 *    XENCAMERA_OP_CONFIG_SET is that the former doesn't actually change
 *    camera configuration, but queries if the configuration is valid.
 *    This can be used while stream is active and/or buffers allocated.
 *  - frontend must check the corresponding response in order to see
 *    if the values reported back by the backend do match the desired ones
 *    and can be accepted.
 *  - frontend may send multiple XENCAMERA_OP_CONFIG_SET requests before
 *    sending XENCAMERA_OP_STREAM_START request to update or tune the
 *    final stream configuration.
 *  - configuration cannot be changed during active streaming, e.g.
 *    after XENCAMERA_OP_STREAM_START and before XENCAMERA_OP_STREAM_STOP
 *    requests.
 */
struct xencamera_config_req {
    uint32_t pixel_format;
    uint32_t width;
    uint32_t height;
};

/*
 * Request current configuration of the camera:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_CONFIG_GET |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * See response format for this request.
 *
 *
 * Request to set the frame rate of the stream:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _FRAME_RATE_SET|   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |                          frame_rate_numer                         | 12
 * +----------------+----------------+----------------+----------------+
 * |                          frame_rate_denom                         | 16
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 20
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * frame_rate_numer - uint32_t, numerator of the frame rate.
 * frame_rate_denom - uint32_t, denominator of the frame rate.
 *
 * Notes:
 *  - to query the current (actual) frame rate use XENCAMERA_OP_CONFIG_GET
 *    request.
 *  - this request can be used with camera buffers allocated, but stream
 *    stopped, e.g. frontend is allowed to stop the stream with
 *    XENCAMERA_OP_STREAM_STOP, hold the buffers allocated (e.g. keep the
 *    configuration set with XENCAMERA_OP_CONFIG_SET), change the
 *    frame rate of the stream and (re)start the stream again with
 *    XENCAMERA_OP_STREAM_START.
 *  - frame rate cannot be changed during active streaming, e.g.
 *    after XENCAMERA_OP_STREAM_START and before XENCAMERA_OP_STREAM_STOP
 *    commands.
 */
struct xencamera_frame_rate_req {
    uint32_t frame_rate_numer;
    uint32_t frame_rate_denom;
};

/*
 * Request camera buffer's layout:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _BUF_GET_LAYOUT|   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * See response format for this request.
 *
 *
 * Request number of buffers to be used:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_BUF_REQUEST|   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |    num_bufs    |                     reserved                     | 12
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 16
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * num_bufs - uint8_t, desired number of buffers to be used.
 *
 * If num_bufs is not zero then the backend validates the requested number of
 * buffers and responds with the number of buffers allowed for this frontend.
 * Frontend is responsible for checking the corresponding response in order to
 * see if the values reported back by the backend do match the desired ones
 * and can be accepted.
 * Frontend is allowed to send multiple XENCAMERA_OP_BUF_REQUEST requests
 * before sending XENCAMERA_OP_STREAM_START request to update or tune the
 * final configuration.
 * Frontend is not allowed to change the camera configuration after this call
 * with a non-zero value of num_bufs. If camera reconfiguration is required
 * then this request must be sent with num_bufs set to zero and any created
 * buffers must be destroyed first.
 * Frontend is not allowed to change the number of buffers after the
 * streaming has started.
 *
 * If num_bufs is 0 and streaming has not started yet, then the backend will
 * free all previously allocated buffers (if any).
 * Trying to call this if streaming is in progress will result in an error.
 *
 * If camera reconfiguration is required then the streaming must be stopped
 * and this request must be sent with num_bufs set to zero and any
 * created buffers must be destroyed.
 *
 * Please note, that the number of buffers in this request must not exceed
 * the value configured in XenStore.max-buffers.
 *
 * See response format for this request.
 */
struct xencamera_buf_request {
    uint8_t num_bufs;
};

/*
 * Request camera buffer creation:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_BUF_CREATE |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |      index     |                     reserved                     | 12
 * +----------------+----------------+----------------+----------------+
 * |                           plane_offset[0]                         | 16
 * +----------------+----------------+----------------+----------------+
 * |                           plane_offset[1]                         | 20
 * +----------------+----------------+----------------+----------------+
 * |                           plane_offset[2]                         | 24
 * +----------------+----------------+----------------+----------------+
 * |                           plane_offset[3]                         | 28
 * +----------------+----------------+----------------+----------------+
 * |                           gref_directory                          | 32
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 36
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * An attempt to create multiple buffers with the same index is an error.
 * index can be re-used after destroying the corresponding camera buffer.
 *
 * index - uint8_t, index of the buffer to be created in the range
 *   from 0 to the num_bufs field returned in response for
 *   XENCAMERA_OP_BUF_REQUEST request
 * plane_offset - array of uint32_t, offset of the corresponding plane
 *   in octets from the buffer start. Number of offsets returned is
 *   equal to the value returned in XENCAMERA_OP_BUF_GET_LAYOUT.num_planes.
 * gref_directory - grant_ref_t, a reference to the first shared page
 *   describing shared buffer references. The size of the buffer is equal to
 *   XENCAMERA_OP_BUF_GET_LAYOUT.size response. At least one page exists. If
 *   shared buffer size exceeds what can be addressed by this single page,
 *   then reference to the next shared page must be supplied (see
 *   gref_dir_next_page below).
 *
 * If XENCAMERA_FIELD_BE_ALLOC configuration entry is set, then backend will
 * allocate the buffer with the parameters provided in this request and page
 * directory is handled as follows:
 *   Frontend on request:
 *     - allocates pages for the directory (gref_directory,
 *       gref_dir_next_page(s)
 *     - grants permissions for the pages of the directory to the backend
 *     - sets gref_dir_next_page fields
 *   Backend on response:
 *     - grants permissions for the pages of the buffer allocated to
 *       the frontend
 *     - fills in page directory with grant references
 *       (gref[] in struct xencamera_page_directory)
 */
struct xencamera_buf_create_req {
    uint8_t index;
    uint8_t reserved[3];
    uint32_t plane_offset[XENCAMERA_MAX_PLANE];
    grant_ref_t gref_directory;
};

/*
 * Shared page for XENCAMERA_OP_BUF_CREATE buffer descriptor (gref_directory in
 * the request) employs a list of pages, describing all pages of the shared
 * data buffer:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |                        gref_dir_next_page                         | 4
 * +----------------+----------------+----------------+----------------+
 * |                              gref[0]                              | 8
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                              gref[i]                              | i*4+8
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             gref[N - 1]                           | N*4+8
 * +----------------+----------------+----------------+----------------+
 *
 * gref_dir_next_page - grant_ref_t, reference to the next page describing
 *   page directory. Must be 0 if there are no more pages in the list.
 * gref[i] - grant_ref_t, reference to a shared page of the buffer
 *   allocated at XENCAMERA_OP_BUF_CREATE.
 *
 * Number of grant_ref_t entries in the whole page directory is not
 * passed, but instead can be calculated as:
 *   num_grefs_total = (XENCAMERA_OP_BUF_REQUEST.size + XEN_PAGE_SIZE - 1) /
 *       XEN_PAGE_SIZE
 */
struct xencamera_page_directory {
    grant_ref_t gref_dir_next_page;
    grant_ref_t gref[1]; /* Variable length */
};

/*
 * Request buffer destruction - destroy a previously allocated camera buffer:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_BUF_DESTROY|   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |      index     |                     reserved                     | 12
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 16
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * index - uint8_t, index of the buffer to be destroyed.
 *
 *
 * Request queueing of the buffer for backend use:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_BUF_QUEUE  |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |      index     |                     reserved                     | 12
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 16
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * Notes:
 *  - frontends must not access the buffer content after this request until
 *    response to XENCAMERA_OP_BUF_DEQUEUE has been received.
 *  - buffers must be queued to the backend before destroying them with
 *    XENCAMERA_OP_BUF_DESTROY.
 *
 * index - uint8_t, index of the buffer to be queued.
 *
 *
 * Request dequeueing of the buffer for frontend use:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |_OP_BUF_DEQUEUE |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |      index     |                     reserved                     | 12
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 16
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * Notes:
 *  - frontend is allowed to access the buffer content after the corresponding
 *    response to this request.
 *
 * index - uint8_t, index of the buffer to be queued.
 *
 *
 * Request camera control details:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_CTRL_ENUM  |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |      index     |                     reserved                     | 8
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 12
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * See response format for this request.
 *
 * index - uint8_t, index of the control to be queried.
 */
struct xencamera_index {
    uint8_t index;
};

/*
 * Request camera control change:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |  _OP_SET_CTRL  |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |       type     |                     reserved                     | 8
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 12
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 16
 * +----------------+----------------+----------------+----------------+
 * |                          value low 32-bit                         | 20
 * +----------------+----------------+----------------+----------------+
 * |                          value high 32-bit                        | 24
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 28
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * type - uint8_t, type of the control, one of the XENCAMERA_CTRL_XXX.
 * value - int64_t, new value of the control.
 */
struct xencamera_ctrl_value {
    uint8_t type;
    uint8_t reserved[7];
    int64_t value;
};

/*
 * Request camera control state:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |  _OP_GET_CTRL  |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |       type     |                     reserved                     | 8
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 12
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * See response format for this request.
 *
 * type - uint8_t, type of the control, one of the XENCAMERA_CTRL_XXX.
 */
struct xencamera_get_ctrl_req {
    uint8_t type;
};

/*
 * Request camera capture stream start:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |_OP_STREAM_START|   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 *
 * Request camera capture stream stop:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |_OP_STREAM_STOP |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 *
 *---------------------------------- Responses --------------------------------
 *
 * All response packets have the same length (64 octets).
 *
 * All response packets have common header:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |    operation   |    reserved    | 4
 * +----------------+----------------+----------------+----------------+
 * |                              status                               | 8
 * +----------------+----------------+----------------+----------------+
 *
 * id - uint16_t, copied from the request.
 * operation - uint8_t, XENCAMERA_OP_* - copied from request.
 * status - int32_t, response status, zero on success and -XEN_EXX on failure.
 *
 *
 * Configuration response - response for XENCAMERA_OP_CONFIG_SET,
 * XENCAMERA_OP_CONFIG_GET and XENCAMERA_OP_CONFIG_VALIDATE requests:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_CONFIG_XXX |    reserved    | 4
 * +----------------+----------------+----------------+----------------+
 * |                               status                              | 8
 * +----------------+----------------+----------------+----------------+
 * |                            pixel format                           | 12
 * +----------------+----------------+----------------+----------------+
 * |                               width                               | 16
 * +----------------+----------------+----------------+----------------+
 * |                               height                              | 20
 * +----------------+----------------+----------------+----------------+
 * |                             colorspace                            | 24
 * +----------------+----------------+----------------+----------------+
 * |                             xfer_func                             | 28
 * +----------------+----------------+----------------+----------------+
 * |                             ycbcr_enc                             | 32
 * +----------------+----------------+----------------+----------------+
 * |                            quantization                           | 36
 * +----------------+----------------+----------------+----------------+
 * |                       displ_asp_ratio_numer                       | 40
 * +----------------+----------------+----------------+----------------+
 * |                       displ_asp_ratio_denom                       | 44
 * +----------------+----------------+----------------+----------------+
 * |                          frame_rate_numer                         | 48
 * +----------------+----------------+----------------+----------------+
 * |                          frame_rate_denom                         | 52
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 56
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * Meaning of the corresponding values in this response is the same as for
 * XENCAMERA_OP_CONFIG_SET and XENCAMERA_OP_FRAME_RATE_SET requests.
 *
 * colorspace - uint32_t, this supplements pixel_format parameter,
 *   one of the XENCAMERA_COLORSPACE_XXX.
 * xfer_func - uint32_t, this supplements colorspace parameter,
 *   one of the XENCAMERA_XFER_FUNC_XXX.
 * ycbcr_enc - uint32_t, this supplements colorspace parameter,
 *   one of the XENCAMERA_YCBCR_ENC_XXX. Please note, that ycbcr_enc is only
 *   valid for YCbCr pixelformats and should be ignored otherwise.
 * quantization - uint32_t, this supplements colorspace parameter,
 *   one of the XENCAMERA_QUANTIZATION_XXX.
 * displ_asp_ratio_numer - uint32_t, numerator of the display aspect ratio.
 * displ_asp_ratio_denom - uint32_t, denominator of the display aspect ratio.
 */
struct xencamera_config_resp {
    uint32_t pixel_format;
    uint32_t width;
    uint32_t height;
    uint32_t colorspace;
    uint32_t xfer_func;
    uint32_t ycbcr_enc;
    uint32_t quantization;
    uint32_t displ_asp_ratio_numer;
    uint32_t displ_asp_ratio_denom;
    uint32_t frame_rate_numer;
    uint32_t frame_rate_denom;
};

/*
 * Request buffer response - response for XENCAMERA_OP_BUF_GET_LAYOUT
 * request:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |_BUF_GET_LAYOUT |    reserved    | 4
 * +----------------+----------------+----------------+----------------+
 * |                               status                              | 8
 * +----------------+----------------+----------------+----------------+
 * |   num_planes   |                     reserved                     | 12
 * +----------------+----------------+----------------+----------------+
 * |                                size                               | 16
 * +----------------+----------------+----------------+----------------+
 * |                           plane_size[0]                           | 20
 * +----------------+----------------+----------------+----------------+
 * |                           plane_size[1]                           | 24
 * +----------------+----------------+----------------+----------------+
 * |                           plane_size[2]                           | 28
 * +----------------+----------------+----------------+----------------+
 * |                           plane_size[3]                           | 32
 * +----------------+----------------+----------------+----------------+
 * |                          plane_stride[0]                          | 36
 * +----------------+----------------+----------------+----------------+
 * |                          plane_stride[1]                          | 40
 * +----------------+----------------+----------------+----------------+
 * |                          plane_stride[2]                          | 44
 * +----------------+----------------+----------------+----------------+
 * |                          plane_stride[3]                          | 48
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * num_planes - uint8_t, number of planes of the buffer.
 * size - uint32_t, overall size of the buffer including sizes of the
 *   individual planes and padding if applicable.
 * plane_size - array of uint32_t, size in octets of the corresponding plane
 *   including padding.
 * plane_stride - array of uint32_t, size in octets occupied by the
 *   corresponding single image line including padding if applicable.
 *
 * Note! The sizes and strides in this response apply to all buffers created
 * with XENCAMERA_OP_BUF_CREATE command, but individual buffers may have
 * different plane offsets, see XENCAMERA_OP_BUF_REQUEST.plane_offset.
 */
struct xencamera_buf_get_layout_resp {
    uint8_t num_planes;
    uint8_t reserved[3];
    uint32_t size;
    uint32_t plane_size[XENCAMERA_MAX_PLANE];
    uint32_t plane_stride[XENCAMERA_MAX_PLANE];
};

/*
 * Request buffer response - response for XENCAMERA_OP_BUF_REQUEST
 * request:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |_OP_BUF_REQUEST |    reserved    | 4
 * +----------------+----------------+----------------+----------------+
 * |                               status                              | 8
 * +----------------+----------------+----------------+----------------+
 * |   num_buffers  |                     reserved                     | 12
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 16
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * num_buffers - uint8_t, number of buffers to be used.
 *
 *
 * Control enumerate response - response for XENCAMERA_OP_CTRL_ENUM:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_CTRL_ENUM  |    reserved    | 4
 * +----------------+----------------+----------------+----------------+
 * |                               status                              | 8
 * +----------------+----------------+----------------+----------------+
 * |     index      |      type      |            reserved             | 12
 * +----------------+----------------+----------------+----------------+
 * |                               flags                               | 16
 * +----------------+----------------+----------------+----------------+
 * |                          min low 32-bits                          | 20
 * +----------------+----------------+----------------+----------------+
 * |                          min high 32-bits                         | 24
 * +----------------+----------------+----------------+----------------+
 * |                          max low 32-bits                          | 28
 * +----------------+----------------+----------------+----------------+
 * |                          max high 32-bits                         | 32
 * +----------------+----------------+----------------+----------------+
 * |                         step low 32-bits                          | 36
 * +----------------+----------------+----------------+----------------+
 * |                         step high 32-bits                         | 40
 * +----------------+----------------+----------------+----------------+
 * |                        def_val low 32-bits                        | 44
 * +----------------+----------------+----------------+----------------+
 * |                        def_val high 32-bits                       | 48
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 52
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * index - uint8_t, index of the camera control in response.
 * type - uint8_t, type of the control, one of the XENCAMERA_CTRL_XXX.
 * flags - uint32_t, flags of the control, one of the XENCAMERA_CTRL_FLG_XXX.
 * min - int64_t, minimum value of the control.
 * max - int64_t, maximum value of the control.
 * step - int64_t, minimum size in which control value can be changed.
 * def_val - int64_t, default value of the control.
 */
struct xencamera_ctrl_enum_resp {
    uint8_t index;
    uint8_t type;
    uint8_t reserved[2];
    uint32_t flags;
    int64_t min;
    int64_t max;
    int64_t step;
    int64_t def_val;
};

/*
 * Get control response - response for XENCAMERA_OP_CTRL_GET:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_CTRL_GET   |    reserved    | 4
 * +----------------+----------------+----------------+----------------+
 * |                               status                              | 8
 * +----------------+----------------+----------------+----------------+
 * |       type     |                     reserved                     | 12
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 16
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 20
 * +----------------+----------------+----------------+----------------+
 * |                          value low 32-bit                         | 24
 * +----------------+----------------+----------------+----------------+
 * |                          value high 32-bit                        | 28
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 32
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * type - uint8_t, type of the control, one of the XENCAMERA_CTRL_XXX.
 * value - int64_t, new value of the control.
 */

/*
 *----------------------------------- Events ----------------------------------
 *
 * Events are sent via a shared page allocated by the front and propagated by
 *   evt-event-channel/evt-ring-ref XenStore entries.
 *
 * All event packets have the same length (64 octets).
 * All event packets have common header:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |      type      |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 *
 * id - uint16_t, event id, may be used by front.
 * type - uint8_t, type of the event.
 *
 *
 * Frame captured event - event from back to front when a new captured
 * frame is available:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |_EVT_FRAME_AVAIL|   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |      index     |                     reserved                     | 12
 * +----------------+----------------+----------------+----------------+
 * |                              used_sz                              | 16
 * +----------------+----------------+----------------+----------------+
 * |                              seq_num                              | 20
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 24
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * index - uint8_t, index of the buffer that contains new captured frame,
 *   see XENCAMERA_OP_BUF_CREATE description on the range
 * used_sz - uint32_t, number of octets this frame has. This can be less
 * than the XENCAMERA_OP_BUF_REQUEST.size (response) for compressed formats.
 * seq_num - uint32_t, sequential number of the frame. Must be
 *   monotonically increasing. If skips are detected in seq_num then that
 *   means that the frames in-between were dropped. Note however that not
 *   all video capture hardware is capable of detecting dropped frames.
 *   In that case there will be no skips in the sequence counter.
 */
struct xencamera_frame_avail_evt {
    uint8_t index;
    uint8_t reserved[3];
    uint32_t used_sz;
    uint32_t seq_num;
};

/*
 * Control change event- event from back to front when camera control
 * has changed:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |_EVT_CTRL_CHANGE|   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |       type     |                     reserved                     | 8
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 12
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 16
 * +----------------+----------------+----------------+----------------+
 * |                          value low 32-bit                         | 20
 * +----------------+----------------+----------------+----------------+
 * |                          value high 32-bit                        | 24
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 28
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * type - uint8_t, type of the control, one of the XENCAMERA_CTRL_XXX.
 * value - int64_t, new value of the control.
 *
 * Notes:
 *  - this event is not sent for write-only controls
 *  - this event is not sent to the originator of the control change
 *  - this event is not sent when frontend first connects, e.g. initial
 *    control state must be explicitly queried
 */

struct xencamera_req {
    uint16_t id;
    uint8_t operation;
    uint8_t reserved[5];
    union {
        struct xencamera_config_req config;
        struct xencamera_frame_rate_req frame_rate;
        struct xencamera_buf_request buf_request;
        struct xencamera_buf_create_req buf_create;
        struct xencamera_index index;
        struct xencamera_ctrl_value ctrl_value;
        struct xencamera_get_ctrl_req get_ctrl;
        uint8_t reserved[56];
    } req;
};

struct xencamera_resp {
    uint16_t id;
    uint8_t operation;
    uint8_t reserved;
    int32_t status;
    union {
        struct xencamera_config_resp config;
        struct xencamera_buf_get_layout_resp buf_layout;
        struct xencamera_buf_request buf_request;
        struct xencamera_ctrl_enum_resp ctrl_enum;
        struct xencamera_ctrl_value ctrl_value;
        uint8_t reserved1[56];
    } resp;
};

struct xencamera_evt {
    uint16_t id;
    uint8_t type;
    uint8_t reserved[5];
    union {
        struct xencamera_frame_avail_evt frame_avail;
        struct xencamera_ctrl_value ctrl_value;
        uint8_t reserved[56];
    } evt;
};

DEFINE_RING_TYPES(xen_cameraif, struct xencamera_req, struct xencamera_resp);

/*
 ******************************************************************************
 *                        Back to front events delivery
 ******************************************************************************
 * In order to deliver asynchronous events from back to front a shared page is
 * allocated by front and its granted reference propagated to back via
 * XenStore entries (evt-ring-ref/evt-event-channel).
 * This page has a common header used by both front and back to synchronize
 * access and control event's ring buffer, while back being a producer of the
 * events and front being a consumer. The rest of the page after the header
 * is used for event packets.
 *
 * Upon reception of an event(s) front may confirm its reception
 * for either each event, group of events or none.
 */

struct xencamera_event_page {
    uint32_t in_cons;
    uint32_t in_prod;
    uint8_t reserved[56];
};

#define XENCAMERA_EVENT_PAGE_SIZE 4096
#define XENCAMERA_IN_RING_OFFS (sizeof(struct xencamera_event_page))
#define XENCAMERA_IN_RING_SIZE (XENCAMERA_EVENT_PAGE_SIZE - XENCAMERA_IN_RING_OFFS)
#define XENCAMERA_IN_RING_LEN (XENCAMERA_IN_RING_SIZE / sizeof(struct xencamera_evt))
#define XENCAMERA_IN_RING(page) \
    ((struct xencamera_evt *)((char *)(page) + XENCAMERA_IN_RING_OFFS))
#define XENCAMERA_IN_RING_REF(page, idx) \
    (XENCAMERA_IN_RING((page))[(idx) % XENCAMERA_IN_RING_LEN])

#endif /* __XEN_PUBLIC_IO_CAMERAIF_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
