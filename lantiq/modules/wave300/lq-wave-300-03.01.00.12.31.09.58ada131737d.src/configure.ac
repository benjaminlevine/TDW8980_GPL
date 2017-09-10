dnl $Id$

AC_PREREQ(2.63)
AC_INIT([mtlk], [3.1.0], [])
AC_CONFIG_SRCDIR([mtlk_version])
AC_CONFIG_HEADER([config.h])

AC_CANONICAL_HOST

AC_MSG_CHECKING([for toolchain to use])
AC_ARG_WITH(app-toolchain,
            AS_HELP_STRING([--with-app-toolchain],
                           [use specified aplication toolchain instead of default one]),
            [override_app_toolchain=$withval],
            [unset override_app_toolchain])
AS_IF([test "x$override_app_toolchain" = x],
      [AC_MSG_RESULT(default)],
      [AC_MSG_RESULT($override_app_toolchain)])


AC_MSG_CHECKING([for kernel to use])
AC_ARG_WITH(kernel,
            AS_HELP_STRING([--with-kernel],
                           [use specified kernel instead of default one]),
            [override_kernel=$withval],
            [unset override_kernel])
AS_IF([test "x$override_kernel" = x],
      [AC_MSG_RESULT(default)],
      [AC_MSG_RESULT($override_kernel)])

AC_MSG_CHECKING([for kernel compiler prefix to use])
AC_ARG_WITH(kernel-cross-compile,
            AS_HELP_STRING([--with-kernel-cross-compile],
                           [use specified kernel compiler prefix instead of default one]),
            [override_kernel_cross_compile=$withval],
            [unset override_kernel_cross_compile])
AS_IF([test "x$override_kernel_cross_compile" = x],
      [AC_MSG_RESULT(default)],
      [AC_MSG_RESULT($override_kernel_cross_compile)])

AC_MSG_CHECKING([for environment configuration file])

AC_ARG_WITH([environment],
            [AS_HELP_STRING([--with-environment],
            [specify environment name to use])],
            [environment_name=$withval],
            [environment_name=x86-linux26])

AC_MSG_RESULT([${environment_name}])

. $(pwd)/.config

AC_MSG_CHECKING([for rflib])

AM_CONDITIONAL([USE_PREBUILT_RFLIB],
               [test "x${CONFIG_USE_PREBUILT_RFLIB}" = xy])

AM_COND_IF(USE_PREBUILT_RFLIB, [
  __rflib_name="mtlk_rflib.a"
  __rflib_path_src="${PWD}/${CONFIG_RFLIB_PATH}/${__rflib_name}"
  __rflib_dir_dst="${PWD}/wireless/rflib"
  __rflib_path_dst="${__rflib_dir_dst}/${__rflib_name}"

  AS_IF([test -f "${__rflib_path_src}"],
  [
    mkdir -p "${__rflib_dir_dst}" && \
    cp -s "${__rflib_path_src}" "${__rflib_path_dst}"

    AS_IF([test -f "${__rflib_path_dst}"],
    [
      AC_MSG_RESULT([use prebuilt RF library '${__rflib_path_src}'])
    ],[
      AC_MSG_ERROR([Cannot prepare RF library '${__rflib_path_dst}'])
    ])
  ],[
    AC_MSG_ERROR([Missing prebuilt RF library '${__rflib_path_src}'])
  ])
],
[
  AC_MSG_RESULT([build it ourselves])
])

get_toolchain_path()
{
  echo ${override_app_toolchain:-$DEFAULT_TOOLCHAIN_PATH}
}

get_kernel_dir()
{
  echo ${override_kernel:-$DEFAULT_KERNEL_DIR}
}

get_kernel_cross_compile()
{
  echo ${override_kernel_cross_compile:-$DEFAULT_KERNEL_CROSS_COMPILE}
}

. ${srcdir}/support/${environment_name}.env

AC_SUBST(CUSTOM_APP_ENV_INFO)
AC_SUBST([APP_TOOLCHAIN_PATH], [$(get_toolchain_path)])

kernel_dir=$(get_kernel_dir)

# MTLK_CONDITIONAL_COMPONENT(CONDITIONAL-VAR, CONFIG-VAR, COMPONENT-NAME)
# --------------------------------------
AC_DEFUN([MTLK_CONDITIONAL_COMPONENT],
         [{ AM_CONDITIONAL([$1], [test "x${[$2]}" = xy ])
            AC_MSG_CHECKING([whether to build [$3]])
            if test x${[$2]} = xy ; then
               AC_MSG_RESULT([yes])
               else
                  AC_MSG_RESULT([no])
            fi;}])

MTLK_CONDITIONAL_COMPONENT([BUILD_DRIVER], [CONFIG_LIN_DRV], [the wireless driver])

MTLK_CONDITIONAL_COMPONENT([BUILD_MTTOOLS], [CONFIG_LIN_MTTOOLS], [the MTTools])

MTLK_CONDITIONAL_COMPONENT([BUILD_UTF], [CONFIG_LIN_UTF], [unit tests])

MTLK_CONDITIONAL_COMPONENT([BUILD_UTF_SELFTEST], [CONFIG_LIN_UTF_SELFTEST],
                           [UTF verification components])

MTLK_CONDITIONAL_COMPONENT([BUILD_RTLOGGER], [CONFIG_LIN_RTLOG_COMPONENTS],
                           [remote logging tools])

MTLK_CONDITIONAL_COMPONENT([BUILD_DUTSERVER], [CONFIG_LIN_DUTSERVER],
                           [the DUT server])

MTLK_CONDITIONAL_COMPONENT([BUILD_HELPER_TOOLS], [CONFIG_LIN_HELPER_TOOLS],
                           [helper tools])

AM_CONDITIONAL([BUILD_UTF_SHARED],
               [test "x${CONFIG_LIN_UTF_SELFTEST}" = xy || test "x${CONFIG_LIN_UTF}" = xy])

AM_CONDITIONAL([BUILD_TOOLS_SHARED],
               [test "x${CONFIG_LIN_UTF_SELFTEST}" = xy || \
                test "x${CONFIG_LIN_UTF}" = xy || \
                test "x${CONFIG_LIN_RTLOG_COMPONENTS}" = xy || \
                test "x${CONFIG_LIN_MTTOOLS}" = xy \
              ])

AM_CONDITIONAL([BUILD_WIRELESS_SHARED],
               [test "x${CONFIG_LIN_UTF_SELFTEST}" = xy || \
                test "x${CONFIG_LIN_UTF}" = xy || \
                test "x${CONFIG_LIN_RTLOG_COMPONENTS}" = xy || \
                test "x${CONFIG_LIN_MTTOOLS}" = xy \
              ])

AC_SUBST(cxx_ldadd)
AC_SUBST(kernel_dir)
AC_SUBST(kernel_cross_compile, $(get_kernel_cross_compile))

if test "x$kernel_arch" != "x" ; then
  kernel_arch_var="ARCH=$kernel_arch"
else
  kernel_arch_var=""
fi

AC_SUBST(kernel_arch_var)

AM_INIT_AUTOMAKE([foreign])

dnl For some reason configure script uses $ECHO variable
dnl which is empty. Probably bug in autoconf
AC_PATH_PROG([ECHO], [echo], [echo])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB

dnl Variables replacements to filter compiler/tools output
dnl with mtlkfront.sh
rebaser_subdir=.rebased
AC_SUBST(rebaser_subdir)

CC="`cd $srcdir; pwd`/support/mtlkfront.sh ${rebaser_subdir} $CC"
CXX="`cd $srcdir; pwd`/support/mtlkfront.sh ${rebaser_subdir} $CXX"

cc_pic=-fPIC
AC_SUBST(cc_pic)
cc_shared=-shared
AC_SUBST(cc_shared)

dnl Does this compiler have built-in functions for atomic memory access?
AC_MSG_CHECKING([if platform supports built-in atomics])
AC_TRY_LINK(,
 [
   int variable = 1;
   return (__sync_add_and_fetch(&variable, 1)
           && __sync_sub_and_fetch(&variable, 1)
             && __sync_lock_test_and_set(&variable, 1)) ? 1 : 0;
 ],
 [
   AC_MSG_RESULT([yes])
   AC_DEFINE(HAVE_BUILTIN_ATOMIC, 1, [Define to 1 if gcc supports builin atomics])
 ],
 [
   AC_MSG_RESULT([no])
 ])

AC_DEFINE(LINUX, 1, [Define as 1 to tell that we are building on Linux])

AC_CONFIG_FILES([Makefile
		 tools/Makefile
		 tools/dutserver/Makefile
		 tools/dutserver/linux/Makefile
		 tools/shared/Makefile
		 tools/shared/3rd_party/Makefile
		 tools/shared/3rd_party/iniparser/Makefile
		 tools/shared/linux/Makefile
		 wireless/Makefile
		 wireless/shared/Makefile
		 wireless/shared/utwls/Makefile
		 wireless/shared/utwls/user/Makefile
		 wireless/driver/Makefile
		 wireless/driver/linux/Makefile
		 wireless/driver/shared/Makefile
		 wireless/driver/shared/utshared/Makefile
		 tools/rtlogger/Makefile
		 tools/rtlogger/logprep/Makefile
		 tools/rtlogger/logserver/Makefile
		 tools/rtlogger/logserver/linux/Makefile
		 tools/rtlogger/logcnv/Makefile
		 tools/rtlogger/logdrv/Makefile
		 tools/mttools/Makefile
		 tools/mttools/bld_info/Makefile
		 tools/mttools/shared/Makefile
		 tools/mttools/drvhlpr/Makefile
		 tools/mttools/mtdump/Makefile
		 tools/mttools/mtlk_cli/Makefile
		 tools/mttools/mtlk_cli_dbg_srv/Makefile
		 tools/BCLSockServer/Makefile
		 wireless/utf/Makefile
		 wireless/utf/utfrunner/Makefile
		 wireless/utf/utfmodule/Makefile
		 wireless/utf/utfmodule/linuser/Makefile
		 wireless/utf/utfselftest/Makefile
		 wireless/utf/utfselftest/utfst1/Makefile
		 wireless/utf/utfselftest/utfst1/user/Makefile
		 wireless/utf/utfselftest/utfst2/Makefile
		 wireless/utf/utfselftest/utfst2/user/Makefile
                 ])
AM_COND_IF(USE_PREBUILT_RFLIB, [], [
  AC_CONFIG_FILES([
    wireless/rflib/Makefile
  ])
])
AC_OUTPUT
