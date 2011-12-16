EXTRA_CFLAGS += -Idrivers/infiniband/include
EXTRA_CFLAGS += -ggdb

obj-$(CONFIG_INFINIBAND_SDP) += ib_sdp.o

ib_sdp-objs := sdp_main.o sdp_cma.o sdp_bcopy.o sdp_proc.o sdp_tx.o sdp_rx.o sdp_zcopy.o
