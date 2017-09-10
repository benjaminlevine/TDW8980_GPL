#!/bin/sh

DEP_FILE=$1
SRC_ROOT=`readlink -f $2`
BLD_ROOT=`readlink -f $3`
BLD_DIR=`readlink -f $4`
MODULE_NAME=$5

SCD_FNAME=$BLD_DIR/$MODULE_NAME.scd

is_c_file()
{
  echo $1 | awk "/.*\.c$/ { exit 0; } ; { exit 1; }" &&
  echo -n y &&
  return 0;
  
  echo -n n
  return 1;
}

check_mtlk_inc_c_file()
{
  awk "/^[[:blank:]]*\#include[[:blank:]]+[\"\<]mtlkinc\.h[\"\>]/ { exit 0; } \
       /^[[:blank:]]*\#include/ { print NR; exit 1; }" < $1 &&
  return 0
  
  return 1
}

check_mtlk_inc_h_file()
{
  awk "/^[[:blank:]]*\#include[[:blank:]]+[\"\<]mtlkinc\.h[\"\>]/ { print NR; exit 1; }"  < $1 &&
  return 0
  
  return 1
}

check_mtlk_inc()
{
  if [ "`is_c_file $1`" = "y" ] ;
  then
    LINE_NO=`check_mtlk_inc_c_file $1`
  else
    LINE_NO=`check_mtlk_inc_h_file $1`
  fi

  if [ "$LINE_NO" != "" ] ;
  then
    echo $1\($LINE_NO\) : error: wrong mtlkinc.h inclusion
    return 1
  fi

  return 0
}

get_bld_path_name()
{
  SRC_PATH_NAME=$1
  echo "$SRC_PATH_NAME" | sed "s:$SRC_ROOT:$BLD_DIR:"
}

logprep_file()
{
  BLD_PATH_NAME=$1
  $BLD_ROOT/tools/rtlogger/logprep/logprep --silent --sid-no-reuse --oid 0 --workdir "$BLD_DIR" $BLD_PATH_NAME &&
  return 0
  
  return 1
}

check_logprep_result()
{
  diff -I '\<[IWE]LOG[0-9]*_*' $1 $2 &&
  return 0
  
  return 1
}

rebase_file()
{
  SRC_PATH_NAME=$1
  BLD_PATH_NAME=`get_bld_path_name $SRC_PATH_NAME`

  check_mtlk_inc $SRC_PATH_NAME &&
  mkdir -p `dirname "$BLD_PATH_NAME"` &&
  cp --remove-destination -p -u $SRC_PATH_NAME $BLD_PATH_NAME &&
  logprep_file "$BLD_PATH_NAME" &&
  check_logprep_result $SRC_PATH_NAME $BLD_PATH_NAME &&
  return 0;

  return 1;
}

log_file_destination()
{
  SRC_PATH_NAME=$1
  echo "$SRC_PATH_NAME" | sed "s:$BLD_ROOT:$BLD_DIR:"
}

copy_log_file()
{
  LOG_FILE=$1
  LOG_SRC=$BLD_DIR/$LOG_FILE
  LOG_DST=`log_file_destination $LOG_SRC`

  if diff -q $LOG_SRC $LOG_DST >/dev/null 2>&1; then

    #Files are the same
    rm -f $LOG_SRC && return 0;
    return 1;

  else

    #Files differ
    mkdir -p `dirname $LOG_DST` && \
    mv $LOG_SRC $LOG_DST && return 0;
    return 2;

  fi

}

generate_log_files()
{
  $BLD_ROOT/tools/rtlogger/logprep/logprep --silent --sid-no-reuse --oid 0 --workdir "$BLD_DIR" -s $SCD_FNAME &&
  copy_log_file logmacros.h && copy_log_file logmacros.c &&
  return 0
  
  return 1
}

clear_log_files()
{
  rm -f $BLD_DIR/.files $BLD_DIR/.gids $BLD_DIR/.text $BLD_DIR/.logprep_*
  
  return 0
}

get_inc_list()
{
  cat $DEP_FILE | \
  awk '{ for (i = 1; i <= NF; i++) print $i }' | \
  sed 's/\\\\//' | \
  grep "\\.[hc]" | \
  grep -v "logmacros\\.[hc]" | \
  grep -v "\\.config\\.h" | \
  sort | uniq | \
  xargs -n 1 -r readlink -f | \
  grep $SRC_ROOT
}

clear_log_files
for i in `get_inc_list`
do
  rebase_file $i || exit 1;
done
generate_log_files
