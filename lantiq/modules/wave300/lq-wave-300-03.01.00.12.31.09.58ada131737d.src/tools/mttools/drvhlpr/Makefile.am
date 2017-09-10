# $Id$

include $(top_srcdir)/Makefile.am.target_apps

mttools_PROGRAMS = drvhlpr
AM_CPPFLAGS = $(MTLK_TARGET_APPS_CPPFLAGS)
dist_mttools_DATA=$(srcdir)/rnotes.txt

drvhlpr_SOURCES = $(rebased_sources)/arphelper.c \
                  $(rebased_sources)/drvhlpr.c \
                  $(rebased_sources)/irbponger.c \
                  $(rebased_sources)/asel.c \
                  $(rebased_sources)/ledsctrl.c \
                  $(rebased_sources)/wpsctrl.c \
                  $(builddir)/../bld_info/bld_info.c

drvhlpr_LDADD = $(builddir)/../shared/libmttools.a \
                $(tools_shared_builddir)/3rd_party/iniparser/libiniparser.a \
                $(wireless_shared_builddir)/libmtlkwls.a \
                $(tools_shared_builddir)/linux/libmtlkc.a \
                -lpthread -lrt -lm

drvhlpr_INCLUDES =  $(wireless_srcdir)/shared_mac/ \
                    $(srcdir)/../shared/ \
                    $(srcdir)/../shared/linux/ \
                    $(tools_shared_srcdir)/linux \
                    $(tools_shared_srcdir)/3rd_party/iniparser \
                    $(tools_shared_srcdir) \
                    $(wireless_shared_srcdir)/linux \
                    $(wireless_shared_srcdir)

rebase_component = drvhlpr

include $(top_srcdir)/Makefile.am.app_rebase
