CC = mpicc
CFLAGS = -g -m32

all: bmpFilter

bmpFilter: bmpBlackWhite.o
	$(CC) bmpBlackWhite.o bmpFilterDinamic.c -o bmpFilterDin -lm

clean:
	rm -f bmpFilterSta
