#**************************************************************************** 
# 
#  Copyright(c) 2010-2011 Shenzhen TP-LINK Technologies Co.Ltd. 
#  All Rights Reserved 
#  YangXv <yangxu@tp-link.com.cn> 
# 
#***************************************************************************

# Top-level Makefile for all linux platforms 

MODEL = W8980V1

include config/$(MODEL).config

ifeq ($(strip $(INCLUDE_CPU_VR268)),y)
CPU_TYPE=vr268
include config/config.wifi
endif

#define basic variable
TOP_PATH = $(shell pwd)/..
BUILD_PATH = $(TOP_PATH)/build
TOOLS_PATH = $(TOP_PATH)/host_tools
FAKEROOT_PATH = $(TOOLS_PATH)/fakeroot

TARGETS_PATH = $(TOP_PATH)/targets
PUBLIC_APPS_PATH = $(TOP_PATH)/apps/public
PRIVATE_APPS_PATH = $(TOP_PATH)/apps/private
CMM_APPS_PATH = $(PRIVATE_APPS_PATH)/user/
OS_LIB_PATH = $(PUBLIC_APPS_PATH)/os_libs
	
####################################################
#
#Get supplier         
#
####################################################	

ifeq ($(strip $(CPU_TYPE)),vr268)
SUPPLIER = lantiq
TOOLPREFIX = mips-linux-uclibc-
HOST = mips-linux
KERNELVERSION = 2.6.32.32
OBJ_DIR = lantiq_4.3.3
SLIB_PATH = $(TOP_PATH)/$(SUPPLIER)/build_dir/target-mips_r2_uClibc-0.9.30.1_2_6_32_LANTIQ/root-ltqcpe/orilib
KERNELPATH = $(TOP_PATH)/$(SUPPLIER)/build_dir/linux-ltqcpe_2_6_32_LANTIQ/linux-2.6.32.32
ROOTFSTOOLS = $(TOP_PATH)/$(SUPPLIER)/staging_dir/host/bin/mksquashfs4
export PATH :=$(TOP_PATH)/$(SUPPLIER)/staging_dir/toolchain-mips_r2_gcc-4.3.3+cs_uClibc-0.9.30.1_2_6_32/usr/bin:$(PATH)
export PATH :=$(TOP_PATH)/$(SUPPLIER)/staging_dir/host/bin:$(PATH)
export KERNELARCH = mips
export MAKEARCH = $(MAKE) ARCH=$(KERNELARCH) CROSS_COMPILE=$(TOOLPREFIX)
export ATH_CARR_DIR = lantiq
export INSTALL_WIRELESS = $(TOP_PATH)/$(SUPPLIER)/wireless
export MODULEPATH = $(INSTALL_WIRELESS)/lib/modules/
export ATHEROSPATH=$(TOP_PATH)/$(SUPPLIER)/modules/wlan
export WAVE300_DIR = $(TOP_PATH)/lantiq/modules/wave300
endif

####################################################
#
#Supplier PATH variable        
#
####################################################	

INSTALL_MOD_PATH = $(KERNELPATH)/../
SUPPLIER_TOOLS = $(TOP_PATH)/$(SUPPLIER)/tools

#TG means target
BOOT_TG_PATH = $(TARGETS_PATH)/$(SUPPLIER)_boot
KERNEL_TG_PATH = $(TARGETS_PATH)/$(SUPPLIER)_$(CPU_TYPE)_kernel
MODULE_TG_PATH = $(TARGETS_PATH)/$(SUPPLIER)_$(CPU_TYPE)_modules
MODEL_FS_PATH = $(TARGETS_PATH)/fs.$(MODEL)

####################################################
#
#Supplier compiler variable        
#
####################################################	

CC = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
AR = $(TOOLPREFIX)ar
STRIP = $(TOOLPREFIX)strip
RANLIB = $(TOOLPREFIX)ranlib

####################################################
#
#Function conditional compilation        
#
####################################################	

DFLAGS = 
WEBFLAGS = 
DF_FLAGS = 

ifeq ($(INCLUDE_WATCHDOG), y) 
DFLAGS += -DINCLUDE_WATCHDOG
endif 

ifeq ($(INCLUDE_PPA_ACCELERATE), y)
DFLAGS += -DINCLUDE_PPA_ACCELERATE
endif

#################### ETH Type ######################
ifeq ($(INCLUDE_ETH_LTQ), y) 
DFLAGS += -DINCLUDE_ETH_LTQ
endif 

ifeq ($(INCLUDE_ETH_RA), y) 
DFLAGS += -DINCLUDE_ETH_RA
endif 

###################### WIFI  ########################
ifeq ($(INCLUDE_LAN_WLAN), y) 
DFLAGS += -DINCLUDE_LAN_WLAN 
endif 

ifeq ($(INCLUDE_WLAN_RA), y) 
DFLAGS += -DINCLUDE_WLAN_RA
endif 

ifeq ($(INCLUDE_WLAN_RTL), y) 
DFLAGS += -DINCLUDE_WLAN_RTL
endif 

ifeq ($(INCLUDE_WLAN_ATH), y) 
DFLAGS += -DINCLUDE_WLAN_ATH
endif 

ifeq ($(INCLUDE_LAN_WLAN_MSSID), y) 
DFLAGS += -DINCLUDE_LAN_WLAN_MSSID 
endif 

ifeq ($(INCLUDE_LAN_WLAN_WDS), y) 
DFLAGS += -DINCLUDE_LAN_WLAN_WDS
endif 

#################### Function ######################

ifeq ($(INCLUDE_BRIDGING), y)
DFLAGS += -DINCLUDE_BRIDGING
endif

ifeq ($(INCLUDE_ADSLWAN), y)
DFLAGS += -DINCLUDE_ADSLWAN
endif

ifeq ($(INCLUDE_ADSL_RA), y)
DFLAGS += -DINCLUDE_ADSL_RA
endif

ifeq ($(INCLUDE_ADSL_RTL), y)
DFLAGS += -DINCLUDE_ADSL_RTL
endif

ifeq ($(INCLUDE_ADSL_LTQ), y)
DFLAGS += -DINCLUDE_ADSL_LTQ
endif

ifeq ($(INCLUDE_ETHERNET_WAN), y)
DFLAGS += -DINCLUDE_ETHERNET_WAN
endif

ifeq ($(INCLUDE_IGMP), y)
DFLAGS += -DINCLUDE_IGMP
endif

ifeq ($(INCLUDE_IPTV), y)
DFLAGS += -DINCLUDE_IPTV
endif

ifeq ($(INCLUDE_AUTO_PVC), y)
DFLAGS += -DINCLUDE_AUTO_PVC
endif

ifeq ($(INCLUDE_RIP), y)
DFLAGS += -DINCLUDE_RIP
endif

ifeq ($(INCLUDE_SNMP), y)
DFLAGS += -DINCLUDE_SNMP
endif

ifeq ($(INCLUDE_SSL), y)
DFLAGS += -DINCLUDE_SSL
endif

ifeq ($(INCLUDE_CWMP), y)
DFLAGS += -DINCLUDE_CWMP
endif

ifeq ($(INCLUDE_CWMP_SSL), y)
DFLAGS += -DINCLUDE_CWMP_SSL
export INCLUDE_CWMP_SSL
endif

ifeq ($(INCLUDE_DIGITAL_SIGNATURE), y)
DFLAGS += -DINCLUDE_DIGITAL_SIGNATURE
endif

ifeq ($(INCLUDE_DDNS), y)
DFLAGS += -DINCLUDE_DDNS
endif

ifeq ($(INCLUDE_DDNS_PH), y)
DFLAGS += -DINCLUDE_DDNS_PH
endif

ifeq ($(INCLUDE_DYNDNS), y)
DFLAGS += -DINCLUDE_DYNDNS
endif

ifeq ($(INCLUDE_NOIPDNS), y)
DFLAGS += -DINCLUDE_NOIPDNS
endif

ifeq ($(INCLUDE_ALG), y)
DFLAGS += -DINCLUDE_ALG
endif

ifeq ($(INCLUDE_ALG_H323), y)
DFLAGS += -DINCLUDE_ALG_H323
endif

ifeq ($(INCLUDE_ALG_SIP), y)
DFLAGS += -DINCLUDE_ALG_SIP
endif

#NOTE:INCLUDE_VOIP in voip.mak
ifeq ($(INCLUDE_VOIP), y)
DFLAGS += -DINCLUDE_VOIP -DCPU_TYPE=$(CPU_TYPE)
DF_FLAGS += $(VOIP_CFLAGS)
endif

ifeq ($(INCLUDE_IPV6), y)
DFLAGS += -DINCLUDE_IPV6

ifeq ($(INCLUDE_IPV6_SLAAC), y)
DFLAGS += -DINCLUDE_IPV6_SLAAC
endif
ifeq ($(INCLUDE_IPV6_HTTP), y)
DFLAGS += -DINCLUDE_IPV6_HTTP
endif

endif

ifeq ($(INCLUDE_USB), y)
DFLAGS += -DINCLUDE_USB
endif

ifeq ($(INCLUDE_USB_OVER_IP), y)
DFLAGS += -DINCLUDE_USB_OVER_IP
endif

ifeq ($(INCLUDE_USB_STORAGE), y)
DFLAGS += -DINCLUDE_USB_STORAGE
endif

ifeq ($(INCLUDE_USB_MEDIA_SERVER), y)
DFLAGS += -DINCLUDE_USB_MEDIA_SERVER
endif

ifeq ($(INCLUDE_USB_SAMBA_SERVER), y)
DFLAGS += -DINCLUDE_USB_SAMBA_SERVER
endif

ifeq ($(INCLUDE_USB_FTP_SERVER), y)
DFLAGS += -DINCLUDE_USB_FTP_SERVER
endif

ifeq ($(INCLUDE_USB_3G_DONGLE), y)
DFLAGS += -DINCLUDE_USB_3G_DONGLE
endif

ifeq ($(INCLUDE_IPSEC), y)
DFLAGS += -DINCLUDE_IPSEC
endif

ifeq ($(INCLUDE_WAN_MODE), y)
DFLAGS += -DINCLUDE_WAN_MODE
endif

ifeq ($(INCLUDE_QOS), y)
DFLAGS += -DINCLUDE_QOS
endif

DF_FLAGS += $(DFLAGS)



WEBFLAGS += WEB_INCLUDE_TEST=0

ifeq ($(WEB_INCLUDE_MULTI_EWAN), y)
WEBFLAGS += WEB_INCLUDE_MULTI_EWAN=1
else
WEBFLAGS += WEB_INCLUDE_MULTI_EWAN=0
endif

####################################################
#
#Export some variable        
#
####################################################	

export TOOLPREFIX
export HOST
#For modules build & br2684ctl build
export KERNELPATH
#For install kernel modules
export INSTALL_MOD_PATH
#For code control, placed different target file
export OBJ_DIR
#For function conditional compilation
export DF_FLAGS

export TOP_PATH

export CMM_APPS_PATH

export OS_LIB_PATH

#For complier
export CC
export LD
export AR
export STRIP
export RANLIB

####################################################
#
#Menuconfig         
#
####################################################
menuconfig:
	cp $(BUILD_PATH)/config/$(MODEL).config $(BUILD_PATH)/.config
	$(BUILD_PATH)/scripts/config/mconf $(BUILD_PATH)/sysdeps/linux/Config.in
	cp $(BUILD_PATH)/.config $(BUILD_PATH)/config/$(MODEL).config
	@if egrep "^INCLUDE_CPU_VR268=y" .config > /dev/null; then \
		cd $(TOP_PATH)/lantiq && $(MAKE) menuconfig;\
	fi
	
####################################################
#
#Build bootloader          
#
####################################################
boot_build:
	@echo "Make Boot Build"

ifeq ($(strip $(SUPPLIER)),lantiq)
	cd $(TOP_PATH)/$(SUPPLIER)/build_dir/linux-ltqcpe_2_6_32_LANTIQ/u-boot-2010.06;chmod a+x ./scripts_platform/*;make clean distclean;make vr9_config; grep 'CONFIG_UBOOT_' $(TOP_PATH)/$(SUPPLIER)/.config | sed -e "s,\\(# \)\\?CONFIG_UBOOT_\\(.*\\),\\1\\2,g" > $(TOP_PATH)/$(SUPPLIER)/build_dir/linux-ltqcpe_2_6_32_LANTIQ/u-boot-2010.06/.config; make oldconfig;make clean all CROSS_COMPILE=mips-linux-uclibc-
	test -d $(BOOT_TG_PATH) || mkdir -p $(BOOT_TG_PATH)
	cp $(TOP_PATH)/$(SUPPLIER)/build_dir/linux-ltqcpe_2_6_32_LANTIQ/u-boot-2010.06/u-boot.lq $(BOOT_TG_PATH)/boot.bin
endif

####################################################
#
#Build linux kernel           
#
####################################################
kernel_build:
	@echo "Make Kernel Build"
	cp $(KERNELPATH)/$(CPU_TYPE)_config $(KERNELPATH)/.config
	cd $(KERNELPATH) && $(MAKE) V=2 linux.7z
	test -d $(KERNEL_TG_PATH) || mkdir -p $(KERNEL_TG_PATH)
	cp $(KERNELPATH)/linux.7z $(KERNEL_TG_PATH)/
	cp $(KERNELPATH)/vmlinux $(KERNEL_TG_PATH)/

kernel_menuconfig:
	cp $(KERNELPATH)/$(CPU_TYPE)_config $(KERNELPATH)/.config
	cd $(KERNELPATH) && $(MAKE) menuconfig
	cp $(KERNELPATH)/.config $(KERNELPATH)/$(CPU_TYPE)_config
	@echo "Replace Kernel Config File"

####################################################
#
#Build all kernel modules
#
####################################################
modules_build:kernel_modules misc_modules

kernel_modules:
	@echo "Making Kernel Modules"
	cp $(KERNELPATH)/$(CPU_TYPE)_config $(KERNELPATH)/.config
	cd $(KERNELPATH) && $(MAKE) modules
	cd $(KERNELPATH) && $(MAKE) DEPMOD=true modules_install
	test -d $(MODULE_TG_PATH) || mkdir -p $(MODULE_TG_PATH) && mkdir -p $(MODULE_TG_PATH)/kmdir/
	cp -pR $(INSTALL_MOD_PATH)/lib/modules/$(KERNELVERSION)/* $(MODULE_TG_PATH)/kmdir/

misc_modules:
	cd $(TOP_PATH)/$(SUPPLIER)/modules/GPL_NetUSB && $(MAKE) 

####################################################
#
#Build libs
#
####################################################

####################################################
#
#Build Lantiq wireless
#
####################################################
lqwlan_build:
	@echo making Lantiq wlan
	cd $(TOP_PATH)/lantiq/modules/wave300/lq-wave-300-03.01.00.12.31.09.58ada131737d.src && \
	./support/cfghlpr.sh .config  && make all 
	cp $(TOP_PATH)/lantiq/modules/wave300/lq-wave-300-03.01.00.12.31.09.58ada131737d.src/builds/ugw5.1-vrx288-nand/binaries/driver/mtlk.ko $(MODULEPATH)

lqhostapd_build:
	@echo making Lantiq hostapd
	cd $(TOP_PATH)/lantiq/modules/wave300/hostapd-lantiq; make clean; make all 
	cp -f $(TOP_PATH)/lantiq/modules/wave300/hostapd-lantiq/hostapd/hostapd $(INSTALL_WIRELESS)/sbin/hostapd_wlan0 

####################################################
#
#Build wireless, include driver and tools
#
####################################################
wireless_tools:
	@echo making wireless_tools
	if test -n "$(WIRELESSTOOLS)"; then \
	cd $(TOP_PATH)/$(SUPPLIER)/modules/$(WIRELESSTOOLS); make clean; make; \
	cp -f $(WIRELESSTOOLSLIB) $(INSTALL_WIRELESS)/lib; \
	cp -f iwpriv $(INSTALL_WIRELESS)/sbin; \
	cp -f iwconfig $(INSTALL_WIRELESS)/sbin; \
	cp -f iwlist $(INSTALL_WIRELESS)/sbin; \
	fi

wpa2:wpa2_clean
	@echo Making wpa2 at 'date'
	cd $(TOP_PATH)/$(SUPPLIER)/modules/wpa2 && $(MAKE) \
		CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld
	@echo End Making wpa2 at 'date'
	cd $(TOP_PATH)/$(SUPPLIER)/modules/wpa2/common && \
	$(STRIP) *.so && cp *.so $(INSTALL_WIRELESS)/lib
	cd $(TOP_PATH)/$(SUPPLIER)/modules/wpa2/hostapd && \
	$(STRIP) hostapd && cp hostapd $(INSTALL_WIRELESS)/sbin
	cd $(TOP_PATH)/$(SUPPLIER)/modules/wpa2/wpa_supplicant && \
	$(STRIP) wpa_supplicant && cp wpa_supplicant $(INSTALL_WIRELESS)/sbin
	cd $(TOP_PATH)/$(SUPPLIER)/modules/wpa2/wpatalk && \
	$(STRIP) wpatalk && cp wpatalk $(INSTALL_WIRELESS)/sbin
	
ifeq ($(strip $(SUPPLIER)),lantiq)
wlan_build: wireless_tools wpa2 lqwlan_build lqhostapd_build
endif

wpa2_clean:
	cd $(TOP_PATH)/$(SUPPLIER)/modules/wpa2 && $(MAKE) clean
	
wlan_clean:wpa2_clean
	make -C $(TOP_PATH)/$(SUPPLIER)/modules/$(WIRELESSTOOLS) clean
	
####################################################
#
#Build apps
#
####################################################

#openssl should before pjsip case pjsip need it
#libusb&libcompat should before handle_card&usb_modeswitch
.PHONY: apps_build
apps_build: libusb libcompat openssl public_apps_build


####################################################
#
#Build public apps
#
####################################################

.PHONY: public_apps_build
public_apps_build:  oslibs dhcp busybox tc iptables ebtables pppd ripd  \
					upnplib upnp flex radvd dhcpv6 samba ntfs-3g arpd \
					vsftp ffmpeg libdlna ushare ip ipsectools \
					chat usb_modeswitch

.PHONY: oslibs
oslibs:
	cd $(PUBLIC_APPS_PATH)/os_libs && $(MAKE) 
					
.PHONY: dhcp
dhcp:
	cd $(PUBLIC_APPS_PATH)/dhcp && $(MAKE) 
	
.PHONY: busybox
busybox:
	cd $(PUBLIC_APPS_PATH)/busybox-1.19.2 && $(MAKE)
	
.PHONY: tc
tc:
	cd $(PUBLIC_APPS_PATH)/iproute2-2.6.24-rc7 && $(MAKE)

.PHONY: iptables
iptables:
	cd $(PUBLIC_APPS_PATH)/iptables-1.4.0rc1 && $(MAKE) INCLUDE_IPV6=$(INCLUDE_IPV6)

.PHONY: ebtables
ebtables:
	#cd $(PUBLIC_APPS_PATH)/ebtables-v2.0.6 && $(MAKE)
	cd $(PUBLIC_APPS_PATH)/ebtables-v2.0.10-4 && $(MAKE) static
	
.PHONY: pppd
pppd:
	cd $(PUBLIC_APPS_PATH)/ppp-2.4.5/pppd && $(MAKE) INCLUDE_IPV6_PPPV6=$(INCLUDE_IPV6_PPPV6)
	
.PHONY: br2684ctl
br2684ctl:
	cd $(PUBLIC_APPS_PATH)/br2684ctl && $(MAKE)
	
.PHONY: arpd
arpd:
	cd $(PUBLIC_APPS_PATH)/linux-atm && $(MAKE)
	
.PHONY: upnplib
upnplib:
	$(MAKE) -C $(PUBLIC_APPS_PATH)/upnp_libs
	
.PHONY: upnp
upnp:
	$(MAKE) -C $(PUBLIC_APPS_PATH)/IGD

.PHONY: ripd
ripd:
ifeq ($(INCLUDE_RIP), y)
	$(MAKE) -C $(PUBLIC_APPS_PATH)/zebra-0.95a
else
	@echo no ripd
endif

.PHONY: vsftp
vsftp:
ifeq ($(INCLUDE_USB_FTP_SERVER), y)
	cd $(PUBLIC_APPS_PATH)/vsftpd-2.3.2 && $(MAKE)
else 
	@echo no vsftp
endif

.PHONY: flex
flex:
ifeq ($(INCLUDE_IPV6_FLEX), y)
	cd $(PUBLIC_APPS_PATH)/flex-2.5.35 && test -e Makefile || ./configure --host=mips-linux 
	cd $(PUBLIC_APPS_PATH)/flex-2.5.35 && $(MAKE)
	mkdir -p $(PUBLIC_APPS_PATH)/lib
	cp -p $(PUBLIC_APPS_PATH)/flex-2.5.35/libfl.a $(PUBLIC_APPS_PATH)/lib
else 
	@echo no ipv6 flex
endif

.PHONY: radvd
radvd:
ifeq ($(INCLUDE_IPV6_RADVD), y)
	cd $(PUBLIC_APPS_PATH)/radvd-1.5 && $(MAKE)
else 
	@echo no ipv6 radvd
endif

.PHONY: dhcpv6
dhcpv6:
ifeq ($(INCLUDE_IPV6_DHCPV6), y)
	cd $(PUBLIC_APPS_PATH)/wide-dhcpv6-20080615 && $(MAKE)
else 
	@echo no ipv6 dhcpv6
endif

.PHONY: ip
ip: 
ifeq ($(INCLUDE_IPV6_TUNNEL), y)
	cd $(PUBLIC_APPS_PATH)/iproute2-2.6.39 && $(MAKE)
else
	@echo no ipv6 ip
endif

#for samba and ntfs-3g, zjj, 20111129
.PHONY: samba
samba:
ifeq ($(INCLUDE_USB_SAMBA_SERVER), y)
	cd $(PUBLIC_APPS_PATH)/samba-3.0.14a/source && $(MAKE)
	$(STRIP) $(PUBLIC_APPS_PATH)/samba-3.0.14a/source/bin/*
else 
	@echo no samba
endif
	
.PHONY: ntfs-3g
ntfs-3g:
ifeq ($(INCLUDE_USB_STORAGE), y)
	cd $(PUBLIC_APPS_PATH)/ntfs-3g-1.2918 && $(MAKE) # modified by zjj, 20120723, have modified the Makefile for ntfs-3g
	$(STRIP) $(PUBLIC_APPS_PATH)/ntfs-3g-1.2918/libntfs-3g/.libs/libntfs-3g.so.38.0.0
	$(STRIP) $(PUBLIC_APPS_PATH)/ntfs-3g-1.2918/src/.libs/ntfs-3g
else 
	@echo no ntfs-3g
endif
#end by zjj

#for dlna, LI CHENGLONG
.PHONY: ffmpeg
ffmpeg:
ifeq ($(INCLUDE_USB_MEDIA_SERVER), y)
	$(MAKE) -C $(PUBLIC_APPS_PATH)/ffmpeg-0.6/
endif

.PHONY: libdlna
libdlna:
ifeq ($(INCLUDE_USB_MEDIA_SERVER), y)
	$(MAKE) -C $(PUBLIC_APPS_PATH)/libdlna-0.2.3/
endif

.PHONY: ushare
ushare:
ifeq ($(INCLUDE_USB_MEDIA_SERVER), y)
	$(MAKE) -C $(PUBLIC_APPS_PATH)/ushare-1.1a/
endif
#end by LI CHENGLONG

.PHONY: openssl
openssl:
ifeq ($(INCLUDE_SSL), y)
	$(MAKE) SHARED_LDFLAGS="-s" -C $(PUBLIC_APPS_PATH)/openssl/
	cd $(PUBLIC_APPS_PATH)/openssl && mkdir -p ./lib && cp -p ./libcrypto.so.0.9.7 ./lib/ && cp -p ./libssl.so.0.9.7 ./lib/ \
	&& cd ./lib/ && ln -sf libcrypto.so.0.9.7 libcrypto.so.0 && ln -sf libcrypto.so.0.9.7 libcrypto.so && ln -sf libssl.so.0.9.7 libssl.so.0 && ln -sf libssl.so.0.9.7 libssl.so
endif

.PHONY: ipsectools
ipsectools:
ifeq ($(INCLUDE_IPSEC), y)
	cd $(PUBLIC_APPS_PATH)/ipsectools && test -e Makefile || ./configure --host=mips-linux --with-kernel-headers=$(KERNELPATH)/include \
	--with-openssl=$(PUBLIC_APPS_PATH)/openssl/ --enable-security-context=no --enable-shared=yes \
	--enable-dpd=yes CFLAGS="-Os -DGNU" 
	cd $(PUBLIC_APPS_PATH)/ipsectools && $(MAKE)
	$(STRIP) $(PUBLIC_APPS_PATH)/ipsectools/src/racoon/.libs/racoon
	$(STRIP) $(PUBLIC_APPS_PATH)/ipsectools/src/setkey/.libs/setkey
	$(STRIP) $(PUBLIC_APPS_PATH)/ipsectools/src/racoon/.libs/libracoon.so.0.0.0
	$(STRIP) $(PUBLIC_APPS_PATH)/ipsectools/src/libipsec/.libs/libipsec.so.0.0.1
endif

.PHONY: chat
chat:
ifeq ($(INCLUDE_USB_3G_DONGLE), y)
	@echo building chat...
	cd $(PUBLIC_APPS_PATH)/ppp-2.4.5/chat && $(MAKE)
	$(STRIP) $(PUBLIC_APPS_PATH)/ppp-2.4.5/chat/chat
else
	@echo no chat
endif

.PHONY: libusb
libusb:
ifeq ($(INCLUDE_USB_3G_DONGLE), y)
	echo "Building usb library"
	cd $(PUBLIC_APPS_PATH)/libusb-1.0.8 && test -e Makefile || ./configure --build=i686-linux --host=$(HOST) --prefix=$(PUBLIC_APPS_PATH)/libusb-1.0.8/install
	cd $(PUBLIC_APPS_PATH)/libusb-1.0.8 && $(MAKE) install
else
	@echo no libusb
endif

.PHONY: libcompat
libcompat:
ifeq ($(INCLUDE_USB_3G_DONGLE), y)
	echo "Building usb compat"
	cd $(PUBLIC_APPS_PATH)/libusb-compat-0.1.3 && test -e Makefile || ./configure --build=i686-linux --host=$(HOST) --prefix=$(PUBLIC_APPS_PATH)/libusb-compat-0.1.3/install LIBUSB_1_0_CFLAGS=-I$(PUBLIC_APPS_PATH)/libusb-1.0.8/install/include/libusb-1.0 LIBUSB_1_0_LIBS=-L$(PUBLIC_APPS_PATH)/libusb-1.0.8/install/lib	
	cd $(PUBLIC_APPS_PATH)/libusb-compat-0.1.3 && $(MAKE) install
else
	@echo no libcompat
endif

.PHONY: usb_modeswitch
usb_modeswitch:
ifeq ($(INCLUDE_USB_3G_DONGLE), y)
	echo "Building usb_modeswitch"
	cd $(PUBLIC_APPS_PATH)/usb-modeswitch-1.2.3 && $(MAKE)
	$(STRIP) $(PUBLIC_APPS_PATH)/usb-modeswitch-1.2.3/usb_modeswitch
else
	@echo no usb_modeswitch
endif

####################################################
#
#Build filesystem, prepare basic fs first
#
####################################################
.PHONY: fs_build
fs_build: fs_clean fs_create fs_lib fs_bin fs_modules fs_rootfs

fs_clean:
	rm -rf $(MODEL_FS_PATH)/../rootfs.$(MODEL)
	rm -rf $(MODEL_FS_PATH)

fakeroot_clean:    
	if [ -e $(FAKEROOT_PATH)/Makefile ];  then make -C $(FAKEROOT_PATH) distclean; fi
	rm -f $(FAKEROOT_PATH)/fakeroot
	rm -f $(FAKEROOT_PATH)/.configured
	
.PHONY: fs_create
fs_create: 
	cp -pR $(TOP_PATH)/targets/fs.dir $(MODEL_FS_PATH)

	#for rcS, yuanshang, 20111026
	rm -fr $(MODEL_FS_PATH)/etc/init.d/*
	cp $(TOP_PATH)/targets/fs.dir/etc/init.d/rcS_$(MODEL) $(MODEL_FS_PATH)/etc/init.d/rcS

.PHONY: fs_lib
fs_lib:
	cp $(SLIB_PATH)/* $(MODEL_FS_PATH)/lib/
	cp $(PUBLIC_APPS_PATH)/upnp_libs/*.so $(MODEL_FS_PATH)/lib/
	cp $(PUBLIC_APPS_PATH)/os_libs/libos.so $(MODEL_FS_PATH)/lib/
	
ifeq ($(INCLUDE_SSL), y)
	cp -p $(PUBLIC_APPS_PATH)/openssl/libcrypto.so.0.9.7 $(MODEL_FS_PATH)/lib/
	cd $(MODEL_FS_PATH)/lib/ && ln -sf libcrypto.so.0.9.7 libcrypto.so
	cp -p $(PUBLIC_APPS_PATH)/openssl/libssl.so.0.9.7 $(MODEL_FS_PATH)/lib/
	cd $(MODEL_FS_PATH)/lib/ && ln -sf libssl.so.0.9.7 libssl.so
endif

	#for the libs of samba and ntfs-3g, zjj, 20111129
ifeq ($(INCLUDE_USB_STORAGE), y)
	cp -p $(PUBLIC_APPS_PATH)/ntfs-3g-1.2918/libntfs-3g/.libs/libntfs-3g.so.38.0.0 $(MODEL_FS_PATH)/lib/
	cd $(MODEL_FS_PATH)/lib/ && ln -sf libntfs-3g.so.38.0.0 libntfs-3g.so.38
	cd $(MODEL_FS_PATH)/lib/ && ln -sf libntfs-3g.so.38.0.0 libntfs-3g.so
ifeq ($(INCLUDE_USB_SAMBA_SERVER), y)
	cp -p $(PUBLIC_APPS_PATH)/samba-3.0.14a/source/bin/libbigballofmud.so $(MODEL_FS_PATH)/lib/
	cd $(MODEL_FS_PATH)/lib/ && ln -sf libbigballofmud.so libbigballofmud.so.0
endif
endif
	#end by zjj
 
ifeq ($(INCLUDE_USB_3G_DONGLE),y)
	cp -p $(PUBLIC_APPS_PATH)/libusb-1.0.8/install/lib/libusb-1.0.so* $(MODEL_FS_PATH)/lib/
	cp -p $(PUBLIC_APPS_PATH)/libusb-compat-0.1.3/install/lib/libusb-0.1.so* $(MODEL_FS_PATH)/lib/
	cp -p $(PUBLIC_APPS_PATH)/libusb-compat-0.1.3/install/lib/libusb.so $(MODEL_FS_PATH)/lib/
endif #INCLUDE_USB_3G_DONGLE
 
ifeq ($(INCLUDE_IPSEC), y)	
	cp -p $(PUBLIC_APPS_PATH)/ipsectools/src/racoon/.libs/libracoon.so.0.0.0 $(MODEL_FS_PATH)/lib/
	cd $(MODEL_FS_PATH)/lib/ && ln -sf libracoon.so.0.0.0 libracoon.so.0
	cd $(MODEL_FS_PATH)/lib/ && ln -sf libracoon.so.0.0.0 libracoon.so
	cp -p $(PUBLIC_APPS_PATH)/ipsectools/src/libipsec/.libs/libipsec.so.0.0.1 $(MODEL_FS_PATH)/lib/
	cd $(MODEL_FS_PATH)/lib/ && ln -sf libipsec.so.0.0.1 libipsec.so.0
	cd $(MODEL_FS_PATH)/lib/ && ln -sf libipsec.so.0.0.1 libipsec.so
endif


.PHONY:fs_bin
fs_bin:
	cd $(PUBLIC_APPS_PATH)/busybox-1.19.2 && $(MAKE) install CONFIG_PREFIX=$(MODEL_FS_PATH)
	cp -p $(PUBLIC_APPS_PATH)/iproute2-2.6.24-rc7/tc/tc $(MODEL_FS_PATH)/usr/bin
	cp -p $(PUBLIC_APPS_PATH)/iptables-1.4.0rc1/iptables $(MODEL_FS_PATH)/usr/bin
	#cp -p $(PUBLIC_APPS_PATH)/br2684ctl/br2684ctl $(MODEL_FS_PATH)/usr/bin
	cp -p $(PUBLIC_APPS_PATH)/ppp-2.4.5/pppd/pppd $(MODEL_FS_PATH)/usr/sbin
	cp -p $(PUBLIC_APPS_PATH)/ebtables-v2.0.10-4/ebtables $(MODEL_FS_PATH)/usr/bin
	cp -p $(PUBLIC_APPS_PATH)/IGD/upnpd $(MODEL_FS_PATH)/usr/bin
	cp -p $(PUBLIC_APPS_PATH)/linux-atm/arpd/atmarp $(MODEL_FS_PATH)/usr/sbin
	cp -p $(PUBLIC_APPS_PATH)/linux-atm/arpd/atmarpd $(MODEL_FS_PATH)/usr/sbin
	cp -p $(PUBLIC_APPS_PATH)/dhcp/dhcpd $(MODEL_FS_PATH)/usr/bin
	cp -p $(PUBLIC_APPS_PATH)/dhcp/dhcpc $(MODEL_FS_PATH)/usr/bin
	
ifeq ($(INCLUDE_USB_FTP_SERVER), y)
	cp -p $(PUBLIC_APPS_PATH)/vsftpd-2.3.2/vsftpd $(MODEL_FS_PATH)/usr/bin
	cp -p $(PUBLIC_APPS_PATH)/vsftpd-2.3.2/vsftpd.conf $(MODEL_FS_PATH)/etc
	cp -p $(PUBLIC_APPS_PATH)/vsftpd-2.3.2/passwd $(MODEL_FS_PATH)/etc/vsftpd_passwd
endif

ifeq ($(INCLUDE_RIP), y)
	cp -p $(PUBLIC_APPS_PATH)/zebra-0.95a/ripd/ripd $(MODEL_FS_PATH)/usr/sbin
	cp -p $(PUBLIC_APPS_PATH)/zebra-0.95a/zebra/zebra $(MODEL_FS_PATH)/usr/sbin
endif
	
ifeq ($(INCLUDE_IPV6_RADVD), y)
	cp -p $(PUBLIC_APPS_PATH)/radvd-1.5/radvd $(MODEL_FS_PATH)/usr/sbin
endif

ifeq ($(INCLUDE_IPV6_DHCPV6), y)
	cp -p $(PUBLIC_APPS_PATH)/wide-dhcpv6-20080615/dhcp6s $(MODEL_FS_PATH)/usr/sbin
	cp -p $(PUBLIC_APPS_PATH)/wide-dhcpv6-20080615/dhcp6c $(MODEL_FS_PATH)/usr/sbin
endif	
	
ifeq ($(INCLUDE_IPV6_TUNNEL), y)
	cp -p $(PUBLIC_APPS_PATH)/iproute2-2.6.39/ip/ip $(MODEL_FS_PATH)/usr/bin
endif		
	
ifeq ($(INCLUDE_IPV6), y)
	cp -p $(PUBLIC_APPS_PATH)/iptables-1.4.0rc1/ip6tables $(MODEL_FS_PATH)/usr/bin
endif		
	
ifeq ($(INCLUDE_USB_STORAGE),y)
	#for the bins of samba and ntfs-3g, zjj, 20111129
	cp -p $(PUBLIC_APPS_PATH)/ntfs-3g-1.2918/src/.libs/ntfs-3g $(MODEL_FS_PATH)/usr/bin
ifeq ($(INCLUDE_USB_SAMBA_SERVER), y)
	cp -p $(PUBLIC_APPS_PATH)/samba-3.0.14a/source/bin/smbd $(MODEL_FS_PATH)/usr/bin
	cp -p $(PUBLIC_APPS_PATH)/samba-3.0.14a/source/bin/smbpasswd $(MODEL_FS_PATH)/usr/bin
	cp -p $(PUBLIC_APPS_PATH)/samba-3.0.14a/source/bin/smbcontrol $(MODEL_FS_PATH)/usr/bin
endif	# INCLUDE_USB_SAMBA_SERVER
	#end by zjj

#for dlna ,LI CHENGLONG
ifeq ($(INCLUDE_USB_MEDIA_SERVER), y)
	cp -p $(PUBLIC_APPS_PATH)/ushare-1.1a/src/ushare $(MODEL_FS_PATH)/usr/bin
	cp -p $(PUBLIC_APPS_PATH)/ushare-1.1a/scripts/ushare.conf $(MODEL_FS_PATH)/etc/ushare.conf
endif #INCLUDE_USB_MEDIA_SERVER
#end by LI CHENGLONG
else

endif	# INCLUDE_USB_STORAGE	
	
ifeq ($(INCLUDE_USB_3G_DONGLE),y)
	cp $(PUBLIC_APPS_PATH)/usb-modeswitch-1.2.3/usb_modeswitch $(MODEL_FS_PATH)/usr/bin
	cp $(PUBLIC_APPS_PATH)/ppp-2.4.5/chat/chat $(MODEL_FS_PATH)/usr/bin
endif #INCLUDE_USB_3G_DONGLE
	
ifeq ($(INCLUDE_IPSEC),y)
	cp -p $(PUBLIC_APPS_PATH)/ipsectools/src/racoon/.libs/racoon $(MODEL_FS_PATH)/usr/bin
	cp -p $(PUBLIC_APPS_PATH)/ipsectools/src/setkey/.libs/setkey $(MODEL_FS_PATH)/usr/bin
endif

ifeq ($(strip $(SUPPLIER)),lantiq)
ifeq ($(INCLUDE_LAN_WLAN), y)
	cp $(INSTALL_WIRELESS)/lib/libiw.so.28 $(MODEL_FS_PATH)/lib/
	cp -p $(INSTALL_WIRELESS)/sbin/* $(MODEL_FS_PATH)/sbin
	cp -p $(INSTALL_WIRELESS)/lib/*.so $(MODEL_FS_PATH)/lib/
endif	#INCLUDE_LAN_WLAN

ifeq ($(INCLUDE_ADSLWAN), y)
#prepare
	mkdir $(MODEL_FS_PATH)/firmware
	mkdir $(MODEL_FS_PATH)/tmp
	mkdir $(MODEL_FS_PATH)/tmp/pipe
	echo "ramfs /tmp ramfs defaults 0 0" >> $(MODEL_FS_PATH)/etc/fstab
	echo "none /sys sysfs defaults 0 0" >> $(MODEL_FS_PATH)/etc/fstab
#app bin
	cp $(SLIB_PATH)/../usr/sbin/br2684ctl $(MODEL_FS_PATH)/usr/sbin
	cp $(SLIB_PATH)/../usr/sbin/br2684ctld $(MODEL_FS_PATH)/usr/sbin
endif	#INCLUDE_ADSLWAN

endif	#SUPPLIER-lantiq

.PHONY:fs_modules
fs_modules:
	cp -pR $(MODULE_TG_PATH)/* $(MODEL_FS_PATH)/lib/modules
	
.PHONY:fs_rootfs
fs_rootfs: fakeroot
	find  $(MODEL_FS_PATH) -name ".svn" | xargs rm -rf
	cd $(TOOLS_PATH) && $(FAKEROOT_PATH)/fakeroot ./buildFs $(MODEL_FS_PATH) $(ROOTFSTOOLS) $(MODEL) $(SUPPLIER)

$(FAKEROOT_PATH)/.configured:
	(tar -jkxf fakeroot.tar.bz2 2> /dev/null || true)
	cd $(FAKEROOT_PATH); CFLAGS="-O2 -DGNU -I./" CC=gcc ./configure --prefix=/usr --disable-nls
	touch $@

.PHONY:fakeroot
fakeroot: $(FAKEROOT_PATH)/.configured
	make -C $(FAKEROOT_PATH)
	sed -e 's,^PREFIX=.*,PREFIX=$(FAKEROOT_PATH),g' -e 's,^BINDIR=.*,BINDIR=$(FAKEROOT_PATH),g' -e 's,^PATHS=.*,PATHS=$(FAKEROOT_PATH)/.libs:/lib:/usr/lib,g' $(FAKEROOT_PATH)/scripts/fakeroot > $(FAKEROOT_PATH)/fakeroot
	chmod u+x $(FAKEROOT_PATH)/fakeroot
