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
	uint8_t                            hMac[6];
} TARGET_PADS, * PTARGET_PADS;

#define PAD_SERVER_SIZE 5
#define PAD_CLIENT_SIZE 24
#define MAC_SIZE 6


#define PAD_ONLINE 0x00u
#define PAD_VIBRA 0x01u
#define PAD_REG 0x03u
#define PAD_STATE 0x04u
#define PAD_RESET 0xFFu

SOCKET sock;
TARGET_PADS pads[XUSER_MAX_COUNT];
PVIGEM_CLIENT client;

bool checkvalidmac(uint8_t hMac[])
{
		for (int i = 0; i < MAC_SIZE; i++) {
			if (hMac[i] > uint8_t(0x00u) && hMac[i] < uint8_t(0xFFu))
				return true;
		}
	return false;
}

VOID CALLBACK gamepad_callback(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData) {
	for (int i = 0; i < XUSER_MAX_COUNT; i++)
	{
		if (Target == pads[i].Pad) {

			char packet[PAD_SERVER_SIZE];
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
				printf("Server controller %d unregistered. \n", pads[i].Pid);
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

	char packet[PAD_SERVER_SIZE];

	packet[0] = PAD_ONLINE;
	packet[1] = Pid;
	packet[2] = 0;
	packet[3] = 0;
	packet[4] = Pindex;

	if (sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&ClientAddr, sizeof(ClientAddr)) != sizeof(packet))
		printf("Failed to Send Online message \n");
}

bool CheckMac(uint8_t iMac[], uint8_t hMac[]) {

	for (int i = 0; i < MAC_SIZE; i++) {
		if (iMac[i] !=  hMac[i]) {
			return false;
		}
	}
	return true;
}




int GetNewPadIndex(uint8_t pid, SOCKADDR_IN ClientAddr,uint8_t hMac[])
{
	int index = -1;
	for (int i = 0; i < XUSER_MAX_COUNT; i++)
	{
		if (pads[i].Pad == nullptr && index < 0 )
		{
			index = i;

		}else if ( pads[i].Pid == pid  && CheckMac(pads[i].hMac,hMac))
		{
			if (pads[i].Addr.sin_port!= ClientAddr.sin_port || pads[i].Addr.sin_addr.S_un.S_addr != ClientAddr.sin_addr.S_un.S_addr)
		     	   pads[i].Addr = ClientAddr;
			index = i;
			break;
		}
	}
	return index;
}


int CheckPadIndex(uint8_t pid, SOCKADDR_IN ClientAddr, uint8_t hMac[])
{
	int index = -1;
	for (int i = 0; i < XUSER_MAX_COUNT; i++)
	{
		if (pads[i].Pad == nullptr)
			continue; 
		
		if (pads[i].Pid == pid && CheckMac(pads[i].hMac, hMac))
		{
			if (pads[i].Addr.sin_port != ClientAddr.sin_port || pads[i].Addr.sin_addr.S_un.S_addr != ClientAddr.sin_addr.S_un.S_addr)
				pads[i].Addr = ClientAddr;
			index = i;
			break;
		}
	}
	return index;
}

void ResetGamepad(SOCKADDR_IN ClientAddr, uint8_t hMac[])
{
	for (int i = 0; i < XUSER_MAX_COUNT; i++)
	{
		if (pads[i].Pad == nullptr)
			continue; 
		if (CheckMac(pads[i].hMac, hMac))
		{			
			printf("Server controller %d unregistered. \n", pads[i].Pid); 
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

	printf("Starting networking ...\n");

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

	printf("Waiting for data...\n");

	std::thread checktimeout(CheckPadUpdateTimeOut);

	uint8_t packet[PAD_CLIENT_SIZE];
	struct sockaddr_in clientAddr;

	while (true)
	{
		memset(&clientAddr, 0, sizeof(clientAddr));
		int addr_len = sizeof(clientAddr);	
		int bytesReceived = recvfrom(sock, (char*)&packet, sizeof(packet), 0, (struct sockaddr*)&clientAddr, &addr_len);

		if (bytesReceived >2 && packet[0] == (uint8_t)PAD_RESET)
		{
			printf("Reset signal received, controllers will be reset.\n");
			uint8_t hMac[MAC_SIZE];
			memcpy(hMac, packet + 1, MAC_SIZE);
			ResetGamepad(clientAddr, hMac);

		}else if (bytesReceived > 2 && packet[0] == (uint8_t)PAD_REG)
		{
			uint8_t hMac[MAC_SIZE];
			uint8_t Pid = (uint8_t)packet[1];
			memcpy(hMac, packet+2, MAC_SIZE);
			uint8_t pindex = GetNewPadIndex(Pid, clientAddr,hMac);

			if (pindex >= 0 && pindex < XUSER_MAX_COUNT && checkvalidmac(hMac))
			{
				if (pads[pindex].Pad == nullptr) {
					auto pad = vigem_target_x360_alloc();
					const auto targetAddResult = vigem_target_add(client, pad);
					if (VIGEM_SUCCESS(targetAddResult))
					{
						printf("Connected server controller %u from client controller %u.\n", pindex, pads[pindex].Pid);

						pads[pindex].Pad = pad;
						pads[pindex].Addr = clientAddr;
						pads[pindex].Pid = Pid;
						pads[pindex].Utime = time(NULL);
						memcpy(pads[pindex].hMac,&hMac, MAC_SIZE);

						SetGamepadOnline(clientAddr,Pid, pindex);

						const auto retval = vigem_target_x360_register_notification(client, pad, &gamepad_callback, nullptr);

						if (VIGEM_SUCCESS(retval))
						{
							printf("Server controller %d register for notification Success! \n", pads[pindex].Pid);

						}
						else {
							printf("Server controller %d register for notification failed! \n", pads[pindex].Pid);
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
		else   if (bytesReceived > 2 && packet[0] == (uint8_t)PAD_STATE && packet[1] >= 0 && packet[1] < XUSER_MAX_COUNT)
		{
			uint8_t hMac[MAC_SIZE];
			uint8_t Pid = (uint8_t)packet[1];
			memcpy(hMac, packet + 2, MAC_SIZE);
			uint8_t pindex = CheckPadIndex(Pid, clientAddr, hMac);
			XINPUT_STATE* state = (XINPUT_STATE*)(packet + MAC_SIZE + 2);
			if (pindex >= 0 && pindex < XUSER_MAX_COUNT && pads[pindex].Pad != nullptr && checkvalidmac(hMac))
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