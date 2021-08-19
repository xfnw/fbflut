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
- this writes directly to your framebuffer, which might not use (aa)rrggbb (bbggrraa is somewhat common).
  if you have alpha trailing at the end, uncomment lines `42` and `43` to allow omitting it in `PX` lines
- fbflut does not store its own buffer, the canvas is entirely stored in the framebuffer,
  so be weary of other programs writing to it as thats the only copy
