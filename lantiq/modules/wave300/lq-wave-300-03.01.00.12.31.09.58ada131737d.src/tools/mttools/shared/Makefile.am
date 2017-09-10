# $Id$

include $(top_srcdir)/Makefile.am.target_apps

AM_CPPFLAGS = $(MTLK_TARGET_APPS_CPPFLAGS)

noinst_LIBRARIES = libmttools.a

libmttools_a_SOURCES = $(rebased_sources)/driver_api.c \
                       $(rebased_sources)/mtlk_cli_client.c \
                       $(rebased_sources)/mtlk_cli_server.c \
                       $(rebased_sources)/linux/mtlk_cli_port.c

libmttools_INCLUDES = $(wireless_shared_srcdir) \
                      $(tools_shared_srcdir) \
                      $(tools_shared_srcdir)/linux
                      
rebase_component = libmttools

include $(top_srcdir)/Makefile.am.app_rebase

