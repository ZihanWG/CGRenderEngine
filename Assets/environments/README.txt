Drop equirectangular HDR files (`.hdr`) into this folder.

At startup, the engine scans `assets/environments` and loads the first `.hdr` file it finds.
If no HDR file is present, it falls back to the built-in procedural sky.
