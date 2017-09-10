#!/bin/sh
APPS_NAME="br2684ctl"
if [ -e sysconfig.sh ]; then
	. sysconfig.sh
	. config.sh
	. model_config.sh
else
        echo "Application "$APPS_NAME" not configured"
        exit 0
fi                                                                                                                                       


display_info "----------------------------------------------------------------------"
display_info "-----------------------      build br2684ctl  ------------------------"
display_info "----------------------------------------------------------------------"

parse_args $@

if [ "$1" = "config_only" ] ;then
	exit 0
fi

if [ $BUILD_CLEAN -eq 1 ]; then
	make clean
	[ ! $BUILD_CONFIGURE -eq 1 ] && exit 0
fi

if [ -n "$PLATFORM_NAME" -a "$PLATFORM_NAME" = "Amazon_SE" ]; then
	IFX_CFLAGS="${IFX_CFLAGS} -DCONFIG_AMAZON_SE"
fi

if [ "$CONFIG_FEATURE_IFX_DSL_CPE_API" = "1" ]; then
	IFX_CFLAGS="${IFX_CFLAGS} -DLINUX -I${USER_IFXSOURCE_DIR}dsl_api/drv_dsl_cpe_api/src/include -I${USER_IFXSOURCE_DIR}dsl_api/dsl_cpe_control/src -DDSL_CHANNELS_PER_LINE=1"
	if [ "$IFX_CONFIG_CPU" = "AMAZON_SE" -o "$IFX_CONFIG_CPU" = "DANUBE" ]; then
        #8111001:<IFTW-linje> integrated DSL API 3.16.3
		IFX_CFLAGS="${IFX_CFLAGS} -DINCLUDE_DSL_CPE_API_DANUBE -DCONFIG_FEATURE_IFX_DSL_CPE_API"
		#IFX_CFLAGS="${IFX_CFLAGS} -DINCLUDE_DSL_CPE_API_${IFX_CONFIG_CPU} -DCONFIG_FEATURE_IFX_DSL_CPE_API"
	fi
	
fi

make AR=${IFX_AR} AS=${IFX_AS} LD=${IFX_LD} NM=${IFX_NM} CC=${IFX_CC} BUILDCC=${IFX_HOSTCC} GCC=${IFX_CC} CXX=${IFX_CXX} CPP=${IFX_CPP} RANLIB=${IFX_RANLIB} IFX_CFLAGS="${IFX_CFLAGS}" IFX_LDFLAGS="${IFX_LDFLAGS}" all
ifx_error_check $? 

${IFX_STRIP} br2684ctl
${IFX_STRIP} br2684ctld

install -d ${BUILD_ROOTFS_DIR}usr/sbin/
cp -f br2684ctl ${BUILD_ROOTFS_DIR}usr/sbin/.
cp -f br2684ctld ${BUILD_ROOTFS_DIR}usr/sbin/.
ifx_error_check $? 
