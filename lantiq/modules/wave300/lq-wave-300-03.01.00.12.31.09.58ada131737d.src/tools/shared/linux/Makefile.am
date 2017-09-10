# $Id$

include $(top_srcdir)/Makefile.am.target_apps

AM_CPPFLAGS = -I$(srcdir)/../ \
              -I$(wireless_shared_srcdir) \
              -I$(wireless_shared_srcdir)/linux \
              -I$(tools_shared_srcdir)/3rd_party/iniparser \
              -I$(tools_srcdir)/mttools/shared \
              $(MTLK_TARGET_APPS_CPPFLAGS)

noinst_LIBRARIES = libmtlkc.a libmtlkcpp.a
libmtlkc_a_SOURCES = mtlkirbm_u.c \
                     mtlknlink.c \
                     osal_osdep.c \
                     utils.c \
                     $(srcdir)/../mtlkcontainer.c \
                     $(srcdir)/../mtlk_socket.c \
                     $(srcdir)/../argv_parser.c

libmtlkcpp_a_SOURCES = $(srcdir)/../aux_utils.cpp  \
                       $(srcdir)/../CmdLine.cpp  \
                       $(srcdir)/../Debug.cpp  \
                       $(srcdir)/../ParamInfo.cpp
