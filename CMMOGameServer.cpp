#include "CMMOGameServer.h"
CMMOGameServer::CMMOGameServer() {

}


void CMMOGameServer::CreateSession() {
	_SessionList = new CGameSession * [_MaxSession];

	//PlayerSession �ۼ� �� ����
	_CPlayerList = new CPlayerSession[_MaxSession];

	//GameSession�� CPlayerSession ������ ����
	for (int i = 0; i < _MaxSession; i++) {
		_SessionList[i] = (CGameSession*)&_CPlayerList[i];
	}
}
void CMMOGameServer::OnGame_Update() {
	return;
}