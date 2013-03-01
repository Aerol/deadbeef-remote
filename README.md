deadbeef-remote
===============

Remote control plugin for DeadBeeF

## Compile and Install
     gcc -shared -O2 -o remote.so remote.c
     mv remote.so ~/.local/lib/deadbeef/
     # You can build the client if you want
     gcc -O2 -o client.exe client.c

I use .exe for .gitignore

## TODO
1. Finish message processing
2. ???
3. Profit!
