# $Id$

#This list must specify correct "high-level" build order
#i.e. shared components to be built before before components
#that may use them, helper applications to be built first


if BUILD_TOOLS_SHARED
TOOLS_SHARED_DIRS = tools/shared
else
TOOLS_SHARED_DIRS =
endif

if BUILD_UTF_SHARED
UTF_SHARED_DIRS = wireless/utf/utfmodule
else
UTF_SHARED_DIRS =
endif

SUBDIRS = $(UTF_SHARED_DIRS) \
          tools/rtlogger/logprep \
          $(TOOLS_SHARED_DIRS) \
          wireless/shared \
          tools \
          wireless
