KERNEL_BUILD := /home/cnexus/storage/kk_kernel/sprint/kernel
KERNEL_CROSS_COMPILE := /home/cnexus/storage/toolchains/linaro/4.8/bin/arm-linux-androideabi-

EXTRA_CFLAGS=-fno-pic

obj-m += s2w_mod.o

all:
	make -C $(KERNEL_BUILD) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) M=$(PWD) modules

clean:
	make -C $(KERNEL_BUILD) M=$(PWD) clean 2> /dev/null
	rm -f modules.order *~
