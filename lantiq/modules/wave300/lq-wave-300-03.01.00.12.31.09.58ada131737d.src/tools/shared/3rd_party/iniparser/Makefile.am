# $Id$

include $(top_srcdir)/Makefile.am.target_apps

AM_CPPFLAGS = -I$(tools_shared_srcdir) \
              -I$(tools_shared_srcdir)/linux \
              -I$(wireless_shared_srcdir) \
              $(MTLK_TARGET_APPS_CPPFLAGS)

noinst_LIBRARIES = libiniparser.a
libiniparser_a_SOURCES = dictionary.c iniparser.c
