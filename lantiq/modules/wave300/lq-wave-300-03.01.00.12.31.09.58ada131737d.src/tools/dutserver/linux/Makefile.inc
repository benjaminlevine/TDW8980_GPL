CFLAGS += -DMTLK_PACKAGE_VERSION=\"1.0\"
CFLAGS += -DMTCFG_DEBUG

# Files that are needed to compile this project
FILES = dutserver.o utils.o driver_api.o sockets.o

# Makefile syntax:
# Project name : dependencies
#  command line

.PHONY: all romfs tags clean

all: .depend dutserver

dutserver: $(FILES)
	$(CC) $(CFLAGS) $(INCLUDE) -o dutserver $(FILES) $(LIBPATH) $(LIB)

dutserver.o: dutserver.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $<

utils.o: utils.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $<

cqueue.o: cqueue.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $<

driver_api.o: driver_api.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $<

sockets.o: sockets.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $<
  
romfs:
	$(ROMFSINST) dutserver /bin/dutserver

tags:
	ctags --recurse=yes . /usr/include

clean:
	rm -rf *.o .depend tags dutserver

.depend:
	touch .depend
	./mkdep -c '$(CC)' -f .depend $(CFLAGS) $(FILES:.o=.c)

-include .depend
