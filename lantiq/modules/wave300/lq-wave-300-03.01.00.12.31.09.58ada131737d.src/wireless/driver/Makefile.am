# $Id$

SUBDIRS=linux shared

if BUILD_DRIVER

include $(top_srcdir)/Makefile.am.common

module_canonical_name = mtlk_ko
mtlk_ko_src_dir = linux
mtlk_ko_binary_name = mtlk.ko

driver_PROGRAMS = mtlk.ko

dist_driver_DATA = $(srcdir)/$(mtlk_ko_src_dir)/rnotes.txt \
                   $(builddir)/$(mtlk_ko_src_dir)/toolchain_info \
                   $(srcdir)/$(mtlk_ko_src_dir)/LICENSE.txt \
                   $(top_builddir)/.config

.PHONY : $(mtlk_ko_binary_name)
mtlk_ko_SOURCES =

include $(top_srcdir)/Makefile.am.module_build

mtlk_ko_LINK = $(mtlk_ko_BUILD_CMD)
clean-local:
	$(mtlk_ko_CLEAN_CMD)

endif