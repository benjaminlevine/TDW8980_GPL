DHCPD_SRC=dhcps.c icmp_check.c relay.c static_leases.c arpping.c socket.c dhcps_linux_helper.c dhcps_helper.c serverpacket.c common.c clientsocket.c packet.c options.c leases.c br_port.c
DHCPD_OBJS=$(DHCPD_SRC:.c=.o) 

DHCPC_SRC=dhcpc.c dhcpc_init_linux.c options.c packet.c script.c socket.c arpping.c clientpacket.c clientsocket.c common.c
DHCPC_OBJS=$(DHCPC_SRC:.c=.o) 

CFLAGS = -I$(OS_LIB_PATH)/include -D__LINUX_OS_FC__
LDFLAGS = -L$(OS_LIB_PATH)/ -los -lpthread -lrt

.PHONY:dhcp
dhcp:dhcpd dhcpc

dhcpd: $(DHCPD_OBJS) 
	$(CC) $(DHCPD_OBJS) $(CFLAGS) $(LDFLAGS) -o dhcpd  && $(STRIP) dhcpd && ls -l dhcpd

dhcpc: $(DHCPC_OBJS) 
	$(CC) $(DHCPC_OBJS) $(CFLAGS) $(LDFLAGS) -o dhcpc  && $(STRIP) dhcpc && ls -l dhcpc
	
ifeq ($(FULL_COMPILE), y)
sinclude $(DHCPD_SRC:.c=.d)
sinclude $(DHCPC_SRC:.c=.d)
endif	

%.o: ./src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.d: ./src/%.c
	@$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.PHONY: clean
clean:
	$(RM) $(DHCPC_SRC:.c=.d) $(DHCPC_OBJS) $(DHCPD_SRC:.c=.d) $(DHCPD_OBJS) dhcpd dhcpc
	
