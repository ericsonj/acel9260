ifneq ($(KERNELRELEASE),)
obj-m := acel9260.o
else
KDIR := $(HOME)/linux-kernel-labs/src/linux
all:
ifeq ($(ARCH),arm)
	$(MAKE) -C $(KDIR) M=$$PWD
else
	@echo "Set ARCH=arm"
endif
endif
