# $Id$

include $(top_srcdir)/Makefile.am.target_apps

mttools_PROGRAMS = mtlk_cli
AM_CPPFLAGS = $(MTLK_TARGET_APPS_CPPFLAGS)

mtlk_cli_SOURCES = $(rebased_sources)/main.c \
                   $(builddir)/../bld_info/bld_info.c

mtlk_cli_LDADD = $(builddir)/../shared/libmttools.a \
                 $(tools_shared_builddir)/linux/libmtlkc.a \
                 $(wireless_shared_builddir)/libmtlkwls.a

mtlk_cli_INCLUDES=$(srcdir)/../shared/ \
                  $(srcdir)/../shared/linux \
                  $(wireless_shared_srcdir) \
                  $(tools_shared_srcdir)/linux \
                  $(tools_shared_srcdir)

rebase_component = mtlk_cli

include $(top_srcdir)/Makefile.am.app_rebase
