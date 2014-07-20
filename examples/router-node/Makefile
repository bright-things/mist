all: router-node
MIST=../..
APPS+=ip64-webserver network-reboot simple-udp-ping simple-rpl
CFLAGS += -DSLIP_ARCH_CONF_ENABLED=1
CFLAGS += -DSLIP_ARCH_CONF_USB=1

CFLAGS+= -DUIP_CONF_IPV6_RPL

include $(MIST)/Makefile.mist

ifndef COMPORT
COMPORT=/dev/ttyACM0
endif # COMPORT

$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)
connect-slip-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -s $(COMPORT) aaaa::1/64
