# Makefile2.6
PWD = $(shell pwd)
KERNEL_SRC = ../../../
obj-m := hdftl.o
obj-m += nand_test.o
hdftl-objs := ftl_globalvar.o ftl_mem.o sep_nand.o ftl_format.o flashdev.o test_driver.o libhandytec-ftl-uc4.1.0.lo
all: 
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	rm -rf *.o *.ko *.order *.mod.c *symvers
