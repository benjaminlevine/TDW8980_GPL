#!/bin/sh
#
# $Id$
#
# This script is intended to build all release binaries
# for specific platform.
#
# Copyright (c) 2006-2009 Metalink Broadband (Israel)
#

build_binary()
{
  if test x"$COMPONENT_NAME" = x""; then
    COMPONENT=all
  else
    COMPONENT=$COMPONENT_NAME
  fi

  (cat $SRC_TREE_ROOT/support/release/configs/$PLATFORM.config \
       $SRC_TREE_ROOT/support/release/configs/$COMPONENT.config > $SRC_TREE_ROOT/mini.config && \
   make -C $SRC_TREE_ROOT defconfig MINICONFIG=$SRC_TREE_ROOT/mini.config) || return 1

  BUILDDIR=$SRC_TREE_ROOT/`$SRC_TREE_ROOT/support/cfghlpr.sh $SRC_TREE_ROOT/.config get_bld_dir`

  (cd $BUILDDIR && \
   rm -rf binaries/ && \
   make install) || return 1

  cp -R $BUILDDIR/binaries/* $RES_DIR || return 2

  return 0
}

PLATFORM=$1
RES_DIR=$2
COMPONENT_NAME=$3
SRC_TREE_ROOT=.

if test x"$PLATFORM" = x"" || test x"$RES_DIR" = x""
then
    echo "Usage: "`basename $0`" <platform> <res_dir> [<component_name>]";
    exit 2;
fi

mkdir -p "$RES_DIR" || exit 1;
build_binary || exit 1;


