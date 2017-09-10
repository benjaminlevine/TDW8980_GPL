/*
 * Automatically generated C config: don't edit
 * Linux kernel version: KERNELVERSION
 * Mon Jul 23 18:03:20 2012
 */
#define AUTOCONF_INCLUDED
#define CONFIG_UBOOT_CONFIG_CROSS_COMPILE_UCLIBC 1
#define CONFIG_KEXEC_TOOLS_TARGET_NAME "mips"
#define CONFIG_UBOOT_CONFIG_UPDATE_TOTALIMAGE "tftpboot $(loadaddr) $(tftppath)$(totalimage);upgrade $(loadaddr) $(filesize)"
#define CONFIG_UBOOT_CONFIG_CMD_MEMORY 1
#define CONFIG_UBOOT_CONFIG_DRIVER_VR9 1
#define CONFIG_UBOOT_CONFIG_RAMARGS "setenv bootargs root=/dev/ram rw"
#define CONFIG_DEFAULT_udevtrigger 1
#define CONFIG_USES_SQUASHFS 1
#define CONFIG_USE_MKLIBS 1
#define CONFIG_UBOOT_CONFIG_SPI_FLASH_SST 1
#define CONFIG_UBOOT_CONFIG_BOOT_FROM_SPI 1
#define CONFIG_UBOOT_CONFIG_CONSOLE "ttyS0"
#define CONFIG_UBOOT_CONFIG_NFSARGS "setenv bootargs root=/dev/nfs rw nfsroot=$(serverip):$(rootpath)"
#define CONFIG_UBOOT_CONFIG_ENABLE_DCDC 1
#define CONFIG_UBOOT_CONFIG_LANTIQ_SPI 1
#define CONFIG_SOFT_FLOAT 1
#define CONFIG_FEATURE_IFX_OAM_F5_LOOPBACK 1
#define CONFIG_EXTERNAL_KERNEL_TREE ""
#define CONFIG_DEFAULT_opkg 1
#define CONFIG_ifx-dsl-cpe-control-vrx_DTI 1
#define CONFIG_UBOOT_CONFIG_CMD_NET 1
#define CONFIG_BIN_DIR ""
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT_0 1
#define CONFIG_LIBC "uClibc"
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT_1 1
#define CONFIG_INSTALL_LIBSTDCPP 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT_2 1
#define CONFIG_NEED_TOOLCHAIN 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT_4 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT0_MIIMODE 4
#define CONFIG_ARCH "mips"
#define CONFIG_UBOOT_CONFIG_TFTP_LOAD_ADDRESS "0x80800000"
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT1_MIIMODE 4
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT2_MIIMODE 1
#define CONFIG_PACKAGE_ifx-dsl-cpe-control-vrx 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT4_MIIMODE 1
#define CONFIG_UBOOT_CONFIG_NET_RAM "tftp $(loadaddr) $(tftppath)$(bootfile); run ramargs addip addmisc; bootm"
#define CONFIG_KERNEL_GIT_CLONE_URI ""
#define CONFIG_UBOOT_CONFIG_CMD_RUN 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT0_MIIRATE 4
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT1_MIIRATE 4
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT2_MIIRATE 4
#define CONFIG_PACKAGE_kmod-ltqcpe_usb_class_drivers 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT4_MIIRATE 4
#define CONFIG_UBOOT_CONFIG_DDR_TUNING_TEXT_BASE 0x9e220000
#define CONFIG_UBOOT_CONFIG_NET_NFS "tftp $(loadaddr) $(tftppath)$(bootfile);run nfsargs addip addmisc;bootm"
#define CONFIG_UBOOT_CONFIG_BOOTCOMMAND "run flash_flash"
#define CONFIG_UBOOT_CONFIG_LZMA 1
#define CONFIG_UBOOT_CONFIG_SERVER_IP_ADDRESS "192.168.1.100"
#define CONFIG_LANTIQ_UBOOT_vr9 1
#define CONFIG_USE_SSTRIP 1
#define CONFIG_DEFAULT_TARGET_OPTIMIZATION "-Os -pipe -mips32r2 -mtune=mips32r2 -funit-at-a-time"
#define CONFIG_UBOOT_CONFIG_OS_LZMA 1
#define CONFIG_TARGET_PREINIT_BROADCAST "192.168.1.255"
#define CONFIG_PACKAGE_kmod-ifx-dsl-cpe-mei-vrx 1
#define CONFIG_UBOOT_CONFIG_SPI_FLASH_STMICRO 1
#define CONFIG_PACKAGE_br2684ctl 1
#define CONFIG_UCLIBC_VERSION "0.9.30.1"
#define CONFIG_UBOOT_CONFIG_FLASH_FLASH "sf probe 3; bootm 0x80800000"
#define CONFIG_FEATURE_IFX_USB_HOST 1
#define CONFIG_ifx-dsl-cpe-control-vrx_MODEL_FULL 1
#define CONFIG_TARGET_PREINIT_IP "192.168.1.1"
#define CONFIG_UBOOT_CONFIG_PHYM "64M"
#define CONFIG_DEFAULT_base-files 1
#define CONFIG_HAS_SUBTARGETS 1
#define CONFIG_AUTOREBUILD 1
#define CONFIG_TARGET_ROOTFS_DIR ""
#define CONFIG_ifx-dsl-cpe-mei-vrx_USE_KERNEL_BUILD_IN 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT2_GMII 1
#define CONFIG_DEFAULT_uboot-lantiq 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT4_GMII 1
#define CONFIG_UBOOT_CONFIG_SPI_FLASH_MXIC 1
#define CONFIG_LOCALMIRROR ""
#define CONFIG_PACKAGE_kmod-ltqcpe_pci 1
#define CONFIG_UBOOT_CONFIG_CMD_SAVEENV 1
#define CONFIG_UBOOT_CONFIG_VR9_DDR2 1
#define CONFIG_KERNEL_GIT_LOCAL_REPOSITORY ""
#define CONFIG_UBOOT_CONFIG_CMD_ECHO 1
#define CONFIG_TARGET_PREINIT_IFNAME ""
#define CONFIG_EXTRA_GCC_CONFIG_OPTIONS ""
#define CONFIG_UBOOT_CONFIG_BOOTFILE "uImage"
#define CONFIG_PACKAGE_kmod-ltqcpe_imq 1
#define CONFIG_GCC_VERSION "4.3.3+cs"
#define CONFIG_TARGET_OPTIONS 1
#define CONFIG_PACKAGE_kmod-ltqcpe_spi 1
#define CONFIG_UBOOT_CONFIG_ENV_IS_NOWHERE 1
#define CONFIG_TARGET_PREINIT_SUPPRESS_STDERR 1
#define CONFIG_UBOOT_CONFIG_FIRMWARE "firmware.img"
#define CONFIG_PACKAGE_kmod-ltqcpe_nor 1
#define CONFIG_PACKAGE_ifx-dsl-cpe-api-vrx 1
#define CONFIG_DEFAULT_dnsmasq 1
#define CONFIG_PACKAGE_ifx-dsl-cpe-mei-vrx 1
#define CONFIG_PACKAGE_libnl 1
#define CONFIG_UBOOT_CONFIG_ADDIP "setenv bootargs $(bootargs) ip=$(ipaddr):$(serverip):$(gatewayip):$(netmask):$(hostname):$(netdev):on"
#define CONFIG_UBOOT_CONFIG_LANTIQ_UART 1
#define CONFIG_UBOOT_CONFIG_TFTPPATH ""
#define CONFIG_PACKAGE_librt 1
#define CONFIG_PACKAGE_libpthread 1
#define CONFIG_UBOOT_CONFIG_ETHERNET_DEVICE "eth0"
#define CONFIG_UBOOT_CONFIG_MTDPARTS "ifx_sflash:128k(uboot),1536k(kernel),5824k(rootfs),512k(firmware),64k(sysconfig),64k(ubootconfig),64k(fwdiag)"
#define CONFIG_DEVEL 1
#define CONFIG_DEFAULT_busybox 1
#define CONFIG_TARGET_INIT_SUPPRESS_STDERR 1
#define CONFIG_ifx-dsl-cpe-api-vrx_MODEL_FULL 1
#define CONFIG_UBOOT_CONFIG_TUNE_DDR 1
#define CONFIG_DEFAULT_ppp-mod-pppoe 1
#define CONFIG_DOWNLOAD_FOLDER ""
#define CONFIG_UBOOT_CONFIG_VR9_CRYSTAL_36M 1
#define CONFIG_GCC_VERSION_4_3 1
#define CONFIG_UBOOT_CONFIG_CONFIG_REMOVE_GZIP 1
#define CONFIG_PACKAGE_kmod-ltqcpe_usb_device_port1 1
#define CONFIG_DEFAULT_kmod-ipt-nathelper 1
#define CONFIG_LINUX_2_6_32 1
#define CONFIG_UBOOT_CONFIG_ASC_BAUDRATE "115200"
#define CONFIG_UBOOT_CONFIG_UPDATE_FIRMWARE "tftpboot $(loadaddr) $(tftppath)$(firmware);upgrade $(loadaddr) $(filesize)"
#define CONFIG_AUTOSELECT_DEFAULT_KERNEL 1
#define CONFIG_UBOOT_CONFIG_SF_DEFAULT_MODE 0
#define CONFIG_HAVE_DOT_CONFIG 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT_5a 1
#define CONFIG_UBOOT_CONFIG_SPI_FLASH_SPANSION 1
#define CONFIG_UBOOT_CONFIG_GPHY_LED_SHIFT_REG 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT5a_MIIMODE 4
#define CONFIG_PACKAGE_open_uboot 1
#define CONFIG_PACKAGE_kmod-pecostat 1
#define CONFIG_UBOOT_CONFIG_SPI_FLASH_ATMEL 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT5a_MIIRATE 4
#define CONFIG_LARGEFILE 1
#define CONFIG_TARGET_INIT_CMD "/sbin/init"
#define CONFIG_UBOOT_CONFIG_SFDDR_TEXT_BASE 0xbe220500
#define CONFIG_TARGET_BOARD "ltqcpe"
#define CONFIG_UBOOT_CONFIG_VRX200 1
#define CONFIG_KERNEL_KALLSYMS 1
#define CONFIG_UBOOT_CONFIG_CONFIG_IFX_MIPS 1
#define CONFIG_UBOOT_CONFIG_FULLIMAGE "fullimage.img"
#define CONFIG_EXTROOT_SETTLETIME 20
#define CONFIG_UBOOT_CONFIG_FLASHARGS "setenv bootargs root=$(rootfsmtd) rw rootfstype=squashfs"
#define CONFIG_DEFAULT_hotplug2 1
#define CONFIG_UBOOT_CONFIG_SPI_FLASH_8M 1
#define CONFIG_FEATURE_IFX_OAM_F5_LOOPBACK_PING 1
#define CONFIG_LIBC_VERSION "0.9.30.1"
#define CONFIG_UBOOT_CONFIG_BOOTDELAY 1
#define CONFIG_PACKAGE_ifx-os 1
#define CONFIG_UBOOT_CONFIG_NAND_PRELOAD_TEXT_BASE 0xbe220000
#define CONFIG_TARGET_INIT_ENV ""
#define CONFIG_FEATURE_IFX_OAM_EVENT_SCRIPT 1
#define CONFIG_BUILD_SUFFIX "LANTIQ"
#define CONFIG_DEFAULT_uci 1
#define CONFIG_UBOOT_CONFIG_UPDATE_UBOOT "tftp $(loadaddr) $(tftppath)$(u-boot); nand write.partial $(loadaddr) 4000 $(filesize);reset"
#define CONFIG_UNSTRIPPED_COPY 1
#define CONFIG_DEFAULT_mtd 1
#define CONFIG_FEATURE_IFX_USB_DEVICE 1
#define CONFIG_PACKAGE_kmod-ltqcpe_pcie 1
#define CONFIG_PACKAGE_kmod-ltqcpe_nand 1
#define CONFIG_PACKAGE_libgcc 1
#define CONFIG_TARGET_ltqcpe 1
#define CONFIG_UCLIBC_VERSION_0_9_30_1 1
#define CONFIG_UBOOT_CONFIG_FLASH_NFS "run nfsargs addip addmisc;bootm $(kernel_addr)"
#define CONFIG_UBOOT_CONFIG_NET_FLASH "tftp $(loadaddr) $(tftppath)$(bootfile); run flashargs addip addmisc; bootm"
#define CONFIG_BUILD_LOG 1
#define CONFIG_TARGET_PREINIT_NETMASK "255.255.255.0"
#define CONFIG_DEFAULT_dropbear 1
#define CONFIG_DEFAULT_ppp 1
#define CONFIG_UBOOT_CONFIG_UPDATE_FULLIMAGE "tftpboot $(loadaddr) $(tftppath)$(fullimage);upgrade $(loadaddr) $(filesize)"
#define CONFIG_UBOOT_CONFIG_IFX_MEMORY_SIZE 64
#define CONFIG_UBOOT_CONFIG_SPI_FLASH 1
#define CONFIG_LINUX_2_6 1
#define CONFIG_DEFAULT_iptables 1
#define CONFIG_mips 1
#define CONFIG_DEFAULT_firewall 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT0_MIIRATE_AUTO 1
#define CONFIG_IFX_VRX_CHANNELS_PER_LINE "1"
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT1_MIIRATE_AUTO 1
#define CONFIG_UBOOT_CONFIG_ROOTFSMTD "/dev/mtdblock2"
#define CONFIG_PACKAGE_ppacmd 1
#define CONFIG_UBOOT_CONFIG_ETHERNET_ADDRESS "00:E0:92:00:01:40"
#define CONFIG_PACKAGE_ifx-dsl-vr9-firmware-xdsl 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT0_RGMII_MAC 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT1_RGMII_MAC 1
#define CONFIG_PACKAGE_libuci 1
#define CONFIG_PACKAGE_kmod-ltqcpe_spi_flash 1
#define CONFIG_UBOOT_CONFIG_SYS_NO_FLASH 1
#define CONFIG_TARGET_PREINIT_TIMEOUT 2
#define CONFIG_PACKAGE_kmod-ltqcpe_usb_host_port2 1
#define CONFIG_DEFAULT_libgcc 1
#define CONFIG_UBOOT_CONFIG_CMD_SF 1
#define CONFIG_UBOOT_CONFIG_SF_DEFAULT_SPEED 33250000
#define CONFIG_UBOOT_CONFIG_ROOT_PATH "/mnt/full_fs"
#define CONFIG_GDB 1
#define CONFIG_TARGET_OPTIMIZATION "-Os -pipe -mips32r2 -mtune=mips32r2 -funit-at-a-time -fhonour-copts"
#define CONFIG_TARGET_ROOTFS_SQUASHFS 1
#define CONFIG_UBOOT_CONFIG_BOOTSTRAP_TEXT_BASE 0xa0100000
#define CONFIG_BINUTILS_VERSION_2_19_1 1
#define CONFIG_UBOOT_CONFIG_ROOTFS "rootfs.img"
#define CONFIG_PACKAGE_kmod-atm_stack 1
#define CONFIG_UBOOT_CONFIG_MII 1
#define CONFIG_UBOOT_CONFIG_MEM "63M"
#define CONFIG_UBOOT_CONFIG_UPDATE_KERNEL "tftpboot $(loadaddr) $(tftppath)$(bootfile);upgrade $(loadaddr) $(filesize)"
#define CONFIG_UBOOT_CONFIG_IFX_UBOOT_OPTIMIZED 1
#define CONFIG_PACKAGE_kmod-ltqcpe_ppa_a5_mod 1
#define CONFIG_BIG_ENDIAN 1
#define CONFIG_TARGET_ltqcpe_platform_vr9_None 1
#define CONFIG_UBOOT_CONFIG_TOTALIMAGE "totalimage.img"
#define CONFIG_UBOOT_CONFIG_U_BOOT "u-boot.lq"
#define CONFIG_PACKAGE_libc 1
#define CONFIG_GCC_VERSION_4 1
#define CONFIG_TARGET_INIT_PATH "/bin:/sbin:/usr/bin:/usr/sbin"
#define CONFIG_TOOLCHAINOPTS 1
#define CONFIG_UBOOT_CONFIG_VR9_CPU_500M_RAM_250M 1
#define CONFIG_PACKAGE_ifx-ethsw 1
#define CONFIG_PACKAGE_linux-atm 1
#define CONFIG_UBOOT_CONFIG_VR9_GPHY_FW_ADDR 0xa0110000
#define CONFIG_SHADOW_PASSWORDS 1
#define CONFIG_EXTRA_BINUTILS_CONFIG_OPTIONS ""
#define CONFIG_EXTERNAL_CPIO ""
#define CONFIG_PACKAGE_kmod-fs-autofs4 1
#define CONFIG_BINUTILS_VERSION "2.19.1"
#define CONFIG_PACKAGE_lib-dti 1
#define CONFIG_UBOOT_CONFIG_ADDMISC "setenv bootargs $(bootargs) console=$(console),$(baudrate) ethaddr=$(ethaddr) phym=$(phym) mem=$(mem) panic=1 mtdparts=$(mtdparts) init=/etc/preinit vpe1_load_addr=0x82000000 vpe1_mem=1M ethwan=$(ethwan)"
#define CONFIG_TARGET_ltqcpe_platform_vr9 1
#define CONFIG_AUTO_CONF_GEN 1
#define CONFIG_UBOOT_CONFIG_VR9_SW_PORT5a_RGMII 1
#define CONFIG_RETAIN_KERNEL_CONFIG 1
#define CONFIG_UBOOT_CONFIG_VR9_GPHY_FW_EMBEDDED 1
#define CONFIG_PACKAGE_kmod-ltq_optimization 1
#define CONFIG_USES_JFFS2 1
#define CONFIG_UBOOT_CONFIG_IP_ADDRESS "192.168.1.1"
#define CONFIG_UBOOT_CONFIG_RAM_TEXT_BASE 0xA0400000
#define CONFIG_TARGET_SUFFIX "uclibc"
#define CONFIG_PACKAGE_ifx-oam 1
#define CONFIG_UBOOT_CONFIG_UPDATE_ROOTFS "tftpboot $(loadaddr) $(tftppath)$(rootfs);upgrade $(loadaddr) $(filesize)"
#define CONFIG_USE_UCLIBC 1
#define CONFIG_GCC_VERSION_4_3_3_CS 1
#define CONFIG_PACKAGE_kmod-jffs2 1
#define CONFIG_DEFAULT_libc 1
