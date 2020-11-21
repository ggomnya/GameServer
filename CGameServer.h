#pragma once

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")
#include "CGameSession.h"
#include <WinSock2.h>
#include <Windows.h>
#include <process.h>
#include <WS2tcpip.h>
#include <locale.h>



class CGameServer {

	CLockfreeStack<int> _IndexSession;
	CObjectPool<st_ACCEPT> _AcceptSocketPool;
	CLockfreeQueue<st_ACCEPT*> _AcceptSocketQ;
	INT64 _SessionIDCnt;
	SOCKET _Listen_sock;
	HANDLE _hcp;
	HANDLE* _hWorkerThread;
	HANDLE _hAcceptThread;
	HANDLE _hSendThread;
	HANDLE _hAuthThread;
	HANDLE _hGameThread;
	int _NumThread;
public:
	CGameSession** _SessionList;
	int _MaxSession;
	long		_Monitor_AcceptSocket; // ¾ê´Â _AcceptSocketQÀÇ size¸¦ Ã¼Å©ÇÏ¸é µÊ
	long		_Monitor_SessionAll;
	long		_Monitor_SessionAuth;
	long		_Monitor_SessionGame;

	long		_Monitor_SendThread_FPS;
	long		_Monitor_AuthUpdate_FPS;
	long		_Monitor_GameUpdate_FPS;
	long		_Monitor_AcceptTPS;
	long		_Monitor_RecvPacketTPS;
	long		_Monitor_SendPacketTPS;

	long		_Monitor_AcceptTotal;
	long		_Monitor_DisconnectTotal;

	CGameServer();

	static unsigned int WINAPI WorkerThreadFunc(LPVOID lParam) {
		((CGameServer*)lParam)->WorkerThread(lParam);
		return 0;
	}

	static unsigned int WINAPI AcceptThreadFunc(LPVOID lParam) {
		((CGameServer*)lParam)->AcceptThread(lParam);
		return 0;
	}

	static unsigned int WINAPI SendThreadFunc(LPVOID lParam) {
		((CGameServer*)lParam)->SendThread(lParam);
		return 0;
	}

	static unsigned int WINAPI AuthThreadFunc(LPVOID lParam) {
		((CGameServer*)lParam)->AuthThread(lParam);
		return 0;
	}

	static unsigned int WINAPI GameThreadFunc(LPVOID lParam) {
		((CGameServer*)lParam)->GameThread(lParam);
		return 0;
	}

	unsigned int WINAPI WorkerThread(LPVOID lParam);

	unsigned int WINAPI AcceptThread(LPVOID lParam);

	unsigned int WINAPI SendThread(LPVOID lParam);

	unsigned int WINAPI AuthThread(LPVOID lParam);

	unsigned int WINAPI GameThread(LPVOID lParam);

	bool Start(ULONG OpenIP, USHORT Port, int NumWorkerthread, int NumIOCP, int MaxSession,
		int iBlockNum = 100, bool bPlacementNew = false, bool Restart = false);

	void RecvPost(CGameSession* pSession);
	void SendPost(CGameSession* pSession);

	virtual void CreateSession() = 0;
	virtual void OnGame_Update() = 0;
};