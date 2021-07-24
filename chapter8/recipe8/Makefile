HOME=/home/k2h
obj-m += recipe8.o

KERNEL_DIR ?= $(HOME)/linux

all:
	make -C $(KERNEL_DIR) \
		ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
		M=$(PWD) modules

clean:
	make -C $(KERNEL_DIR) \
		ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
		M=$(PWD) clean

deploy:
	sshpass -praspberry scp -o StrictHostKeyChecking=no *.ko pi@192.168.1.120:/home/pi/module-test/

