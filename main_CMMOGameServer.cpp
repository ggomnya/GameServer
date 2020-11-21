#include <WinSock2.h>
#include <Windows.h>
#include "CMMOGameServer.h"
#include "textparser.h"
#include <conio.h>

WCHAR ServerIP[16];
USHORT ServerPort;
DWORD ServerThreadNum;
DWORD ServerIOCPNum;
DWORD ServerMaxSession;

int wmain() {
	CINIParse Parse;
	Parse.LoadFile(L"GameServer_Config.ini");
	Parse.GetValue(L"IP", ServerIP);
	Parse.GetValue(L"PORT", (DWORD*)&ServerPort);
	Parse.GetValue(L"THREAD_NUMBER", &ServerThreadNum);
	Parse.GetValue(L"IOCP_NUMBER", &ServerIOCPNum);
	Parse.GetValue(L"MAX_SESSION", &ServerMaxSession);

	CMMOGameServer* MMOServer = new CMMOGameServer;
	MMOServer->Start(INADDR_ANY, ServerPort, ServerThreadNum, ServerIOCPNum, ServerMaxSession, false);
	DWORD curTime = timeGetTime();
	while (1) {
		//1초마다 List 현황 출력
		if (timeGetTime() - curTime >= 1000) {
			wprintf(L"================GameServer====================\n");
			wprintf(L"[Session List Size: %d]\n", MMOServer->_Monitor_SessionAll);
			wprintf(L"[Auth Count: %lld]\n", MMOServer->_Monitor_SessionAuth);
			wprintf(L"[Game Count: %lld]\n", MMOServer->_Monitor_SessionGame);
			wprintf(L"[Accept Count: %d]\n", MMOServer->_Monitor_AcceptTotal);
			wprintf(L"[AcceptTPS: %d]\n", MMOServer->_Monitor_AcceptTPS);
			wprintf(L"[Disconnect Count: %d]\n", MMOServer->_Monitor_DisconnectTotal);
			wprintf(L"[Packet Alloc Count: %lld]\n", CPacket::GetAllocCount());
			wprintf(L"[Send TPS: %d]\n", MMOServer->_Monitor_SendPacketTPS);
			wprintf(L"[Recv TPS: %d]\n", MMOServer->_Monitor_RecvPacketTPS);
			wprintf(L"[Auth FPS: %lld]\n", MMOServer->_Monitor_AuthUpdate_FPS);
			wprintf(L"[Game FPS: %lld]\n", MMOServer->_Monitor_GameUpdate_FPS);
			wprintf(L"[SendThread FPS: %lld]\n", MMOServer->_Monitor_SendThread_FPS);
			wprintf(L"\n\n");
			//wprintf(L"[Update Block Count: %lld]\n", server->_BlockCount);

			//wprintf(L"[NewCount: %d] [DeleteCount: %d]\n\n", server._NewCount, server._DeleteCount);
			MMOServer->_Monitor_AcceptTPS = 0;
			MMOServer->_Monitor_SendPacketTPS = 0;
			MMOServer->_Monitor_RecvPacketTPS = 0;
			MMOServer->_Monitor_AuthUpdate_FPS = 0;
			MMOServer->_Monitor_GameUpdate_FPS = 0;
			MMOServer->_Monitor_SendThread_FPS = 0;
			curTime = timeGetTime();
		}
		if (_kbhit()) {
			WCHAR cmd = _getch();
			if (cmd == L'q' || cmd == L'Q')
				CCrashDump::Crash();
		}
		Sleep(10);
	}
}
