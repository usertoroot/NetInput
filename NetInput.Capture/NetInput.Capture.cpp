#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <XInput.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <objbase.h>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <string>
#include <stdint.h>
#include <fstream>


#define PAD_OUT_SIZE 5
#define PAD_ONLINE 0x00
#define PAD_VIBRA 0x01

SOCKET sock;
struct sockaddr_in addr;
XINPUT_STATE lastSentInputStates[XUSER_MAX_COUNT];

void CheckControllerMessage()
{ 
	while (true)
	{
		if (GetKeyState(VK_ESCAPE) & 0x8000)
			break;
		
		char packet[PAD_OUT_SIZE];
		int addr_size = sizeof(addr);
		int bytesReceived = recvfrom(sock, (char*)&packet, sizeof(packet), 0, (struct sockaddr*)&addr, &addr_size);		
		if (bytesReceived == PAD_OUT_SIZE){	
			uint8_t i = (uint8_t)packet[1];
			if ((i >= 0) && (i < XUSER_MAX_COUNT)) {
				if (packet[0] == PAD_ONLINE) {
					XINPUT_VIBRATION Vibration;
					
					uint8_t pindex = (uint8_t)packet[4];
					Vibration.wLeftMotorSpeed = 0;
					Vibration.wRightMotorSpeed = -1;
					XInputSetState(i, &Vibration);

					printf("Get Local controller %u as Online controller %u.\n", i,pindex);
					std::this_thread::sleep_for(std::chrono::milliseconds(250));

					Vibration.wLeftMotorSpeed = 0;
					Vibration.wRightMotorSpeed = 0;
					XInputSetState(i, &Vibration);

				}
				else if (packet[0] == PAD_VIBRA) {
					XINPUT_VIBRATION Vibration;
					Vibration.wLeftMotorSpeed = packet[2] << 8;
					Vibration.wRightMotorSpeed = packet[3] << 8;
					XInputSetState(i, &Vibration);
				}
			}
		}
	}
}


void SendResetControllers()
{
	uint8_t packet[] = { 0xFFu };
	if (sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&addr, sizeof(addr)) != sizeof(packet))
		printf("Failed to send reset message.\n");
}

void PollControllers() 
{
	XINPUT_STATE state;
	for (uint8_t i = 0u; i < XUSER_MAX_COUNT; i++)
	{
		memset(&state, 0, sizeof(XINPUT_STATE));
		if (XInputGetState(i, &state) != ERROR_SUCCESS)
			continue;

		if (memcmp(lastSentInputStates + i, &state, sizeof(XINPUT_STATE)) == 0)
			continue;

		uint8_t packet[sizeof(XINPUT_STATE) + 1];
		packet[0] = (uint8_t)i;
		memcpy(packet + 1, &state, sizeof(XINPUT_STATE));
		memcpy(lastSentInputStates + i, &state, sizeof(XINPUT_STATE));

		if (sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&addr, sizeof(addr)) != sizeof(packet))
			printf("Failed to send update message of controller %u.\n", i);
	}
}

int main(int argc, char* argv[])
{
	std::string ip;
	int port = 4313;
	
	if (argc > 1) {
		ip = argv[1];
	}

	if (argc > 2) {
		sscanf_s(argv[2], "%d", &port);
	}
	
	if (ip.empty())
	{
		while (true)
		{
			printf("Please enter the IP of the target computer.\n");

			std::cin >> ip;			
			if (inet_pton(AF_INET, ip.c_str(), &(addr.sin_addr)) == 1)
				break;

			std::cout << ip << " is not a valid ip.\n";
		}
	}

	if (inet_pton(AF_INET, ip.c_str(), &(addr.sin_addr)) != 1)
	{
		std::cout << ip << " is not a valid ip, please try again ! \n";
		return -1;
	}

	printf("Target is %s:%d.\n", ip.c_str(),port);

	CoInitialize(NULL);

	printf("Starting networking...\n");

	WSADATA wsaData;
	int wsaStartupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaStartupResult != 0) 
	{
		printf("WSAStartup failed with error code 0x%08X.\n", wsaStartupResult);
		return -2;
	}

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET)
	{
		printf("Failed to create UDP transmission socket.\n");
		return -3;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	printf("Sending reset...\n");
	SendResetControllers();
	printf("Done.\n");
	printf("Waiting for gamepad input, press ESC to exit...\n");
	
	std::thread message(CheckControllerMessage); // new thread for vibra notify

	memset(lastSentInputStates, 0, sizeof(lastSentInputStates));
	while (true)
	{
		if (GetKeyState(VK_ESCAPE) & 0x8000)
			break;
		PollControllers();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	printf("Wait For Closing...\n");
	SendResetControllers();
	closesocket(sock);
	WSACleanup();
	CoUninitialize();
	return 0;
}