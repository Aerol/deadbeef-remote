deadbeef-remote
===============

Remote control plugin for DeadBeeF

## Compile and Install
     gcc -shared -O2 -o remote.so remote.c
     strip remote.so #optional
     mv remote.so ~/.local/lib/deadbeef/
     Start DB and go to the plugin configuration
     Fill in IP and Port, save and restart DB
     # You can build the client if you want
     gcc -O2 -o client.exe client.c
     strip client.exe #optional

I use .exe for .gitignore

## Usage
     ./client.exe ip port command

IP and port should be self explanatory, replace command with a number between 1-5
You can see what each number does by looking at the remote.c file

## TODO
1. Write enable/disable code for socket binding?
2. ???
3. Profit!
