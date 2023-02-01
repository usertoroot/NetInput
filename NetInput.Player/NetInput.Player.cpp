#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <ViGEm/Client.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <objbase.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <stdint.h>

typedef struct _TARGET_PADS
{
	uint8_t                            Pid;
	PVIGEM_TARGET                      Pad;
	SOCKADDR_IN                        Addr;
	time_t                             Utime;
} TARGET_PADS, * PTARGET_PADS;

#define PAD_OUT_SIZE 5
#define PAD_ONLINE 0x00
#define PAD_VIBRA 0x01

SOCKET sock;
TARGET_PADS pads[XUSER_MAX_COUNT];
PVIGEM_CLIENT client;

VOID CALLBACK gamepad_callback(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData) {
	for (int i = 0; i < XUSER_MAX_COUNT; i++)
	{
		if (Target == pads[i].Pad){				

			char packet[PAD_OUT_SIZE];
			packet[0] = PAD_VIBRA; 
			packet[1] = pads[i].Pid;
			
			packet[2] = LargeMotor;
			packet[3] = SmallMotor;
			packet[4] = LedNumber;
     		if (sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&pads[i].Addr, sizeof(pads[i].Addr)) != sizeof(packet))
			     printf("Failed to Send Notify message \n");
			break;

		}
	}
}

void CheckPadUpdateTimeOut() {

   while (true)
   {
		for (int i = 0; i < XUSER_MAX_COUNT; i++)
		{
			if (pads[i].Pad == nullptr)
				continue;

			if (time(NULL) - pads[i].Utime >= 15)
			{
				auto pad = pads[i].Pad;
				pads[i].Pad = nullptr;
				vigem_target_remove(client, pad);
				vigem_target_x360_unregister_notification(pad);
				vigem_target_free(pad);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
   }
}

void SetGamepadOnline(SOCKADDR_IN ClientAddr, uint8_t Pid, uint8_t Pindex) {

	char packet[PAD_OUT_SIZE];

	packet[0] = PAD_ONLINE;
	packet[1] = Pid;
	packet[2] = 0;
	packet[3] = 0;
	packet[4] = Pindex;

	if (sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&ClientAddr, sizeof(ClientAddr)) != sizeof(packet))
		printf("Failed to Send Online message \n");
}


int GetNewPadIndex(uint8_t pid, SOCKADDR_IN ClientAddr)
{
	int index = -1;
	for (int i = 0; i < XUSER_MAX_COUNT; i++)
	{
		if (pads[i].Pad == nullptr && index < 0 )
		{
			index = i;

		}else if ( pads[i].Pid == pid  && pads[i].Addr.sin_addr.S_un.S_addr == ClientAddr.sin_addr.S_un.S_addr && pads[i].Addr.sin_port == ClientAddr.sin_port)
		{
			index = i;
			break;
		}
	}
	return index;
}


int CheckPadIndex(uint8_t pid, SOCKADDR_IN ClientAddr)
{
	int index = -1;
	for (int i = 0; i < XUSER_MAX_COUNT; i++)
	{
		if (pads[i].Pad == nullptr)
			continue; 
		
		if (pads[i].Pid == pid && pads[i].Addr.sin_addr.S_un.S_addr == ClientAddr.sin_addr.S_un.S_addr && pads[i].Addr.sin_port == ClientAddr.sin_port)
		{
			index = i;
			break;
		}
	}
	return index;
}

void ResetGamepad(SOCKADDR_IN ClientAddr)
{
	for (int i = 0; i < XUSER_MAX_COUNT; i++)
	{
		if (pads[i].Pad == nullptr)
			continue; 

		if (pads[i].Addr.sin_addr.S_un.S_addr == ClientAddr.sin_addr.S_un.S_addr && pads[i].Addr.sin_port== ClientAddr.sin_port)
		{			
		    auto pad = pads[i].Pad;
			pads[i].Pad = nullptr;
			vigem_target_remove(client, pad);
			vigem_target_x360_unregister_notification(pad);
			vigem_target_free(pad);
		}	
	}
}

void ResetGamepads()
{
	for (int i = 0; i < XUSER_MAX_COUNT; i++)
	{
		if (pads[i].Pad == nullptr)
			continue;
			auto pad = pads[i].Pad;
			pads[i].Pad = nullptr;
			vigem_target_remove(client, pad);
			vigem_target_x360_unregister_notification(pad);
			vigem_target_free(pad);
	}
}


int main(int argc, char* argv[])
{
	int port = 4313;

	if (argc > 1) {
		sscanf_s(argv[1], "%d", &port);
	}
	
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
	addr.sin_port = htons(port);
	inet_pton(AF_INET, "0.0.0.0", &(addr.sin_addr));

	if (bind(sock, (const sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		printf("Failed to bind to 0.0.0.0:%d.", port);
		closesocket(sock);
		WSACleanup();
		return -3;
	}

	printf("Bind to 0.0.0.0:%d .\n",port);

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


	std::thread checktimeout(CheckPadUpdateTimeOut);

	uint8_t packet[sizeof(XINPUT_STATE) + 1];
	struct sockaddr_in clientAddr;

	while (true)
	{
		memset(&clientAddr, 0, sizeof(clientAddr));
		int addr_len = sizeof(clientAddr);	
		int bytesReceived = recvfrom(sock, (char*)&packet, sizeof(packet), 0, (struct sockaddr*)&clientAddr, &addr_len);

		if (bytesReceived == 1 && packet[0] == (uint8_t)0xFFu)
		{
			printf("Reset signal received, controllers will be reset.\n");
			ResetGamepad(clientAddr);
		}else if (bytesReceived == 2 && packet[0] == (uint8_t)0xFEu)
		{
			uint8_t pindex = GetNewPadIndex((uint8_t)packet[1], clientAddr);
			if (pindex >= 0 && pindex < XUSER_MAX_COUNT)
			{
				if (pads[pindex].Pad == nullptr) {
					auto pad = vigem_target_x360_alloc();
					const auto targetAddResult = vigem_target_add(client, pad);
					if (VIGEM_SUCCESS(targetAddResult))
					{
						printf("Connected new controller %u.\n", pindex);

						pads[pindex].Pad = pad;
						pads[pindex].Addr = clientAddr;
						pads[pindex].Pid = (uint8_t)packet[1];
						pads[pindex].Utime = time(NULL);

						SetGamepadOnline(clientAddr, (uint8_t)packet[1], pindex);

						const auto retval = vigem_target_x360_register_notification(client, pad, &gamepad_callback, nullptr);

						if (VIGEM_SUCCESS(retval))
						{
							printf("Register for notification Success! \n");

						}
						else {
							printf("Register for notification failed! \n");
							vigem_target_x360_unregister_notification(pad);
						}

					}
					else
					{
						vigem_target_free(pad);
						printf("Failed to add pad %u with error code 0x%08X.\n", pindex, targetAddResult);
					}
				}

				if (pads[pindex].Pad != nullptr)
					pads[pindex].Utime = time(NULL);
			}
		}
		else if (bytesReceived == sizeof(packet) && packet[0] >= 0 && packet[0] < XUSER_MAX_COUNT)
		{
			uint8_t pindex = CheckPadIndex((uint8_t)packet[0], clientAddr);
			XINPUT_STATE* state = (XINPUT_STATE*)(packet + 1);
			if (pindex >= 0 && pindex < XUSER_MAX_COUNT && pads[pindex].Pad != nullptr)
			{			
					pads[pindex].Utime = time(NULL);
					vigem_target_x360_update(client, pads[pindex].Pad, *reinterpret_cast<XUSB_REPORT*>(&state->Gamepad));					
			}
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