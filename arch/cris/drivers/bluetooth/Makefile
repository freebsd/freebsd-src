include $(AXIS_TOP_DIR)/tools/build/Rules.axis

all:

install:	src/bluetooth.c include/btcommon.h
	ln -sfn ../../arch/cris/drivers/bluetooth/include ../../../../include/linux/bluetooth
	(cd .. && patch -N -r /tmp/bt.rej < bluetooth/bt.patch >/dev/null || true)

clean:

src/bluetooth.c:
	@echo "You must install the OpenBT src directory before install can be done here!"
	@exit 1

include/btcommon.h:
	@echo "You must install the OpenBT include directory before install can be done here!"
	@exit 1
