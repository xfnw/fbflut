CC = c99
CFLAGS ?= -DHAVE_LINUX_SECCOMP_H -D_GNU_SOURCE -O3

all: fbflut

fbflut: fbflut.o
	${CC} ${CFLAGS} -o fbflut fbflut.o -lpthread

clean:
	rm -f fbflut.o fbflut
