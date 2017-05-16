CC=gcc
FLAGS=-g -Wall

ramdisk: ramdisk.c 
	$(CC) $(FLAGS) `pkg-config fuse --cflags --libs` -o ramdisk ramdisk.c

clean:
	rm ramdisk
