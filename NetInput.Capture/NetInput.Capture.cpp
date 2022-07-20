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

SOCKET sock;
struct sockaddr_in addr;
XINPUT_STATE lastSentInputStates[XUSER_MAX_COUNT];

void SendResetControllers()
{
	uint8_t packet[] = { 0xFFu };
	if (sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&addr, sizeof(addr)) != sizeof(packet))
		printf("Failed to send reset message.\n");
}

void PollControllers()
{
	XINPUT_STATE state;
	for (uint32_t i = 0u; i < XUSER_MAX_COUNT; i++)
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

int main()
{
	std::string ip;
	std::int16_t port = 4313;

	std::ifstream input_file("target.txt");
	if (input_file.is_open())
	{
		printf("Reading ip from target.txt.\n");
		std::string line;

		int index = 0;
		while (!input_file.eof()) {
			getline(input_file, line);

			switch (index)
			{
			case 0:
				if (inet_pton(AF_INET, line.c_str(), &(addr.sin_addr)) != 1)
				{
					std::cout << line << " is not a valid ip, please correct target.txt\n";
					return -1;
				}

				ip = line;
				break;
			case 1:
				try
				{
					port = std::stoi(line);
					if (port <= 0)
						port = 4313;
				}
				catch (...)
				{
					std::cout << line << " is not a valid Port number, using default value(4313) correct target.txt if your intent was to use another Port number\n";
					port = 4313;
				}
				break;
			default:
				break;
			}

			index++;
		}
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

	printf("Target is %s.\n", ip.c_str());

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

	printf("Port is %d.\n", port);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	printf("Sending reset...\n");
	SendResetControllers();
	printf("Done.\n");

	printf("Waiting for gamepad input, press ESC to exit...\n");
	memset(lastSentInputStates, 0, sizeof(lastSentInputStates));
	while (true)
	{
		if (GetKeyState(VK_ESCAPE) & 0x8000)
			break;

		PollControllers();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	SendResetControllers();
	closesocket(sock);
	WSACleanup();
	CoUninitialize();
	return 0;
}