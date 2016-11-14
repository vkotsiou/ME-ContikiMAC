CONTIKI_PROJECT = myproject

all: $(CONTIKI_PROJECT)

DEFINES=NETSTACK_CONF_NETWORK=my_driver

SMALL=1
CONTIKI = ../..
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
include $(CONTIKI)/Makefile.include
