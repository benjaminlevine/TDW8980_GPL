#Makefile_GPL_NetUSB_Module

obj-m += GPL_NetUSB.o
GPL_NetUSB-objs := GPL_map_NetUSB.o

ifeq ($(KDIR), )
KDIR    := ../../build_dir/linux-ltqcpe_2_6_32_LANTIQ/linux-2.6.32.32
endif

#LINUX_SRC = /usr/src/linux
#ARCH = 

SRC_TO_TOP = GPL_NetUSB.ko Module.symvers

all: 
	make -C $(KDIR) SUBDIRS=$(PWD) modules $(KC_COMPILE_ARG)  EXTRA_CFLAGS=$(KC_EXTRA_CFLAGS)
	@test $(KC_TOP) && cp -f $(SRC_TO_TOP) $(KC_TOP) || echo "KC_TOP is NULL"


clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean 
