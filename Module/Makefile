obj-m += write-msr.o
#obj-m += read-msr.o
obj-m += enable-user-pmc.o
#obj-m += get-cpuid.o
#obj-m += write-msr-monster.o

#obj-m += crash.o


all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
