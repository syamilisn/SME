obj-m += mod.o


# my_module-objs := ssd1306.o
# my_module-y := main.o

KDIR = /lib/modules/$(shell uname -r)/build



all:
	make -C $(KDIR)  M=$(shell pwd) modules

clean:
	make -C $(KDIR)  M=$(shell pwd) clean
