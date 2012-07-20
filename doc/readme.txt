1.ready the device

1.1 create the bootloader:
	cd u-boot;
	make sep0611_db;
	make -j2;
	now, the u-boot.bin is ready.

1.2 create the kernel image:
	cd kernel;
	make toy_defconfig;
	make uImage -j2;
	now, the uImage is ready in arch/unicore/boot.

1.3 install the system:
	first boot into u-boot, and exexute:
	tftp 0x40008000 uImage;
	nand erase 0x100000 0x800000;
	nand write 0x40008000 0x100000 0x800000;

1.4 reboot

2.ready the controller

2.1 install the wificar.apk in you android phone.

2.2 when the red led on device is twinkling, open the apk.

2.3 check you apk's setting, ensure its host IP is 192.168.100.100. if you has done it, skip.

2.4 when you look the pictrue, it is successful. you can control the device via the apk.

2.5 enjoy it.
