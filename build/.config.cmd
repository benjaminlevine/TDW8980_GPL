deps_config := \
	/home/yangxu/tplink/lantiq_lettuce/tpbsp/build/../build/sysdeps/linux/Config.in

.config include/config.h: $(deps_config)

$(deps_config):
