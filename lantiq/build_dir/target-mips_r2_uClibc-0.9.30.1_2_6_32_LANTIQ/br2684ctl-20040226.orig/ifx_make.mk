include .app_configured

ifeq ($(MAKE_USED),1)
include $(INCLUDE_DIR)/Rules.mk
include $(INCLUDE_DIR)/Makefile.common
-include $(UBOOT_CONFIG)

APP_NAME=br2684ctl
APP_TYPE=OPEN
APP_VER=20040226-1
CONFIG_FULL_PACKAGE=y
BUILD_DEPENDS=linux_atm

ifeq ($(CONFIG_FEATURE_IFX_TR69),y)
        BUILD_DEPENDS+=devm
endif

IFX_APIS_DIR:=$(USER_IFXSOURCE_DIR)/IFXAPIs/
IFX_CFLAGS      +=-I$(USER_OPENSOURCE_DIR)/linux_atm/src/include -I$(IFX_APIS_DIR)/include/ -I$(KERNEL_INCLUDE_PATH)

ifeq ($(PLATFORM),"AMAZON_SE")
	IFX_CFLAGS		+=-DCONFIG_AMAZON_SE
endif

ifeq ($(CONFIG_FEATURE_IFX_DSL_CPE_API),y)
	IFX_CFLAGS		+=-DLINUX -I$(USER_IFXSOURCE_DIR)/dsl_api/drv_dsl_cpe_api/src/include \
				-I$(USER_IFXSOURCE_DIR)/dsl_api/dsl_cpe_control/src -DDSL_CHANNELS_PER_LINE=1
        ifeq ($(IFX_CONFIG_CPU),"AMAZON_SE")
		#8111001:<IFTW-linje> integrated DSL API 3.16.3
		IFX_CFLAGS	+=-DINCLUDE_DSL_CPE_API_DANUBE -DCONFIG_FEATURE_IFX_DSL_CPE_API
                #IFX_CFLAGS	+=$(IFX_CFLAGS) -DINCLUDE_DSL_CPE_API_$(IFX_CONFIG_CPU) -DCONFIG_FEATURE_IFX_DSL_CPE_API
	endif
        ifeq ($(IFX_CONFIG_CPU),"DANUBE")
		#8111001:<IFTW-linje> integrated DSL API 3.16.3
		IFX_CFLAGS	+=-DINCLUDE_DSL_CPE_API_DANUBE -DCONFIG_FEATURE_IFX_DSL_CPE_API
                #IFX_CFLAGS	+=$(IFX_CFLAGS) -DINCLUDE_DSL_CPE_API_$(IFX_CONFIG_CPU) -DCONFIG_FEATURE_IFX_DSL_CPE_API
	endif
        ifeq ($(IFX_CONFIG_CPU),"AMAZON_S")
		IFX_CFLAGS	+=-DINCLUDE_DSL_CPE_API_DANUBE -DCONFIG_FEATURE_IFX_DSL_CPE_API
	endif
endif

all: configure compile install

define menuconfig

config FEATURE_RFC2684
	bool 'Support for RFC2684'
	help
	  Support for RFC2684

endef

define configure
endef

define compile
        $(MAKE) $(BUILD_FLAGS) IFX_CFLAGS="$(IFX_CFLAGS)" IFX_LDFLAGS="$(IFX_LDFLAGS)" all
	$(IFX_STRIP) br2684ctl
	$(IFX_STRIP) br2684ctld
endef

define install
	install -d $(BUILD_ROOTFS_DIR)/usr/sbin
	cp -f br2684ctl $(BUILD_ROOTFS_DIR)/usr/sbin/.
	cp -f br2684ctld $(BUILD_ROOTFS_DIR)/usr/sbin/.
endef

define clean
	$(MAKE) clean
endef

define distclean
	$(MAKE) clean
endef

$(eval $(call define_eval_application))
else
export MAKE_USED=1
all %:
	@$(MAKE) -s -C $(BUILD_TOOLS_DIR) make_installed
	@$(BUILD_TOOLS_DIR)/tmp/bin/make -f ifx_make.mk $@
endif
