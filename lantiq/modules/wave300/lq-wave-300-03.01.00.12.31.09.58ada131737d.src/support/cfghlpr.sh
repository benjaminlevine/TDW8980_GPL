#!/bin/sh

#WARNING: configure script must be called by relative path, not absolute
#         in other case, makefiles will receive absolute pathes in top_srcdir and 
#         srcdir variables and won't operate properly.
#         This is a long-running bug between autoconf and some shells (at least bash)
#         See http://sourceware.org/ml/automake/2000-03/msg00151.html for additional info.

CONFIG_FILE=$1
WORK_MODE=$2

. $PWD/$CONFIG_FILE || EXIT 1

if [ x"$WORK_MODE" = x"get_bld_tree_cfg" ]; then
echo builds/$CONFIG_ENVIRONMENT_NAME/.config builds/$CONFIG_ENVIRONMENT_NAME/.config.h
exit 0;
fi

if [ x"$WORK_MODE" = x"get_bld_dir" ]; then
echo builds/$CONFIG_ENVIRONMENT_NAME
exit 0;
fi

( cd `readlink -f $PWD` && \
  rm -rf builds/$CONFIG_ENVIRONMENT_NAME && \
  mkdir -p builds/$CONFIG_ENVIRONMENT_NAME && \
  cd builds/$CONFIG_ENVIRONMENT_NAME && \
  cp -f ../../.config . && \
  awk -f ../../wireless/scripts/make_cfg_header.awk < .config > .config.h && \
  ../../configure --host $CONFIG_HOST_TYPE \
                  --build=`../../config.guess` \
                  --with-environment=$CONFIG_ENVIRONMENT_NAME \
                  --with-app-toolchain=$APP_TOOLCHAIN_DIR \
                  --with-kernel=$KERNEL_DIR \
                  --with-kernel-cross-compile=$KERNEL_CROSS_COMPILE \
                  --prefix=`readlink -f .`/binaries && \
  exit 0 ) || ( rm -f .config .config.h && exit 1 )

