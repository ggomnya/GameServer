#include <WinSock2.h>
#include <Windows.h>
#include "CMMOGameServer.h"
#include "CMonitoringLanClient.h"
#include "textparser.h"
#include "CProcessUsage.h"
#include <conio.h>
#include <time.h>

WCHAR ServerIP[16];
USHORT ServerPort;
DWORD ServerThreadNum;
DWORD ServerIOCPNum;
DWORD ServerMaxSession;

WCHAR ClientIP[16];
USHORT ClientPort;
DWORD ClientThreadNum;
DWORD ClientIOCPNum;

WCHAR monitorClientIP[16];
USHORT monitorClientPort;
DWORD monitorClientThreadNum;
DWORD monitorClientIOCPNum;

int wmain() {
	timeBeginPeriod(1);
	CProcessUsage ProcessUsage;
	CINIParse Parse;
	Parse.LoadFile(L"GameServer_Config.ini");
	Parse.GetValue(L"IP", ServerIP);
	Parse.GetValue(L"PORT", (DWORD*)&ServerPort);
	Parse.GetValue(L"THREAD_NUMBER", &ServerThreadNum);
	Parse.GetValue(L"IOCP_NUMBER", &ServerIOCPNum);
	Parse.GetValue(L"MAX_SESSION", &ServerMaxSession);

	Parse.GetValue(L"IP", ClientIP);
	Parse.GetValue(L"PORT", (DWORD*)&ClientPort);
	Parse.GetValue(L"THREAD_NUMBER", &ClientThreadNum);
	Parse.GetValue(L"IOCP_NUMBER", &ClientIOCPNum);

	Parse.GetValue(L"IP", monitorClientIP);
	Parse.GetValue(L"PORT", (DWORD*)&monitorClientPort);
	Parse.GetValue(L"THREAD_NUMBER", &monitorClientThreadNum);
	Parse.GetValue(L"IOCP_NUMBER", &monitorClientIOCPNum);

	CMMOGameServer* MMOServer = new CMMOGameServer;
	CMonitoringLanClient* monitorClient = new CMonitoringLanClient;
	MMOServer->Start(INADDR_ANY, ServerPort, ServerThreadNum, ServerIOCPNum, ServerMaxSession, false);
	monitorClient->Start(monitorClientIP, monitorClientPort, monitorClientThreadNum, monitorClientIOCPNum);

	while (1) {
		//1초마다 List 현황 출력
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
		wprintf(L"================MonitorClient===============\n");
		wprintf(L"[Monitor Connnect Sucess Count: %d]\n", monitorClient->_ConnectSuccess);
		ProcessUsage.UpdateProcessTime();
		ProcessUsage.PrintProcessInfo();
		wprintf(L"\n\n");

		//MonitorServer에 데이터 전송
		DWORD TimeStamp = time(NULL);
		// GameServer 실행 여부 ON / OFF
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_SERVER_RUN, 1, TimeStamp);
		// GameServer CPU 사용률
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_SERVER_CPU, (int)ProcessUsage.ProcessTotal(), TimeStamp);
		// GameServer 메모리 사용 MByte
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_SERVER_MEM, NULL, TimeStamp);
		// 게임서버 세션 수 (컨넥션 수)
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_SESSION, MMOServer->_Monitor_SessionAll, TimeStamp);
		// 게임서버 AUTH MODE 플레이어 수
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER, MMOServer->_Monitor_SessionAuth, TimeStamp);
		// 게임서버 GAME MODE 플레이어 수
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_GAME_PLAYER, MMOServer->_Monitor_SessionGame, TimeStamp);
		// 게임서버 Accept 처리 초당 횟수
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_ACCEPT_TPS, MMOServer->_Monitor_AcceptTPS, TimeStamp);
		// 게임서버 패킷처리 초당 횟수
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS, MMOServer->_Monitor_RecvPacketTPS, TimeStamp);
		// 게임서버 패킷 보내기 초당 완료 횟수
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS, MMOServer->_Monitor_SendPacketTPS, TimeStamp);
		//// 게임서버 DB 저장 메시지 초당 처리 횟수
		//monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_DB_WRITE_TPS, MMOServer->_Monitor_AcceptTPS, TimeStamp);
		//// 게임서버 DB 저장 메시지 큐 개수 (남은 수)
		//monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_DB_WRITE_MSG, MMOServer->_Monitor_AcceptTPS, TimeStamp);
		// 게임서버 AUTH 스레드 초당 프레임 수 (루프 수)
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS, MMOServer->_Monitor_AuthUpdate_FPS, TimeStamp);
		// 게임서버 GAME 스레드 초당 프레임 수 (루프 수)
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS, MMOServer->_Monitor_GameUpdate_FPS, TimeStamp);
		// 게임서버 패킷풀 사용량
		monitorClient->TransferData(dfMONITOR_DATA_TYPE_GAME_PACKET_POOL, CPacket::GetAllocCount(), TimeStamp);


		MMOServer->_Monitor_AcceptTPS = 0;
		MMOServer->_Monitor_SendPacketTPS = 0;
		MMOServer->_Monitor_RecvPacketTPS = 0;
		MMOServer->_Monitor_AuthUpdate_FPS = 0;
		MMOServer->_Monitor_GameUpdate_FPS = 0;
		MMOServer->_Monitor_SendThread_FPS = 0;
		
		if (_kbhit()) {
			WCHAR cmd = _getch();
			if (cmd == L'q' || cmd == L'Q')
				CCrashDump::Crash();
		}
		Sleep(999);
	}
}
