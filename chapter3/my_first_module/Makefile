HOME=/home/k2h
obj-m += my_first_modules.o

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
	sshpass -praspberry scp -o StrictHostKeyChecking=no *.ko pi@192.168.1.100:/home/pi/module-test/

