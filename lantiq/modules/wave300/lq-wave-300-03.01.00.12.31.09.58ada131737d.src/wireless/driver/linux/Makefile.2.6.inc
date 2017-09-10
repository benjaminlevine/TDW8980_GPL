.PHONY: tags info

export CUR_DIR:=$(PWD)
export SRC_ROOT:=$(abspath $(CUR_DIR)/../../..)
export BLD_ROOT:=$(CUR_DIR)/.___build___
export BLD_DIR=$(subst $(SRC_ROOT),$(BLD_ROOT),$(CUR_DIR))
export MODULE_NAME=mtlk

include Makefile.inc

#new implementation
all: info toolchain_info
	@$(MAKE) $(KERNEL_MAKE_EXT_PARAMS) -C $(KERNELDIR) SUBDIRS=$(PWD) modules

objdump: all
	@echo "Creating objdump ($(MODULE_NAME).ko.objdump)..."
	@$(CROSS_COMPILE)objdump -S -d  $(MODULE_NAME).ko > $(MODULE_NAME).ko.objdump
	@tar -cjf $(MODULE_NAME).ko.objdump.tar.bz2 $(MODULE_NAME).ko.objdump
	@echo "Done!"

DRV_INFO_NAME=drv_info.c
NEW_DRV_INFO_NAME=drv_info.c.new

toolchain_info: info $(DRV_INFO_NAME)
	@awk '/\".+\"/{print gensub(/.*\"(.+)\".*/, "\\1", 1, $$0)}' < $(DRV_INFO_NAME) > toolchain_info

info:
	@echo "const char *mtlk_drv_info[] = {" > $(NEW_DRV_INFO_NAME)
ifeq ($(MAKELEVEL),0) # 1st level makefile can get top-level PID
	@echo "  \"CMD_LINE=`cat /proc/$$PPID/cmdline`\"," >> $(NEW_DRV_INFO_NAME)
else  # next level makefile should receive top-level PID as parameter
	@echo "  \"CMD_LINE(m)=`cat /proc/$(PPID)/cmdline`\"," >> $(NEW_DRV_INFO_NAME)
endif
	@echo "  \"CFLAGS=$(subst \",',$(CFLAGS))\"," >> $(NEW_DRV_INFO_NAME)
	@echo "  \"KERNELDIR=$(KERNELDIR)\"," >> $(NEW_DRV_INFO_NAME)
	@echo "  \"CROSS_COMPILE=$(CROSS_COMPILE)\"," >> $@.new
	@echo "  \"HOSTNAME=$(HOSTNAME)\"," >> $(NEW_DRV_INFO_NAME)
	@echo "  \"CROSS_COMPILE=$(CROSS_COMPILE)\"," >> $(NEW_DRV_INFO_NAME)
	@echo "  \"ARCH=$(ARCH)\"," >> $(NEW_DRV_INFO_NAME)
	@echo "  \"CONFIG_HW_PCI=$(CONFIG_HW_PCI)\"," >> $(NEW_DRV_INFO_NAME)
ifneq ($(CUSTOM_DRV_INFO), )
	@echo "$(CUSTOM_DRV_INFO)" | awk '{for (i=1; i<=NF; i++){ print "  \"" $$i "\","; }}' >> $(NEW_DRV_INFO_NAME)
endif
	@echo "  (const char *)0" >> $(NEW_DRV_INFO_NAME)
	@echo "};" >> $(NEW_DRV_INFO_NAME)
	@echo "" >> $(NEW_DRV_INFO_NAME)
	@diff -q $(NEW_DRV_INFO_NAME) $(DRV_INFO_NAME) >/dev/null 2>&1 || cp -f $(NEW_DRV_INFO_NAME) $(DRV_INFO_NAME)
	@rm -f $(NEW_DRV_INFO_NAME)

# Module specific targets
tags:
	ctags --recurse=yes . ../../driver/shared/ ../../shared_mac/ $(KERNELDIR)/include/

# Before generating documentation, update DoxyGen and GraphViz packages:
# "doxygen --version": v.1.5.1
# "dot -V": v.2.8
doc:
	rm -rf doc_doxygen
	doxygen

clean:
	rm -rf $(BLD_ROOT) tags doc_doxygen sources logmacros.*

