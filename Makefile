CC = c99
PIXELSIZE ?= 32
CFLAGS ?= -DHAVE_LINUX_SECCOMP_H -D_GNU_SOURCE -DPIXELSIZE=${PIXELSIZE} -O3

all: fbflut

fbflut: fbflut.o
	${CC} ${CFLAGS} -o fbflut fbflut.o -lpthread

clean:
	rm -f fbflut.o fbflut
