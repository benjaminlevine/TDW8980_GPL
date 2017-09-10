# 
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=squashfs3
PKG_BASE_NAME:=squashfs
PKG_VERSION:=3.2-r2

PKG_SOURCE:=$(PKG_BASE_NAME)$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=@SF/squashfs
PKG_MD5SUM:=bf360b92eba9e6d5610196ce2e02fcd1
PKG_CAT:=zcat

HOST_BUILD_DIR:=$(BUILD_DIR_HOST)/$(PKG_BASE_NAME)$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk

define Host/Compile
	$(MAKE) -C $(HOST_BUILD_DIR)/squashfs-tools \
		LZMAPATH=$(STAGING_DIR_HOST)/lib/ \
		 mksquashfs unsquashfs 
endef

define Host/Install
	$(INSTALL_BIN) $(HOST_BUILD_DIR)/squashfs-tools/mksquashfs $(STAGING_DIR_HOST)/bin/mksquashfs-lzma
	$(INSTALL_BIN) $(HOST_BUILD_DIR)/squashfs-tools/unsquashfs $(STAGING_DIR_HOST)/bin/
endef

define Host/Clean
	rm -f $(STAGING_DIR_HOST)/bin/mksquashfs-lzma
	rm -f $(STAGING_DIR_HOST)/bin/unsquashfs
endef

$(eval $(call HostBuild))
