# 
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=lzma443
PKG_VERSION:=443

PKG_SOURCE:=$(PKG_NAME).tar.bz2
PKG_SOURCE_URL:=http://kent.dl.sourceforge.net/sourceforge/sevenzip/
PKG_MD5SUM:=c4e1b467184c7cffd4371c74df2baf0f

HOST_BUILD_DIR:=$(BUILD_DIR_HOST)/$(PKG_NAME)
HOST_UNPACK:=bzcat $(DL_DIR)/$(PKG_SOURCE) | $(TAR) -C $(HOST_BUILD_DIR)/ $(TAR_OPTIONS)

include $(INCLUDE_DIR)/host-build.mk

LIB_DIR=$(HOST_BUILD_DIR)/C/7zip/Compress/LZMA_Lib
ALONE_DIR=$(HOST_BUILD_DIR)/C/7zip/Compress/LZMA_Alone
LzmaC = $(HOST_BUILD_DIR)/C/7zip/Compress/LZMA_C

define Host/Compile
	$(MAKE) -f sqlzma.mk -C $(ALONE_DIR)
	$(MAKE) -f sqlzma.mk -C $(LzmaC)
endef

define Host/Install
	$(INSTALL_BIN) $(ALONE_DIR)/lzma $(STAGING_DIR_HOST)/bin/
	cp -f  $(ALONE_DIR)/*.a $(STAGING_DIR_HOST)/lib/
	cp -f $(LzmaC)/*.a $(STAGING_DIR_HOST)/lib/
endef

define Host/Clean
	rm -f \
		$(STAGING_DIR_HOST)/lib/liblzma.a \
		$(STAGING_DIR_HOST)/bin/lzma
endef

$(eval $(call HostBuild))
