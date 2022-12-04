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
- this writes directly to your framebuffer, and uses the
  assumtion that it is little-endian to make colors go in the
  correct place if alpha is omitted. if this does not apply
  to you, add `-DALPHA_AT_END` to `CFLAGS` in the Makefile to
  allow omitting alpha in `PX` commands
- fbflut does not store its own buffer, the canvas is entirely
  stored in the framebuffer, so be weary of other programs
  writing to it as thats the only copy

