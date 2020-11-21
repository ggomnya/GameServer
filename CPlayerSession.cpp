#include "CPlayerSession.h"

CPlayerSession::CPlayerSession() {
	_AccountNo = -1;
	_SendTick = 0;
}

void CPlayerSession::MPGameResLogin(CPacket* pPacket, WORD Type, BYTE Status, INT64 AccountNo) {
	*pPacket << Type << Status << AccountNo;
}
void CPlayerSession::MPGameResEcho(CPacket* pPacket, WORD Type, INT64 AccountNo, LONGLONG SendTick) {
	*pPacket << Type << AccountNo << SendTick;
}
void CPlayerSession::OnAuth_ClientJoin() {
	return;
}
void CPlayerSession::OnAuth_ClientLeave() {
	return;
}
void CPlayerSession::OnAuth_Packet(CPacket* pPacket) {
	WORD Type;
	*pPacket >> Type;
	if (Type != en_PACKET_CS_GAME_REQ_LOGIN) {
		//�� ���� �־ �ȵ�
		CCrashDump::Crash();
	}
	*pPacket >> _AccountNo;
	pPacket->GetData(_SessionKey, 64);
	//�α��� ���� ��Ŷ ������ and ��� ��ȯ ��û�ϱ�
	CPacket* pSendPacket = CPacket::Alloc();
	MPGameResLogin(pSendPacket, en_PACKET_CS_GAME_RES_LOGIN, 1, _AccountNo);
	SendPacket(pSendPacket);
	pSendPacket->Free();
	_bAuthToGameFlag = true;

}
void CPlayerSession::OnGame_ClientJoin() {
	return;
}
void CPlayerSession::OnGame_ClientLeave() {
	return;
}
void CPlayerSession::OnGame_Packet(CPacket* pPacket) {
	WORD Type;
	INT64 AccountNo;
	*pPacket >> Type;
	if (Type != en_PACKET_CS_GAME_REQ_ECHO) {
		CCrashDump::Crash();
	}
	*pPacket >> AccountNo;
	if (_AccountNo != AccountNo) {
		CCrashDump::Crash();
	}
	*pPacket >> _SendTick;
	CPacket* pSendPacket = CPacket::Alloc();
	*pSendPacket << (WORD)en_PACKET_CS_GAME_RES_ECHO << AccountNo << _SendTick;
	//MPGameResEcho(pSendPacket, en_PACKET_CS_GAME_RES_ECHO, AccountNo, _SendTick);
	SendPacket(pSendPacket);
	pSendPacket->Free();
}
void CPlayerSession::OnGame_ClientRelease() {
	return;
}