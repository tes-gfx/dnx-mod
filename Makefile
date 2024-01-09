obj-m := dnx.o
dnx-y := dnx_drv.o \
	 dnx_selftest.o \
	 dnx_gpu.o \
	 dnx_buffer.o \
	 dnx_gem.o \
	 dnx_gem_submit.o \
	 dnx_debugfs.o \
	 dnx_dbg.o 

ccflags-y := -I$(src)/../../../../driver/kernel/linux/drm-dnx  -I$(src)/../../../../interface/src -I$(src)/../drm-dnx -I$(src)/../drm-dnx/davenx

ifeq ($(DEBUG),1)
	ccflags-y += -DDEBUG=1 -g -Og
else
	ccflags-y += -DDISABLE_ASSERTIONS
endif

KERNEL_SRC := $(SDKTARGETSYSROOT)/usr/src/kernel

SRC := $(shell pwd)

.PHONY:
all:
ifeq ($(DEBUG),1)
	$(warning "!!! DEBUG BUILD !!!")
endif
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

.PHONY:
modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

.PHONY:
clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers

.PHONY:
deploy: all
	scp *.ko root@$(BOARD_IP):/home/root/
	#scp *.ko root@$(BOARD_IP):/lib/modules/4.14.130-ltsi-altera/extra/
