# NetInput

NetInput is an easy way to send gamepad input over the network with low latency. It supports up to 4 gamepads at a time. The program assumes a single source and a single receiver. Ideal in a setup where you have a display out but no way to access USB to plug in a gamepad.

The program will ask for your target computer when starting up, if you do not want to be prompted every time please create a file called "target.txt" with the target computer ip as its contents and the port number.

For example:
```
192.168.1.31
4313
```

## Local Setup Guide

1. Install the requirements
2. Run the Player on the computer you would like to send the gamepad input to
3. Figure out the IP address of the computer running the player (open up terminal and type ipconfig)
4. Create a file called target.txt in the same folder as the Capture program on the computer you would like to capture the gamepad input from and insert add the IP of the computer running the Player in that file and the Port number
5. Run the Capture program on the computer you would like to game the gamepad input from
6. Enjoy!

## Over The Internet Setup Guide

1. Install the requirements
2. Run the Player on the computer you would like to send the gamepad input to
3. Figure out the remote IP address of the computer running the player (https://www.whatismyip.com/) and forward the required port (UDP default:4313) on your router
4. Create a file called target.txt in the same folder as the Capture program on the computer you would like to capture the gamepad input from and insert add the IP of the computer running the Player in that file and the Port number
5. Run the Capture program on the computer you would like to game the gamepad input from
6. Enjoy!

## How It Works

The Capture program iterates all gamepad states using XInput every 1ms (to reduce CPU load) and if the gamepad state has changed it sends the raw gamepad state to the Player over UDP along with the gamepad index.

The Player will see if a gamepad with that index is connected. If not, a new virtual gamepad is created. Once the virtual gamepad is available the gamepad state is updated to mirror the gamepad state of the Capture side.

## Requirements

- ViGEmBus https://github.com/ViGEm/ViGEmBus/releases
