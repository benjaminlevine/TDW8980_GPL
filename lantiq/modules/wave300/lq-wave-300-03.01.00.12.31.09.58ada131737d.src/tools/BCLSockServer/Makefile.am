# $Id$

include $(top_srcdir)/Makefile.am.target_apps

AM_CPPFLAGS = -I$(srcdir)/BCL/
mttools_PROGRAMS = BclSockServer
dist_mttools_DATA = $(srcdir)/scripts/bcl_util.tcl

BclSockServer_SOURCES = BCLSockServer.c \
                        mt_tftp.c \
                        BCL/mt_bcl.c \
                        BCL/mt_util.c \
                        BCL/mt_bcl_funcs.c \
                        BCL/mt_lchacc.c \
                        BCL/mt_wapi.c

BclSockServer_LDADD = -lpthread

