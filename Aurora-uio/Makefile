CC=gcc

obj-m += uio_aurora.o

all: kmod app

kmod:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

app:
	$(CC) -o uio_user uio_user.c

kmod-clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

app-clean:
	rm -f uio_user

clean: kmod-clean app-clean

