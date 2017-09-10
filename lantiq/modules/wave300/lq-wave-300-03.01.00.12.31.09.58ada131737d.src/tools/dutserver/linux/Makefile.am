# $Id$

include $(top_srcdir)/Makefile.am.target_apps

dut_PROGRAMS = dutserver
AM_CPPFLAGS = $(MTLK_TARGET_APPS_CPPFLAGS)

dutserver_SOURCES = driver_api.c dutserver.c sockets.c utils.c
dutserver_LDADD = -lpthread
