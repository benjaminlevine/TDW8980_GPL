CFLAGS += -Wno-multichar -Werror

ifneq (,$(findstring objdump,$(MAKECMDGOALS)))
$(warning *******************************************************************)
$(warning *** Objdump compilation! Driver is for debugging purposes only! ***)
$(warning *******************************************************************)
CFLAGS += -g
endif

EXTRA_CFLAGS += $(CFLAGS)
MTLK_CFLAGS:=$(EXTRA_CFLAGS)
export MTLK_CFLAGS
