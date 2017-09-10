# $Id$

include $(top_srcdir)/Makefile.am.target_apps

if BUILD_UTF
SUBDIRS = utwls
else
SUBDIRS =
endif

if BUILD_WIRELESS_SHARED

AM_CPPFLAGS = -I$(srcdir)/../ \
              -I$(wireless_srcdir)/shared_mac/ \
              -I$(tools_shared_srcdir)/ \
              -I$(tools_shared_srcdir)/linux/ \
              $(MTLK_TARGET_APPS_CPPFLAGS)

noinst_LIBRARIES = libmtlkwls.a

libmtlkwls_a_SOURCES = mem_leak.c  \
                       mtlk_algorithms.c  \
                       mtlkirb.c  \
                       mtlkmetrics.c  \
                       mtlk_objpool.c  \
                       mtlkstartup.c  \
                       mtlkstrtok.c  \
                       pattern.c

endif