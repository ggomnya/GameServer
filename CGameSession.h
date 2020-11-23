#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include "PacketBuffer.h"
#include "LockfreeQueue.h"
#include "RingBuffer_Lock.h"
#include "CommonStruct.h"
#include "CommonProtocol.h"


class CGameSession {
public:
	CGameSession();
	BYTE _Mode;
	SOCKET _sock;
	SOCKET _closeSock;
	INT64 _SessionID;
	INT64 _SessionIndex;
	SOCKADDR_IN _clientaddr;
	stOVERLAPPED _SendOverlapped;
	stOVERLAPPED _RecvOverlapped;
	CRingBuffer _RecvQ;
	CLockfreeQueue<CPacket*> _SendQ;
	CLockfreeQueue<CPacket*> _ComplateQ;
	volatile LONG _SendFlag;
	CPacket* _PacketArray[200];
	int _PacketCount;
	bool _bAuthToGameFlag;
	__declspec(align(64))
		LONG64 _IOCount;
	LONG64 _ReleaseFlag;


	bool SendPacket(CPacket* pSendPacket, int type = eNET);
	bool Disconnect();
	void ReleaseSession();
	int Release();

	virtual void OnAuth_ClientJoin() = 0;
	virtual void OnAuth_ClientLeave() = 0;
	virtual void OnAuth_Packet(CPacket* pPacket) = 0;
	virtual void OnGame_ClientJoin() = 0;
	virtual void OnGame_ClientLeave() = 0;
	virtual void OnGame_Packet(CPacket* pPacket) = 0;
	virtual void OnGame_ClientRelease() = 0;
};