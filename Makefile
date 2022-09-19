MODULE=ttyPos
KERNEL_VER:=$(shell uname -r)
KERNEL_DIR:=/lib/modules/$(KERNEL_VER)/build
INSTALL_DIR:=/lib/modules/$(KERNEL_VER)/ttyPos

KBUILD_CFLAGS1:=$(call cc-option,-Wno-error=implicit-function-declaration,)
KBUILD_CFLAGS2:=$(call cc-option,-Wno-error=incompatible-pointer-types,)
KBUILD_CFLAGS+=$(KBUILD_CFLAGS1)
KBUILD_CFLAGS+=$(KBUILD_CFLAGS2)
obj-m := $(MODULE).o


all:
	$(MAKE) -C $(KERNEL_DIR) M=$(shell pwd) SUBDIRS=$(shell pwd) modules

clean:
	$(RM) *.o *.ko *.mod.* .*.cmd *~
	$(RM) -r .tmp_versions
	$(RM) *.order *.symvers
install: all
	install -D -m 644 ttyPos.ko $(INSTALL_DIR)/ttyPos.ko
	/sbin/depmod -a
uninstall:
	modprobe -r ttyPos ; echo -n
	$(RM) $(INSTALL_DIR)/ttyPos.ko
	/sbin/depmod -a
