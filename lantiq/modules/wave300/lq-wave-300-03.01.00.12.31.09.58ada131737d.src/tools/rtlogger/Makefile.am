# $Id$

if BUILD_RTLOGGER
RTLOGGER_SUBDIRS = logserver logcnv
else
RTLOGGER_SUBDIRS =
endif

SUBDIRS = logprep $(RTLOGGER_SUBDIRS)

#logdrv build disabled because of compilation problems 
#       when source tree is configured to work without logger
#       To be enabled after sources interconnection redesign
