CONTIKI_PROJECT =  observed-sender sender-node root-node sender-node
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
PROJECT_SOURCEFILES += ../dualmotes.c
MAKEFLAGS += -j
all:$(CONTIKI_PROJECT)

CONTIKI = ../../..

ifndef TARGET
TARGET=zoul
BOARD=firefly-reva
endif

ifneq ($(TARGET),zoul)
$(error Software is platform specific, only for Zolertia Zoul.)
endif

#MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC
MAKE_NET = MAKE_NET_NULLNET
include $(CONTIKI)/Makefile.include
