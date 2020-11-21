#include "CMMOGameServer.h"
CMMOGameServer::CMMOGameServer() {

}


void CMMOGameServer::CreateSession() {
	_SessionList = new CGameSession * [_MaxSession];

	//PlayerSession 작성 후 수정
	_CPlayerList = new CPlayerSession[_MaxSession];

	//GameSession에 CPlayerSession 포인터 저장
	for (int i = 0; i < _MaxSession; i++) {
		_SessionList[i] = (CGameSession*)&_CPlayerList[i];
	}
}
void CMMOGameServer::OnGame_Update() {
	return;
}