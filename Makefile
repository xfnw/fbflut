CC = c99
CFLAGS ?= -DHAVE_LINUX_SECCOMP_H -D_GNU_SOURCE

all: fbflut

fbflut: fbflut.o
	${CC} -lpthread -o fbflut fbflut.o

clean:
	rm -f fbflut.o fbflut
