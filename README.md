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
- this writes provided colors directly to your framebuffer,
  meaning it inherits the color format.
- fbflut does not store its own buffer, the canvas is
  entirely mmapped to the framebuffer, so be weary of other
  programs writing to it

## PIXELSIZE compile option
for performance reasons, the bit depth can only be changed
at compile time. if your framebuffer is not using 32 bits
per pixel, this can be changed via `PIXELSIZE` in Makefile.

