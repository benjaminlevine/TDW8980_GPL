

DEFINES= -DRESOLVE_HOSTNAMES

#options if you have a bind>=4.9.4 libresolv (or, maybe, glibc)
LDLIBS=-lresolv
ADDLIB=

#options for decnet
ADDLIB+=dnet_ntop.o dnet_pton.o

#options for ipx
ADDLIB+=ipx_ntop.o ipx_pton.o

HOSTCC = gcc
#CC     = /opt/trendchip/mips-linux26/bin/mips-linux-gcc
#CC	   = mips-linux-gcc	
CCOPTS = -D_GNU_SOURCE -O2 -Wstrict-prototypes -Wall
CFLAGS = $(CCOPTS) -I../include $(DEFINES)

LDLIBS += -L../lib -lnetlink -lutil

SUBDIRS=lib ip tc #misc #netem genl

LIBNETLINK=../lib/libnetlink.a 
LIBUTIL=../lib/libutil.a

all: Config
	@set -e; \
	for i in $(SUBDIRS); \
	do $(MAKE) $(MFLAGS) -C $$i; done

Config:
	sh configure $(KERNEL_INCLUDE)

snapshot:
	echo "static const char SNAPSHOT[] = \""`date +%y%m%d`"\";" \
		> include/SNAPSHOT.h

romfs:
	@for i in $(SUBDIRS); \
	do $(MAKE) $(MFLAGS) -C $$i romfs; done

clean:
	@for i in $(SUBDIRS) doc; \
	do $(MAKE) $(MFLAGS) -C $$i clean; done

clobber: clean
	rm -f Config

distclean: clobber


.EXPORT_ALL_VARIABLES:
