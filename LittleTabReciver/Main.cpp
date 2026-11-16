#define WIN32_LEAN_AND_MEAN

#ifndef UNICODE
#define UNICODE
#endif

#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#include <winsock2.h>
#include <windows.h>
#include <winuser.h>
#include <iostream>
#include <cstdint>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ws2_32.lib")

struct TabletPacket {
	int32_t x;
	int32_t y;
	int32_t pressure;
	int32_t touch;
};



HSYNTHETICPOINTERDEVICE g_penDevice = nullptr;

void SetupVirtualPen() {
	g_penDevice = CreateSyntheticPointerDevice(PT_PEN, 1, POINTER_FEEDBACK_DEFAULT);
	if (!g_penDevice) {
		// Fixed: was GetLastError (no parentheses), which printed the function's address
		std::cerr << "Failed to create synthetic pen. Error " << GetLastError() << std::endl;
	}
}

void InjectionPenFrame(int x, int y, int pressure, int isTouching, TabletPacket packet) {
	POINTER_TYPE_INFO penFrame = { 0 };
	penFrame.type = PT_PEN;

	static UINT32 frameId = 0;
	penFrame.penInfo.pointerInfo.frameId = frameId++;
	penFrame.penInfo.pointerInfo.dwTime = GetTickCount();

	penFrame.penInfo.pointerInfo.pointerType = PT_PEN;
	penFrame.penInfo.pointerInfo.pointerId = 0;
	penFrame.penInfo.pointerInfo.ptPixelLocation.x = x;
	penFrame.penInfo.pointerInfo.ptPixelLocation.y = y;
	penFrame.penInfo.pointerInfo.ptPixelLocationRaw.x = packet.x;
	penFrame.penInfo.pointerInfo.ptPixelLocationRaw.y = packet.y;
	penFrame.penInfo.penMask = PEN_MASK_PRESSURE;
	penFrame.penInfo.pressure = min(max(pressure, 0), 1024);

	penFrame.penInfo.penFlags = 0;
	static bool wasTouching = false;

	// Fixed: every frame now carries exactly one of DOWN / UPDATE / UP.
	//   pen touches down  -> INRANGE | INCONTACT | DOWN
	//   pen drawing       -> INRANGE | INCONTACT | UPDATE
	//   pen lifts         -> INRANGE | UP
	//   pen hovering      -> INRANGE | UPDATE
	POINTER_FLAGS flags = POINTER_FLAG_INRANGE;

	if (isTouching) {
		flags |= POINTER_FLAG_INCONTACT;
		flags |= wasTouching ? POINTER_FLAG_UPDATE : POINTER_FLAG_DOWN;
	}
	else {
		flags |= wasTouching ? POINTER_FLAG_UP : POINTER_FLAG_UPDATE;
	}

	penFrame.penInfo.pointerInfo.pointerFlags = flags;

	wasTouching = isTouching;

	// Fixed: check whether Windows accepted the frame
	if (!InjectSyntheticPointerInput(g_penDevice, &penFrame, 1)) {
		std::cerr << "\nInjection failed. Error " << GetLastError() << std::endl;
	}
}

int main() {
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(9000);

	if (bind(s, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		std::cerr << "Bind failed, port 9000 might be in use." << std::endl;
		return -1;
	}

	SetupVirtualPen();
	if (!g_penDevice) return -1;

	std::cout << "Listening for pen on port 9000" << std::endl;

	TabletPacket packet;
	int server_len = sizeof(server);
	while (true) {
		int res = recvfrom(s, (char*)&packet, sizeof(packet), 0, (struct sockaddr*)&server, &server_len);
		if (res > 0) {
			int winX = (int)packet.x * 1920 / 29376;
			int winY = (int)packet.y * 1080 / 16524;
			int winPressure = (int)packet.pressure;

			int winTouch = (int)packet.touch;

			std::cout << "X: " << winX
				<< " | Y: " << winY
				<< " | Pres: " << winPressure
				<< " | Touch: " << (winTouch ? "DOWN" : "UP")
				<< "      \r" << std::flush;

			InjectionPenFrame(winX, winY, winPressure, winTouch, packet);
		}
	}

	closesocket(s);
	WSACleanup();
	return 0;


}