/*
 * Automatically generated C config: don't edit
 * Linux kernel version: 2.6.27.8
 * Fri Jul 20 15:44:02 2012
 */
#define AUTOCONF_INCLUDED
#define CONFIG_CROSS_COMPILE_UCLIBC 1
#define CONFIG_UPDATE_TOTALIMAGE "tftpboot $(loadaddr) $(tftppath)$(totalimage);upgrade $(loadaddr) $(filesize)"
#define CONFIG_CMD_MEMORY 1
#define CONFIG_DRIVER_VR9 1
#define CONFIG_RAMARGS "setenv bootargs root=/dev/ram rw"
#define CONFIG_SPI_FLASH_SST 1
#define CONFIG_BOOT_FROM_SPI 1
#define CONFIG_CONSOLE "ttyS0"
#define CONFIG_NFSARGS "setenv bootargs root=/dev/nfs rw nfsroot=$(serverip):$(rootpath)"
#define CONFIG_ENABLE_DCDC 1
#define CONFIG_LANTIQ_SPI 1
#define CONFIG_CMD_NET 1
#define CONFIG_VR9_SW_PORT_0 1
#define CONFIG_VR9_SW_PORT_1 1
#define CONFIG_ARCH "mips"
#define CONFIG_VR9_SW_PORT_2 1
#define CONFIG_VR9_SW_PORT_4 1
#define CONFIG_VR9_SW_PORT0_MIIMODE 4
#define CONFIG_TFTP_LOAD_ADDRESS "0x80800000"
#define CONFIG_VR9_SW_PORT1_MIIMODE 4
#define CONFIG_VR9_SW_PORT2_MIIMODE 1
#define CONFIG_VR9_SW_PORT4_MIIMODE 1
#define CONFIG_NET_RAM "tftp $(loadaddr) $(tftppath)$(bootfile); run ramargs addip addmisc; bootm"
#define CONFIG_CMD_RUN 1
#define CONFIG_VR9_SW_PORT0_MIIRATE 4
#define CONFIG_VR9_SW_PORT1_MIIRATE 4
#define CONFIG_VR9_SW_PORT2_MIIRATE 4
#define CONFIG_VR9_SW_PORT4_MIIRATE 4
#define CONFIG_DDR_TUNING_TEXT_BASE 0x9e220000
#define CONFIG_NET_NFS "tftp $(loadaddr) $(tftppath)$(bootfile);run nfsargs addip addmisc;bootm"
#define CONFIG_LANTIQ_UBOOT_vr9 1
#define CONFIG_BOOTCOMMAND "run flash_flash"
#define CONFIG_LZMA 1
#define CONFIG_SERVER_IP_ADDRESS "192.168.1.100"
#define CONFIG_OS_LZMA 1
#define CONFIG_SPI_FLASH_STMICRO 1
#define CONFIG_FLASH_FLASH "sf probe 3; bootm 0x80800000"
#define CONFIG_PHYM "64M"
#define CONFIG_VR9_SW_PORT2_GMII 1
#define CONFIG_VR9_SW_PORT4_GMII 1
#define CONFIG_SPI_FLASH_MXIC 1
#define CONFIG_CMD_SAVEENV 1
#define CONFIG_VR9_DDR2 1
#define CONFIG_CMD_ECHO 1
#define CONFIG_BOOTFILE "uImage"
#define CONFIG_ENV_IS_NOWHERE 1
#define CONFIG_FIRMWARE "firmware.img"
#define CONFIG_ADDIP "setenv bootargs $(bootargs) ip=$(ipaddr):$(serverip):$(gatewayip):$(netmask):$(hostname):$(netdev):on"
#define CONFIG_LANTIQ_UART 1
#define CONFIG_TFTPPATH ""
#define CONFIG_ETHERNET_DEVICE "eth0"
#define CONFIG_MTDPARTS "ifx_sflash:128k(uboot),1536k(kernel),5824k(rootfs),512k(firmware),64k(sysconfig),64k(ubootconfig),64k(fwdiag)"
#define CONFIG_TUNE_DDR 1
#define CONFIG_VR9_CRYSTAL_36M 1
#define CONFIG_CONFIG_REMOVE_GZIP 1
#define CONFIG_ASC_BAUDRATE "115200"
#define CONFIG_UPDATE_FIRMWARE "tftpboot $(loadaddr) $(tftppath)$(firmware);upgrade $(loadaddr) $(filesize)"
#define CONFIG_RESET_UBOOT_CONFIG "sf probe 3; sf write 80400000 $(f_ubootconfig_addr) $(f_ubootconfig_size)"
#define CONFIG_SF_DEFAULT_MODE 0
#define CONFIG_VR9_SW_PORT_5a 1
#define CONFIG_SPI_FLASH_SPANSION 1
#define CONFIG_GPHY_LED_SHIFT_REG 1
#define CONFIG_VR9_SW_PORT5a_MIIMODE 4
#define CONFIG_SPI_FLASH_ATMEL 1
#define CONFIG_VR9_SW_PORT5a_MIIRATE 4
#define CONFIG_SFDDR_TEXT_BASE 0xbe220500
#define CONFIG_VRX200 1
#define CONFIG_CONFIG_IFX_MIPS 1
#define CONFIG_FULLIMAGE "fullimage.img"
#define CONFIG_FLASHARGS "setenv bootargs root=$(rootfsmtd) rw rootfstype=squashfs"
#define CONFIG_SPI_FLASH_8M 1
#define CONFIG_BOOTDELAY 1
#define CONFIG_NAND_PRELOAD_TEXT_BASE 0xbe220000
#define CONFIG_UPDATE_UBOOT "tftp $(loadaddr) $(tftppath)$(u-boot); nand write.partial $(loadaddr) 4000 $(filesize);reset"
#define CONFIG_FLASH_NFS "run nfsargs addip addmisc;bootm $(kernel_addr)"
#define CONFIG_NET_FLASH "tftp $(loadaddr) $(tftppath)$(bootfile); run flashargs addip addmisc; bootm"
#define CONFIG_UPDATE_FULLIMAGE "tftpboot $(loadaddr) $(tftppath)$(fullimage);upgrade $(loadaddr) $(filesize)"
#define CONFIG_IFX_MEMORY_SIZE 64
#define CONFIG_SPI_FLASH 1
#define CONFIG_VR9_SW_PORT0_MIIRATE_AUTO 1
#define CONFIG_VR9_SW_PORT1_MIIRATE_AUTO 1
#define CONFIG_ROOTFSMTD "/dev/mtdblock2"
#define CONFIG_ETHERNET_ADDRESS "00:E0:92:00:01:40"
#define CONFIG_VR9_SW_PORT0_RGMII_MAC 1
#define CONFIG_VR9_SW_PORT1_RGMII_MAC 1
#define CONFIG_SYS_NO_FLASH 1
#define CONFIG_CMD_SF 1
#define CONFIG_SF_DEFAULT_SPEED 33250000
#define CONFIG_ROOT_PATH "/mnt/full_fs"
#define CONFIG_RESET_DDR_CONFIG "sf probe 3; sf write 80400000 $(f_ddrconfig_addr) $(f_ddrconfig_size)"
#define CONFIG_UNAME_RELEASE "2.6.9-5.ELsmp"
#define CONFIG_BOOTSTRAP_TEXT_BASE 0xa0100000
#define CONFIG_ROOTFS "rootfs.img"
#define CONFIG_MII 1
#define CONFIG_MEM "63M"
#define CONFIG_UPDATE_KERNEL "tftpboot $(loadaddr) $(tftppath)$(bootfile);upgrade $(loadaddr) $(filesize)"
#define CONFIG_IFX_UBOOT_OPTIMIZED 1
#define CONFIG_TOTALIMAGE "totalimage.img"
#define CONFIG_KERNELVERSION "2.6.27.8"
#define CONFIG_U_BOOT "u-boot.lq"
#define CONFIG_VR9_CPU_500M_RAM_250M 1
#define CONFIG_VR9_GPHY_FW_ADDR 0xa0110000
#define CONFIG_ADDMISC "setenv bootargs $(bootargs) console=$(console),$(baudrate) ethaddr=$(ethaddr) phym=$(phym) mem=$(mem) panic=1 mtdparts=$(mtdparts) init=/etc/preinit vpe1_load_addr=0x82000000 vpe1_mem=1M ethwan=$(ethwan)"
#define CONFIG_VR9_SW_PORT5a_RGMII 1
#define CONFIG_VR9_GPHY_FW_EMBEDDED 1
#define CONFIG_IP_ADDRESS "192.168.1.1"
#define CONFIG_RAM_TEXT_BASE 0xA0400000
#define CONFIG_UPDATE_ROOTFS "tftpboot $(loadaddr) $(tftppath)$(rootfs);upgrade $(loadaddr) $(filesize)"
