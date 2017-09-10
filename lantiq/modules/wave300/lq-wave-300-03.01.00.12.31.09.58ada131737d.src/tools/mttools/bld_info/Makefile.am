# $Id$

include $(top_srcdir)/Makefile.am.common

dist_mttools_DATA=toolchain_info $(top_builddir)/.config

toolchain_info: bld_info.c
	@awk '/\".+\"/{print gensub(/.*\"(.+)\".*/, "\\1", 1, $$0)}' < bld_info.c > toolchain_info

bld_info.c:
	@echo "const char *mtlk_bld_info[] = {" > bld_info.c
	@echo "  \"CMD_LINE=`cat /proc/$$PPID/cmdline`\"," >> bld_info.c
	@echo "  \"CMD_LINE(m)=`cat /proc/$(PPID)/cmdline`\"," >> bld_info.c
	@echo "  \"PATH=$(PATH)\"," >> bld_info.c
	@echo "  \"CC=$(CC)\"," >> bld_info.c
	@echo "  \"LD=$(LD)\"," >> bld_info.c
	@echo "  \"INCLUDE=$(INCLUDE)\"," >> bld_info.c
	@echo "  \"CFLAGS=$(subst \",',$(CFLAGS))\"," >> bld_info.c
	@echo "  \"LIBPATH=$(LIBPATH)\"," >> bld_info.c
	@echo "  \"LIB=$(LIB)\"," >> bld_info.c
	@echo "$(CUSTOM_DRV_INFO)" | awk '{for (i=1; i<=NF; i++){ print "  \"" $$i "\","; }}' >> bld_info.c
	@echo "  (const char *)0" >> bld_info.c
	@echo "};" >> bld_info.c
	@echo "" >> bld_info.c

