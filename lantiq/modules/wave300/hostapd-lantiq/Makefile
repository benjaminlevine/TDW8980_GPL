all: 
	rm -f src/crypto/tls_internal.o
	$(MAKE) -C wpa_supplicant
	rm -f src/crypto/tls_internal.o
	$(MAKE) -C hostapd

clean:
	$(MAKE) -C wpa_supplicant clean
	$(MAKE) -C hostapd clean

romfs:
	$(MAKE) -C wpa_supplicant romfs
	$(MAKE) -C hostapd romfs
