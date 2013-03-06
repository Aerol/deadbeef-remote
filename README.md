deadbeef-remote
===============

Remote control plugin for DeadBeeF

## Compile and Install
     make
     strip remote.so #optional
     strip client.exe #optional
     mv remote.so ~/.local/lib/deadbeef/
     Start DB and go to the plugin configuration
     Fill in IP and Port, save and restart DB

I use .exe for .gitignore

## Usage
     ./client.exe ip port command

IP and port should be self explanatory, replace command with a number between 1-7
You can see what each number does by looking at the remote.c file

## TODO
1. Write enable/disable code for socket binding?
2. ???
3. Profit!
