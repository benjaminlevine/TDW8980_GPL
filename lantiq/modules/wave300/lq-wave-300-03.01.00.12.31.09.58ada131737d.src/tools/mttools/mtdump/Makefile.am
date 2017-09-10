# $Id$

include $(top_srcdir)/Makefile.am.target_apps

mttools_PROGRAMS = mtdump
AM_CPPFLAGS = $(MTLK_TARGET_APPS_CPPFLAGS)

mtdump_SOURCES = $(rebased_sources)/mtdump.c \
                 $(builddir)/../bld_info/bld_info.c

mtdump_LDADD = $(builddir)/../shared/libmttools.a \
               $(tools_shared_builddir)/linux/libmtlkc.a \
               $(wireless_shared_builddir)/libmtlkwls.a

mtdump_INCLUDES=$(srcdir)/../shared/ \
                $(wireless_shared_srcdir) \
                $(tools_shared_srcdir)/linux \
                $(tools_shared_srcdir)

rebase_component = mtdump

include $(top_srcdir)/Makefile.am.app_rebase
