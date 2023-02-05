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
#include <nb30.h>
#pragma comment(lib, "Netapi32.lib")

typedef struct _ASTAT_
{
	ADAPTER_STATUS adapt;
	NAME_BUFFER NameBuff[30];
} ASTAT, * PASTAT;


#define PAD_SERVER_SIZE 5
#define PAD_CLIENT_SIZE 24
#define MAC_SIZE 6

#define PAD_ONLINE 0x00u
#define PAD_VIBRA 0x01u
#define PAD_REG 0x03u
#define PAD_STATE 0x04u
#define PAD_RESET 0xFFu

SOCKET sock;
struct sockaddr_in addr;
XINPUT_STATE lastSentInputStates[XUSER_MAX_COUNT];
time_t updatetime[XUSER_MAX_COUNT];
uint8_t hmac[MAC_SIZE];

bool checkvalidmac(uint8_t hMac[])
{
	for (int i = 0; i < MAC_SIZE; i++) {
			if (hMac[i] > 0x00u && hMac[i] < uint8_t(0xFFu))
				return true;
		}	
	return false;
}


bool getMac()
{
	ASTAT Adapter;
	NCB Ncb;
	UCHAR uRetCode;
	LANA_ENUM lenum;
	int i = 0;

	memset(&Ncb, 0, sizeof(Ncb));
	Ncb.ncb_command = NCBENUM;
	Ncb.ncb_buffer = (UCHAR*)&lenum;
	Ncb.ncb_length = sizeof(lenum);

	uRetCode = Netbios(&Ncb);

	for (i = 0; i < lenum.length; i++)
	{
		memset(&Ncb, 0, sizeof(Ncb));
		Ncb.ncb_command = NCBRESET;
		Ncb.ncb_lana_num = lenum.lana[i];
		uRetCode = Netbios(&Ncb);

		memset(&Ncb, 0, sizeof(Ncb));
		Ncb.ncb_command = NCBASTAT;
		Ncb.ncb_lana_num = lenum.lana[i];
		strcpy_s((char*)Ncb.ncb_callname,  sizeof(Ncb.ncb_callname), "*");
		Ncb.ncb_buffer = (unsigned char*)&Adapter;
		Ncb.ncb_length = sizeof(Adapter);
		uRetCode = Netbios(&Ncb);

		if (uRetCode == 0)
		{
			for (int j = 0; j < MAC_SIZE; j++) {
				hmac[j] = int(Adapter.adapt.adapter_address[j]);
			}		
			
			if (checkvalidmac(hmac)) 
				return true;
		}
	}
	return false;
}


void CheckControllerMessage()
{ 	
	while (true)
	{		
		char packet[PAD_SERVER_SIZE];
		int addr_size = sizeof(addr);
		int bytesReceived = recvfrom(sock, (char*)&packet, sizeof(packet), 0, (struct sockaddr*)&addr, &addr_size);	

		if (bytesReceived == PAD_SERVER_SIZE){
			uint8_t i = (uint8_t)packet[1];
			if ((i >= 0) && (i < XUSER_MAX_COUNT)) {
				if (packet[0] == (uint8_t)PAD_ONLINE) {
					XINPUT_VIBRATION Vibration;
					
					uint8_t pindex = (uint8_t)packet[4];
					Vibration.wLeftMotorSpeed = 0;
					Vibration.wRightMotorSpeed = -1;
					XInputSetState(i, &Vibration);

					printf("Connected client controller %u to server controller %u.\n", i,pindex);
					std::this_thread::sleep_for(std::chrono::milliseconds(250));

					Vibration.wLeftMotorSpeed = 0;
					Vibration.wRightMotorSpeed = 0;
					XInputSetState(i, &Vibration);

				}
				else if (packet[0] == (uint8_t)PAD_VIBRA) {
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
	uint8_t packet[1+ MAC_SIZE];
	packet[0] = (uint8_t)PAD_RESET;
	memcpy(packet + 1, &hmac, MAC_SIZE);
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

		if (time(NULL) - updatetime[i] >= 5)
		{
			uint8_t packet[MAC_SIZE+2];
			packet[0] = (uint8_t)PAD_REG;
			packet[1] = (uint8_t)i;
			memcpy(packet + 2, &hmac, MAC_SIZE);
			if (sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&addr, sizeof(addr)) != sizeof(packet))
				printf("Failed to checktimeOut of client controller %u .\n",i);
			updatetime[i] = time(NULL);
		}

		if (memcmp(lastSentInputStates + i, &state, sizeof(XINPUT_STATE)) == 0)
		  continue;		

		uint8_t packet[PAD_CLIENT_SIZE];
		
		packet[0] = (uint8_t)PAD_STATE;
		packet[1] = (uint8_t)i;

		memcpy(packet + 2, &hmac, MAC_SIZE);
		memcpy(packet + MAC_SIZE + 2, &state, sizeof(XINPUT_STATE));
		memcpy(lastSentInputStates + i, &state, sizeof(XINPUT_STATE));

		if (sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&addr, sizeof(addr)) != sizeof(packet))
			printf("Failed to send update message of client controller %u.\n", i);
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

	if (!getMac()) {
		printf("Failed to get mac address .\n");
		return -1;
	}

	printf("Connected to server %s:%d.\n", ip.c_str(),port);

	CoInitialize(NULL);
	
	printf("Starting networking at %02X:%02X:%02X:%02X:%02X:%02X ...\n",
		  hmac[0], hmac[1], hmac[2], hmac[3], hmac[4], hmac[5]);
	
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

	printf("Waiting for gamepad input, press Ctrl+C to exit...\n");

	std::thread message(CheckControllerMessage); // new thread for message

	memset(lastSentInputStates, 0, sizeof(lastSentInputStates));
	while (true)
	{
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