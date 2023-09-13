# fbflut

framebuffer pixelflut server in C.

## running

- compile with `make`
- make sure you have read/write access
  to the framebuffer (ie put yourself in the `video` group)
- go in something that wont fight you for the framebuffer,
  ie a tty
- do `./fbflut` (or `./fbflut <port>`)
- connect with netcat or something, the default port is 1234

## caveats
- on arm and places with a weird dynamic linker, you may need
  to change `-lpthread` to `-pthread` in the Makefile
- this writes provided colors directly to your framebuffer,
  meaning it inherits the color format. if your system is
  big-endian, or if your color format is weird, you may need
  to add `-DALPHA_AT_END` to `CFLAGS` in the Makefile to
  allow omitting alpha in `PX` commands
- fbflut does not store its own buffer, the canvas is
  entirely mmapped to the framebuffer, so be weary of other
  programs writing to it

## LOSSY compile option
add `-DLOSSY` to `CFLAGS` in the Makefile to disable waiting
for incomplete line reads to finish, which can make it
slightly faster at the expense of losing a couple pixels.

