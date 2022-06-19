#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <ViGEm/Client.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <objbase.h>

#include <iostream>
#include <stdint.h>

SOCKET sock;
PVIGEM_TARGET pads[XUSER_MAX_COUNT];
PVIGEM_CLIENT client;

void ResetGamepads()
{
	for (int i = 0; i < XUSER_MAX_COUNT; i++)
	{
		auto pad = pads[i];
		pads[i] = nullptr;

		if (pad == nullptr)
			continue;

		vigem_target_remove(client, pad);
		vigem_target_free(pad);
	}
}

int main()
{
	CoInitialize(NULL);

	printf("Starting networking...\n");

	WSADATA wsaData;
	int wsaStartupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaStartupResult != 0)
	{
		printf("WSAStartup failed with error code 0x%08X.\n", wsaStartupResult);
		return -1;
	}

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET)
	{
		printf("Failed to create UDP transmission socket.\n");
		return -2;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(4313);
	inet_pton(AF_INET, "0.0.0.0", &(addr.sin_addr));

	if (bind(sock, (const sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		printf("Failed to bind to 0.0.0.0:4313.");
		closesocket(sock);
		WSACleanup();
		return -3;
	}

	printf("Done.\n");

	printf("Setting up ViGEmClient...\n");
	client = vigem_alloc();
	if (client == nullptr)
	{
		printf("Failed to setup ViGEmClient.\n");
		closesocket(sock);
		WSACleanup();
		return -4;
	}

	printf("Connecting to ViGEm driver...\n");
	const auto connectResult = vigem_connect(client);
	if (!VIGEM_SUCCESS(connectResult))
	{
		printf("ViGEm Bus connection failed with error code 0x%08X.\n", connectResult);
		vigem_free(client);
		closesocket(sock);
		WSACleanup();
		return -5;
	}

	printf("Done.\n");

	printf("Waiting for data...\n");

	struct sockaddr_in clientAddr;
	uint8_t packet[sizeof(XINPUT_STATE) + 1];
	while (true)
	{
		if (GetKeyState(VK_ESCAPE) & 0x8000)
			break;

		memset(&clientAddr, 0, sizeof(clientAddr));
		int addrLen = sizeof(clientAddr);
		int bytesReceived = recvfrom(sock, (char*)&packet, sizeof(packet), 0, (struct sockaddr*)&clientAddr, &addrLen);

		if (bytesReceived == 1 && packet[0] == (uint8_t)0xFFu)
		{
			printf("Reset signal received, gamepads will be reset.\n");
			ResetGamepads();
		}
		else if (bytesReceived == sizeof(packet) && packet[0] >= 0 && packet[0] < XUSER_MAX_COUNT)
		{
			uint32_t i = (uint32_t)packet[0];
			XINPUT_STATE* state = (XINPUT_STATE*)(packet + 1);

			if (pads[i] == nullptr)
			{
				auto pad = vigem_target_x360_alloc();
				const auto targetAddResult = vigem_target_add(client, pad);
				if (VIGEM_SUCCESS(targetAddResult))
				{
					printf("Connected new gamepad %u.\n", i);
					pads[i] = pad;
				}
				else
				{
					vigem_target_free(pad);
					printf("Failed to add pad %u with error code 0x%08X.\n", i, targetAddResult);
				}
			}

			auto pad = pads[i];
			if (pad != nullptr)
				vigem_target_x360_update(client, pad, *reinterpret_cast<XUSB_REPORT*>(&state->Gamepad));
		}
	}

	ResetGamepads();
	vigem_disconnect(client);
	vigem_free(client);
	closesocket(sock);
	WSACleanup();
	CoUninitialize();
	return 0;
}