
all: bfat make_bfat

bfat: bfat.c
	gcc -O2 -Wall -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=25 -c -o bfat.o bfat.c
	gcc bfat.o -lfuse -O2 -Wall -D_FILE_OFFSET_BITS=64  -DFUSE_USE_VERSION=25 -o bfat

make_bfat: make_bfat.c
	gcc -Wall -g  -D_FILE_OFFSET_BITS=64 make_bfat.c   -o make_bfat

clean:
	rm -f *~ bfat.o bfat make_bfat
	
	