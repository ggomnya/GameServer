#include "CGameSession.h"

CGameSession::CGameSession() {
	_Mode = MODE_NONE;
	_sock = NULL;
	_closeSock = NULL;
	_SessionID = -1;
	_SessionIndex = -1;
}

bool CGameSession::SendPacket(CPacket* pSendPacket, int type) {
	pSendPacket->AddRef();
	if (type == eNET){
		pSendPacket->SetHeader_5();
		pSendPacket->Encode();
	}
	else if (type == eLAN) {
		pSendPacket->SetHeader_2();
	}
	_SendQ.Enqueue(pSendPacket);
	return true;
}
bool CGameSession::Disconnect() {
	int retval = InterlockedExchange(&_sock, INVALID_SOCKET);
	if (retval != INVALID_SOCKET) {
		_closeSock = retval;
		CancelIoEx((HANDLE)_closeSock, NULL);
		return true;
	}
	return false;
}
void CGameSession::ReleaseSession() {
	int retval = InterlockedDecrement64(&_IOCount);
	if (retval == 0) {
		if (_ReleaseFlag == TRUE) {
			stRELEASE temp;
			InterlockedCompareExchange128(&_IOCount, (LONG64)FALSE, (LONG64)0, (LONG64*)&temp);
		}
	}
	else if (retval < 0) {
		CCrashDump::Crash();
	}
}

int CGameSession::Release() {
	_SessionID = -1;
	while (_SendQ.Size() > 0 || _PacketCount > 0) {
		while (_SendQ.Size() > 0) {
			if (_PacketCount >= dfPACKETNUM)
				break;
			_SendQ.Dequeue(&(_PacketArray[_PacketCount]));
			if (_PacketArray[_PacketCount] != NULL)
				_PacketCount++;
		}
		for (int j = 0; j < _PacketCount; j++) {
			_PacketArray[j]->Free();
		}
		_PacketCount = 0;
	}
	while (_ComplateQ.Size() > 0) {
		CPacket* delPacket;
		_ComplateQ.Dequeue(&delPacket);
		delPacket->Free();
	}
	LINGER optval;
	optval.l_linger = 0;
	optval.l_onoff = 1;
	setsockopt(_closeSock, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
	closesocket(_closeSock);
	_Mode = MODE_NONE;
	return _SessionIndex;
}