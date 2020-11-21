#pragma once
#include "CGameSession.h"

class CPlayerSession : public CGameSession {
private:
	//牧刨明 包访 单捞磐 技泼
	INT64 _AccountNo;
	char _SessionKey[64];
	LONGLONG _SendTick;
public :
	CPlayerSession();

	void MPGameResLogin(CPacket* pPacket, WORD Type, BYTE Status, INT64 AccountNo);
	void MPGameResEcho(CPacket* pPacket, WORD Type, INT64 AccountNo, LONGLONG SendTick);

	virtual void OnAuth_ClientJoin();
	virtual void OnAuth_ClientLeave();
	virtual void OnAuth_Packet(CPacket* pPacket);;
	virtual void OnGame_ClientJoin();
	virtual void OnGame_ClientLeave();
	virtual void OnGame_Packet(CPacket* pPacket);
	virtual void OnGame_ClientRelease();
};