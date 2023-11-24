#ifndef __XEN_DEVICE_TREE_DEFS_H__
#define __XEN_DEVICE_TREE_DEFS_H__

#if defined(__XEN__) || defined(__XEN_TOOLS__)
/*
 * The device tree compiler (DTC) is allocating the phandle from 1 to
 * onwards. Reserve a high value for the GIC phandle.
 */
#define GUEST_PHANDLE_GIC (65000)

#define GUEST_ROOT_ADDRESS_CELLS 2
#define GUEST_ROOT_SIZE_CELLS 2

/**
 * IRQ line type.
 *
 * DT_IRQ_TYPE_NONE            - default, unspecified type
 * DT_IRQ_TYPE_EDGE_RISING     - rising edge triggered
 * DT_IRQ_TYPE_EDGE_FALLING    - falling edge triggered
 * DT_IRQ_TYPE_EDGE_BOTH       - rising and falling edge triggered
 * DT_IRQ_TYPE_LEVEL_HIGH      - high level triggered
 * DT_IRQ_TYPE_LEVEL_LOW       - low level triggered
 * DT_IRQ_TYPE_LEVEL_MASK      - Mask to filter out the level bits
 * DT_IRQ_TYPE_SENSE_MASK      - Mask for all the above bits
 * DT_IRQ_TYPE_INVALID         - Use to initialize the type
 */
#define DT_IRQ_TYPE_NONE           0x00000000
#define DT_IRQ_TYPE_EDGE_RISING    0x00000001
#define DT_IRQ_TYPE_EDGE_FALLING   0x00000002
#define DT_IRQ_TYPE_EDGE_BOTH                           \
    (DT_IRQ_TYPE_EDGE_FALLING | DT_IRQ_TYPE_EDGE_RISING)
#define DT_IRQ_TYPE_LEVEL_HIGH     0x00000004
#define DT_IRQ_TYPE_LEVEL_LOW      0x00000008
#define DT_IRQ_TYPE_LEVEL_MASK                          \
    (DT_IRQ_TYPE_LEVEL_LOW | DT_IRQ_TYPE_LEVEL_HIGH)
#define DT_IRQ_TYPE_SENSE_MASK     0x0000000f

#define DT_IRQ_TYPE_INVALID        0x00000010

#endif

#endif
