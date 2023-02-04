# NetInput

NetInput is an easy way to send gamepad input over the network with low latency. It supports up to 4 gamepads at a time. The program assumes a single source and a single receiver. Ideal in a setup where you have a display out but no way to access USB to plug in a gamepad.

The program will ask for your target computer when starting up, if you do not want to be prompted every time please create a file called "target.txt" with the target computer ip as its contents.

For example:
```
192.168.1.31
```

## Local Setup Guide

1. Install the requirements
2. Run the Player on the computer you would like to send the gamepad input to
3. Figure out the IP address of the computer running the player (open up terminal and type ipconfig)
4. Create a file called target.txt in the same folder as the Capture program on the computer you would like to capture the gamepad input from and insert add the IP of the computer running the Player in that file
5. Run the Capture program on the computer you would like to game the gamepad input from
6. Enjoy!

## Over The Internet Setup Guide

1. Install the requirements
2. Run the Player on the computer you would like to send the gamepad input to
3. Figure out the remote IP address of the computer running the player (https://www.whatismyip.com/) and forward the required port (UDP 4313) on your router
4. Create a file called target.txt in the same folder as the Capture program on the computer you would like to capture the gamepad input from and insert add the IP of the computer running the Player in that file
5. Run the Capture program on the computer you would like to game the gamepad input from
6. Enjoy!

## How It Works

The Capture program iterates all gamepad states using XInput every 1ms (to reduce CPU load) and if the gamepad state has changed it sends the raw gamepad state to the Player over UDP along with the gamepad index.

The Player will see if a gamepad with that index is connected. If not, a new virtual gamepad is created. Once the virtual gamepad is available the gamepad state is updated to mirror the gamepad state of the Capture side.

## Requirements

- ViGEmBus https://github.com/ViGEm/ViGEmBus/releases

# Thank  usertoroot 
# 1.13 By QeeAI

2023.2.2 
1. Server (Custom Port)      :  netinput.paly.exe 0-65536 (Default 4313)
2. Client (Custom IPV4/Port) :  netinput.capture.exe 192.168.1.31 (for example) 0-65536 (Default 4313) ,  "target.txt" never use .
3. Support Xbox360/X1S/XSS Vibration 
4. GamePads depended By Client IP ，Port, and Local Xbox controller Index Id 。
5. Added UDP Client HeartBreak every 5s, TimeOut 15s to break.

2023年2月2日
1. 服务端 (自定义端口)    :  netinput.paly.exe 4313（0-65536）
2. 客户端 (自定义IP和端口) :  netinput.capture.exe 192.168.1.31（自定义）  4313（0-65536） ， target.txt 不再使用。
3. 现在可以完美支持Xbox360/X1S/XSS手柄震动。
4. 服务端的手柄分组现在按IP,端口和本地XBOX手柄序号共同决定。
5. 增加客户端手柄的心跳连接每5秒1次，超时15秒自动断开。



# 1.14 By QeeAI

2023.2.4 
GamePads depended By Client Mac Address and Xbox controller Index Id now.

2023年2月4日
服务端的手柄分组现在按客户端MAC地址和XBOX手柄序号共同决定。