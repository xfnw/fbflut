CC=c99

all: fbflut

fbflut: fbflut.o
	${CC} -lpthread -o fbflut fbflut.o

clean:
	rm -f fbflut.o fbflut
