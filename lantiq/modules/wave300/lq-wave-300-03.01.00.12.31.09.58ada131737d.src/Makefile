
export CONFIG_FILE:=$(PWD)/.config

CONFIG_EXISTS := $(wildcard $(CONFIG_FILE))

VCPROJ_FNAME:=msvc_all_[$(shell echo $${PWD\#\#*/})].vcproj
VCXPROJ_FNAME:=msvc_all_[$(shell echo $${PWD\#\#*/})].vcxproj
VCXPROJ_FILTERS_FNAME:=$(VCXPROJ_FNAME).filters

ifneq ($(CONFIG_EXISTS),)

include $(CONFIG_FILE)

all: .build_tree_ts
	@make -C builds/$(CONFIG_ENVIRONMENT_NAME) install
ifneq ($(_TFTPROOT),)
ifeq ($(CONFIG_LIN_DRV), y)
	@cp -v builds/$(CONFIG_ENVIRONMENT_NAME)/binaries/driver/* $(_TFTPROOT)
endif
ifeq ($(CONFIG_LIN_MTTOOLS), y)
	@cp -v builds/$(CONFIG_ENVIRONMENT_NAME)/binaries/mttools/* $(_TFTPROOT)
endif
ifeq ($(CONFIG_LIN_RTLOG_COMPONENTS), y)
	@cp -v builds/$(CONFIG_ENVIRONMENT_NAME)/binaries/rtlogger/* $(_TFTPROOT)
endif
ifeq ($(CONFIG_LIN_UTF), y)
	@cp -v builds/$(CONFIG_ENVIRONMENT_NAME)/binaries/tests/* $(_TFTPROOT)
endif
endif

else
all:
	@echo ERROR: no .config file found!
	@echo Run "make xconfig" or "make menuconfig" first to generate .config file!

endif

vcproj:
	@echo "Building VCPROJ ($(VCPROJ_FNAME))..."
	@perl mtlk_list_files.pl . mtlk_list_files.cfg msvc_all.vcproj.cfg.template NO_FILTERS > $(VCPROJ_FNAME) < msvc_all.vcproj.template
	@echo "Done ($(VCPROJ_FNAME))!"

vcxproj:
	@echo "Building VCXPROJ ($(VCXPROJ_FNAME))..."
	@perl mtlk_list_files.pl . mtlk_list_files.cfg msvc_all.vcxproj.cfg.template msvc_all.vcxproj.cfg_list.template > $(VCXPROJ_FNAME) < msvc_all.vcxproj.template
	@perl mtlk_list_files.pl . mtlk_list_files.cfg msvc_all.vcxproj.cfg.template msvc_all.vcxproj.cfg_list.template > $(VCXPROJ_FILTERS_FNAME) < msvc_all.vcxproj.filters.template
	@echo "Done ($(VCXPROJ_FNAME))!"

.PHONY: .build_conf .build_mconf .build_qconf .config_x .config_m .config_c .config_d config xconfig menuconfig defconfig

configure: configure.ac
	autoreconf --install

config: .build_conf .config_c .config.h .build_tree_ts
xconfig: .build_qconf .config_x .config.h .build_tree_ts
menuconfig: .build_mconf .config_m .config.h .build_tree_ts
defconfig: .build_conf .config_d .config.h .build_tree_ts

BUILD_TREE_DEPS:=$(shell [ -f .config ] && ./support/cfghlpr.sh .config get_bld_tree_cfg)

.build_tree_ts:: configure .config $(BUILD_TREE_DEPS)
	./support/cfghlpr.sh .config
	@touch .build_tree_ts

.build_conf:
	@make -C tools/kconfig config

.build_qconf:
	@make -C tools/kconfig xconfig

.build_mconf:
	@make -C tools/kconfig menuconfig

ifneq ( $(BUILD_TREE_DEPS), )

$(BUILD_TREE_DEPS):
	@echo -n

endif

.config_c:
	$(PWD)/tools/kconfig/conf ./MTLKConfig

.config_x:
	$(PWD)/tools/kconfig/qconf ./MTLKConfig

.config_m:
	$(PWD)/tools/kconfig/mconf ./MTLKConfig

ifeq ($(MINICONFIG),)
MINICONFIG=/dev/null
endif

.config_d:
	$(PWD)/tools/kconfig/conf -D $(MINICONFIG) MTLKConfig

#TODO: remove .config.h generation from here after new build system merge finished
#      .config.h will be generated by cfghlpr.sh
.config.h: .config
	gawk -f wireless/scripts/make_cfg_header.awk < .config > .config.h

%:
	@make -C builds/$(CONFIG_ENVIRONMENT_NAME) $@
