CFLAGS=	-I../../../../sys/ofed/include -libverbs -lmlx4 -lmthca -pthread

all:	asyncwatch devinfo device_list rc_pingpong srq_pingpong uc_pingpong ud_pingpong

clean:
	rm asyncwatch devinfo device_list rc_pingpong srq_pingpong uc_pingpong ud_pingpong

asyncwatch:
	gcc -o asyncwatch asyncwatch.c ${CFLAGS}

devinfo:
	gcc -o devinfo devinfo.c ${CFLAGS}

device_list:
	gcc -o device_list device_list.c  ${CFLAGS}

rc_pingpong:
	gcc -o rc_pingpong rc_pingpong.c pingpong.c ${CFLAGS}

srq_pingpong:
	gcc -o srq_pingpong srq_pingpong.c pingpong.c ${CFLAGS}

uc_pingpong:
	gcc -o uc_pingpong uc_pingpong.c pingpong.c ${CFLAGS}

ud_pingpong:
	gcc -o ud_pingpong ud_pingpong.c pingpong.c ${CFLAGS}

