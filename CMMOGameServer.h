#pragma once
#include "CGameServer.h"
#include "CPlayerSession.h"

class CMMOGameServer : public CGameServer{
	CPlayerSession* _CPlayerList;
public:
	CMMOGameServer();

	virtual void CreateSession();
	virtual void OnGame_Update();


};