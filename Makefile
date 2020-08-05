obj-m += sevtest.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc sev_demo.c -o sev_demo.out

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f sev_demo.out
	sudo rmmod sevtest 2> /dev/null

install:
	sudo insmod sevtest.ko