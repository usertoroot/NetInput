# NetInput

NetInput is an easy way to send gamepad input over the network with low latency. It supports up to 4 gamepads at a time. The program assumes a single source and a single receiver. Ideal in a setup where you have a display out but no way to access your USB to plug in a gamepad.

The program will ask for your target computer when starting up, if you do not want to be prompted every time please create a file called "target.txt" with the target computer ip as its contents.

For example:
```
192.168.1.31
```

Requirements:
- ViGEmBus https://github.com/ViGEm/ViGEmBus/releases