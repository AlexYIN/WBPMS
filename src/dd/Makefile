CROSS_COMPILE=
ARCH:=x86
CC:=$(CROSS_COMPILE)gcc
LD:=$(CROSS_COMPILE)ld
PWD=$(shell pwd)
obj-m = blood.o 
module-objs = blood.o 

KDIR = /usr/src/linux-source-2.6.32/
PWD = $(shell pwd)
default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	rm -rf  *.o *.ko *~ *.mod.c *.symvers *.order

