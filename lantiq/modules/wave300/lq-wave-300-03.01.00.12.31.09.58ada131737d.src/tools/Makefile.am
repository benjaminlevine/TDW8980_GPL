# $Id$

if BUILD_TOOLS_SHARED
TOOLS_SHARED_DIRS = shared
else
TOOLS_SHARED_DIRS =
endif

if BUILD_DUTSERVER
DUTSERVER_DIRS = dutserver
else
DUTSERVER_DIRS =
endif

if BUILD_MTTOOLS
BCL_DIRS = BCLSockServer
else
BCL_DIRS =
endif

SUBDIRS = $(TOOLS_SHARED_DIRS) mttools rtlogger $(DUTSERVER_DIRS) $(BCL_DIRS)

if BUILD_HELPER_TOOLS

include $(top_srcdir)/Makefile.am.common

module_canonical_name = oom_killer_hook_ko
oom_killer_hook_ko_src_dir = oom_killer_hook
oom_killer_hook_ko_binary_name = oom_killer_hook.ko

hlprtools_PROGRAMS = oom_killer_hook.ko

dist_hlprtools_DATA = $(srcdir)/$(oom_killer_hook_ko_src_dir)/rnotes.txt \
                      $(builddir)/$(oom_killer_hook_ko_src_dir)/toolchain_info \
                      $(top_builddir)/.config

.PHONY : $(oom_killer_hook_ko_binary_name)
oom_killer_hook_ko_SOURCES =

include $(top_srcdir)/Makefile.am.module_build

oom_killer_hook_ko_LINK = $(oom_killer_hook_ko_BUILD_CMD)

clean-local:
	$(oom_killer_hook_ko_CLEAN_CMD)

endif