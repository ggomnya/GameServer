
#include "CGameServer.h"


CGameServer::CGameServer() {
	_Monitor_AcceptSocket = 0;
	_Monitor_SessionAll = 0;
	_Monitor_SessionAuth = 0;
	_Monitor_SessionGame = 0;

	_Monitor_AuthUpdate_FPS = 0;
	_Monitor_GameUpdate_FPS = 0;
	_Monitor_AcceptTPS = 0;
	_Monitor_RecvPacketTPS = 0;
	_Monitor_SendPacketTPS = 0;

	_Monitor_AcceptTotal = 0;
	_Monitor_DisconnectTotal = 0;
}

unsigned int WINAPI CGameServer::WorkerThread(LPVOID lParam) {
	int retval;
	DWORD cbTransferred=0;
	CGameSession* pSession;
	stOVERLAPPED* Overlapped;
	CPacket::stPACKET_HEADER stPacketHeader;
	int iSessionUseSize = 0;
	int iRecvTPS = 0;
	WORD wPacketCount;
	while (1) {
		retval = GetQueuedCompletionStatus(_hcp, &cbTransferred, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&Overlapped, INFINITE);
		//if (retval == false) {
		//	DWORD dwError = GetLastError();
		//	/*if (dwError != 64)
		//		Log(L"GQCS_CHAT", LEVEL_WARNING, (WCHAR*)L"SessionID: %lld, Overlapped Type: %d, Overlapped Error:%lld, WSAGetLastError: %d\n",
		//			pSession->SessionID, Overlapped->Type, Overlapped->Overlapped.Internal, dwError);*/
		//}
		//���� ó��
		if (Overlapped == NULL) {
			return 0;
		}
		/*if (&pSession->_RecvOverlapped != Overlapped && &pSession->_SendOverlapped != Overlapped) {
			CCrashDump::Crash();
		}*/
		//���� ����
		if (cbTransferred == 0) {
			pSession->Disconnect();
		}
		//Recv, Send ����
		else {
			//recv�� ���
			if (Overlapped->Type == RECV) {
				iRecvTPS = 0;
				//������ �޾Ƽ� SendQ�� ���� �� Send�ϱ�
				pSession->_RecvQ.MoveRear(cbTransferred);
				while (1) {
					iSessionUseSize = pSession->_RecvQ.GetUseSize();
					//��� ������ �̻��� �ִ��� Ȯ��
					if (iSessionUseSize < NETHEADER)
						break;
					//�����Ͱ� �ִ��� Ȯ��
					pSession->_RecvQ.Peek((char*)&stPacketHeader, NETHEADER);
					//��� �ڵ� ����
					if (stPacketHeader.byCode != dfPACKET_CODE) {
						pSession->Disconnect();
						break;
					}
					if (stPacketHeader.shLen > dfMAX_PACKET_BUFFER_SIZE - NETHEADER) {
						pSession->Disconnect();
						break;
					}
					if (iSessionUseSize < int(NETHEADER + stPacketHeader.shLen))
						break;
					CPacket* RecvPacket = CPacket::Alloc();
					retval = pSession->_RecvQ.Dequeue(RecvPacket->_Buffer, stPacketHeader.shLen + 5);
					RecvPacket->MoveWritePos(stPacketHeader.shLen);
					if (retval < stPacketHeader.shLen + NETHEADER) {
						pSession->Disconnect();
						RecvPacket->Free();
						break;
					}
					//���⼭ OnRecvȣ�� ���� Decode�� ���� ������ ���� ���� üũ
					RecvPacket->Decode();
					if (RecvPacket->_CheckSum != (BYTE) * (RecvPacket->GetHeaderPtr() + 4)) {
						pSession->Disconnect();
						RecvPacket->Free();
						break;
					}
					//Decode�� Packet�� ComplateQ�� �ֱ�
					pSession->_ComplateQ.Enqueue(RecvPacket);
					iRecvTPS++;
					//InterlockedIncrement(&_Monitor_RecvPacketTPS);
				}
				InterlockedAdd(&_Monitor_RecvPacketTPS, iRecvTPS);
				//Recv ��û�ϱ�
				RecvPost(pSession);

			}
			//send �Ϸ� ���
			else if (Overlapped->Type == SEND) {
				wPacketCount = pSession->_PacketCount;
				for (int i = 0; i < wPacketCount; i++) {
					pSession->_PacketArray[i]->Free();
				}
				pSession->_PacketCount = 0;
				InterlockedExchange(&(pSession->_SendFlag), TRUE);
			}
		}
		pSession->ReleaseSession();
	}
}

unsigned int WINAPI CGameServer::AcceptThread(LPVOID lParam) {
	while (1) {
		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		memset(&clientaddr, 0, sizeof(clientaddr));

		SOCKET client_sock = accept(_Listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			wprintf(L"accept() %d\n", WSAGetLastError());
			return 0;
		}
		_Monitor_AcceptTotal++;
		_Monitor_AcceptTPS++;
		//�� ������ ���� ��� ���� ����
		if (_IndexSession.Size() == 0) {
			wprintf(L"accept()_MaxSession\n");
			LINGER optval;
			optval.l_linger = 0;
			optval.l_onoff = 1;
			setsockopt(client_sock, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
			closesocket(client_sock);
			continue;
		}
		st_ACCEPT* pAcceptSocket = _AcceptSocketPool.Alloc();
		pAcceptSocket->sock = client_sock;
		pAcceptSocket->clientaddr = clientaddr;
		_AcceptSocketQ.Enqueue(pAcceptSocket);
	}
}

unsigned int WINAPI CGameServer::AuthThread(LPVOID lParam) {
	//AcceptQueue Ȯ�� �� ���� ����
	CGameSession* pSession;
	while (1) {
		//�� �����Ӵ� AcceptȽ���� ���س�����??
		while (_AcceptSocketQ.Size() != 0) {
			st_ACCEPT* pAcceptSocket;
			_AcceptSocketQ.Dequeue(&pAcceptSocket);
			int curIdx;
			_IndexSession.Pop(&curIdx);
			//�־ �ȵ�
			pSession = _SessionList[curIdx];
			if (pSession->_Mode != MODE_NONE)
				CCrashDump::Crash();
			pSession->_sock = pAcceptSocket->sock;
			pSession->_clientaddr = pAcceptSocket->clientaddr;
			pSession->_SessionID = ++_SessionIDCnt;
			pSession->_SessionIndex = curIdx;
			memset(&pSession->_RecvOverlapped, 0, sizeof(pSession->_RecvOverlapped));
			memset(&pSession->_SendOverlapped, 0, sizeof(pSession->_SendOverlapped));
			pSession->_RecvOverlapped.Type = RECV;
			pSession->_SendOverlapped.Type = SEND;
			pSession->_RecvOverlapped.SessionID = pSession->_SessionID;
			pSession->_SendOverlapped.SessionID = pSession->_SessionID;
			pSession->_IOCount = 1;
			pSession->_RecvQ.ClearBuffer();
			pSession->_SendQ.Clear();
			pSession->_ComplateQ.Clear();
			pSession->_ReleaseFlag = TRUE;
			pSession->_SendFlag = TRUE;
			pSession->_PacketCount = 0;
			pSession->_bAuthToGameFlag = false;
			InterlockedIncrement(&_Monitor_SessionAll);
			CreateIoCompletionPort((HANDLE)pSession->_sock, _hcp, (ULONG_PTR)pSession, 0);
			pSession->_Mode = MODE_AUTH;
			_Monitor_SessionAuth++;
			pSession->OnAuth_ClientJoin();
			RecvPost(pSession);
			_AcceptSocketPool.Free(pAcceptSocket);
			pSession->ReleaseSession();
		}

		//MODE_AUTH�� ���� Session ���� ó��
		for (int i = 0; i < _MaxSession; i++) {
			pSession = _SessionList[i];
			if (pSession->_Mode == MODE_AUTH) {
				//���⼭ Packet ó���Ҷ� GAME_PACKET, AUTH_PACKET �����ϱ�...
				while (pSession->_ComplateQ.Size() != 0) {
					//����� ��ȯ�� �� �� Recv�� �޾� packet�� ������ ��츦 ���� ����
					if (pSession->_bAuthToGameFlag)
						break;
					CPacket* pRecvPacket;
					pSession->_ComplateQ.Dequeue(&pRecvPacket);
					//OnAuth_Packet���� Auth_To_Game��� ��ȯ���� �ؾ� �ұ�?
					pSession->OnAuth_Packet(pRecvPacket);
					pRecvPacket->Free();
				}
			}
		}

		for (int i = 0; i < _MaxSession; i++) {
			pSession = _SessionList[i];
			if (pSession->_Mode == MODE_AUTH) {
				//release�� ��� üũ
				if (pSession->_ReleaseFlag == false) {
					pSession->OnAuth_ClientLeave();
					pSession->_Mode = MODE_WAIT_RELEASE;
					_Monitor_SessionAuth--;
				}
				//Game���� ��ȯ �ʿ��� �� üũ
				else if (pSession->_bAuthToGameFlag == true) {
					pSession->_Mode = MODE_AUTH_TO_GAME;
					_Monitor_SessionAuth--;
				}
			}
		}
		
		//���߿� �������� �ٲٱ�
		_Monitor_AuthUpdate_FPS++;
		Sleep(30);
	}
}

unsigned int WINAPI CGameServer::SendThread(LPVOID lParam) {
	CGameSession* pSession;
	int MaxSession = _MaxSession;
	while (1) {
		for (int i = 0; i < MaxSession; i++) {
			pSession = _SessionList[i];
			//�ش� ����� ��츸 �޼��� ������
			if (pSession->_Mode == MODE_AUTH || pSession->_Mode == MODE_GAME) {
				if(pSession->_SendQ.Size()>0)
					SendPost(pSession);
			}
		}
		//���߿� �����ϱ�
		_Monitor_SendThread_FPS++;
		Sleep(1);
	}
}

unsigned int WINAPI CGameServer::GameThread(LPVOID lParam) {
	while (1) {
		CGameSession* pSession;
		CPacket* pRecvPacket;
		int MaxSession = _MaxSession;
		for (int i = 0; i < MaxSession; i++) {
			pSession = _SessionList[i];
			//auth���� �Ѿ�� �� ����
			if (pSession->_Mode == MODE_AUTH_TO_GAME) {
				pSession->_Mode = MODE_GAME;
				pSession->OnGame_ClientJoin();
				_Monitor_SessionGame++;
			}

			//game�� ���¾� ó��
			if (pSession->_Mode == MODE_GAME) {
				//���⼭ Packet ó���Ҷ� GAME_PACKET, AUTH_PACKET �����ϱ�...
				while (pSession->_ComplateQ.Size() != 0) {
					pSession->_ComplateQ.Dequeue(&pRecvPacket);
					pSession->OnGame_Packet(pRecvPacket);
					pRecvPacket->Free();
				}
			}
		}
		//������ ����
		OnGame_Update();

		//release �� ��� üũ
		for (int i = 0; i < MaxSession; i++) {
			pSession = _SessionList[i];
			if (pSession->_ReleaseFlag == false) {
				if (pSession->_Mode == MODE_GAME) {
					pSession->OnGame_ClientLeave();
					pSession->_Mode = MODE_WAIT_RELEASE;
					_Monitor_SessionGame--;
				}
			}
			if (pSession->_Mode == MODE_WAIT_RELEASE) {
				InterlockedDecrement(&_Monitor_SessionAll);
				int SessionIndex = pSession->Release();
				_IndexSession.Push(SessionIndex);
				_Monitor_DisconnectTotal++;
			}
		}

		//���߿� ����
		_Monitor_GameUpdate_FPS++;
		Sleep(3);
	}
}

bool CGameServer::Start(ULONG OpenIP, USHORT Port, int NumWorkerthread, int NumIOCP, int MaxSession,
	int iBlockNum, bool bPlacementNew, bool Restart) {
	int retval;
	if (!Restart) {
		_wsetlocale(LC_ALL, L"Korean");
		timeBeginPeriod(1);
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			wprintf(L"WSAStartup() %d\n", WSAGetLastError());
			return false;
		}

		//IOCP ����
		_hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, NumIOCP);
		if (_hcp == NULL) {
			wprintf(L"CreateIoCompletionPort() %d\n", GetLastError());
			return 1;
		}

		_MaxSession = MaxSession;
		CreateSession();
		for (int i = _MaxSession - 1; i >= 0; i--)
			_IndexSession.Push(i);

	//PacketBuffer �ʱ�ȭ
		CPacket::Initial(iBlockNum, bPlacementNew);
	}
	_Listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (_Listen_sock == INVALID_SOCKET) {
		wprintf(L"socket() %d\n", WSAGetLastError());
		return false;
	}


	/*int optval = 0;
	setsockopt(_Listen_sock, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));*/

	SOCKADDR_IN serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(OpenIP);
	serveraddr.sin_port = htons(Port);

	retval = bind(_Listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) {
		wprintf(L"bind() %d\n", WSAGetLastError());
		return false;
	}

	retval = listen(_Listen_sock, SOMAXCONN_HINT(2048));
	if (retval == SOCKET_ERROR) {
		wprintf(L"listen() %d\n", WSAGetLastError());
	}
	_NumThread = NumWorkerthread;
	_hWorkerThread = new HANDLE[NumWorkerthread];
	for (int i = 0; i < NumWorkerthread; i++) {
		_hWorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThreadFunc, this, 0, NULL);
		if (_hWorkerThread[i] == NULL) return 1;
	}
	_hSendThread = (HANDLE)_beginthreadex(NULL, 0, SendThreadFunc, this, 0, NULL);
	_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThreadFunc, this, 0, NULL);
	_hAuthThread = (HANDLE)_beginthreadex(NULL, 0, AuthThreadFunc, this, 0, NULL);
	_hGameThread = (HANDLE)_beginthreadex(NULL, 0, GameThreadFunc, this, 0, NULL);
}


void CGameServer::RecvPost(CGameSession* pSession) {
	memset(&pSession->_RecvOverlapped, 0, sizeof(pSession->_RecvOverlapped.Overlapped));
	DWORD recvbyte = 0;
	DWORD lpFlags = 0;
	//Recv ��û�ϱ�
	char* rearBufPtr = pSession->_RecvQ.GetRearBufferPtr();
	char* bufPtr = pSession->_RecvQ.GetBufferPtr();
	if ((rearBufPtr != bufPtr) && (pSession->_RecvQ.GetFrontBufferPtr() <= rearBufPtr)) {
		WSABUF recvbuf[2];
		int iDirEnqSize = pSession->_RecvQ.DirectEnqueueSize();
		recvbuf[0].buf = rearBufPtr;
		recvbuf[0].len = iDirEnqSize;
		recvbuf[1].buf = bufPtr;
		recvbuf[1].len = pSession->_RecvQ.GetFreeSize() - iDirEnqSize;
		InterlockedIncrement64(&(pSession->_IOCount));
		int retval = WSARecv(pSession->_sock, recvbuf, 2, &recvbyte, &lpFlags, (WSAOVERLAPPED*)&pSession->_RecvOverlapped, NULL);
		if (retval == SOCKET_ERROR) {
			if (WSAGetLastError() != ERROR_IO_PENDING) {
				int WSA = WSAGetLastError();
				pSession->Disconnect();
				pSession->ReleaseSession();
			}
		}
	}
	else {
		WSABUF recvbuf;
		recvbuf.buf = rearBufPtr;
		recvbuf.len = pSession->_RecvQ.DirectEnqueueSize();
		InterlockedIncrement64(&(pSession->_IOCount));
		int retval = WSARecv(pSession->_sock, &recvbuf, 1, &recvbyte, &lpFlags, (WSAOVERLAPPED*)&pSession->_RecvOverlapped, NULL);
		if (retval == SOCKET_ERROR) {
			if (WSAGetLastError() != ERROR_IO_PENDING) {
				int WSA = WSAGetLastError();
				pSession->Disconnect();
				pSession->ReleaseSession();
			}
		}
	}
}

void CGameServer::SendPost(CGameSession* pSession) {
	if (!pSession->_SendFlag)
		return;
	pSession->_SendFlag = false;
	/*int retval = InterlockedExchange(&(pSession->_SendFlag), FALSE);
	if (retval == FALSE)
		return;*/
	int retval = InterlockedIncrement64(&(pSession->_IOCount));
	if (retval == 1) {
		//�� ��쿣 �̹� Release�� �ֱ� ������ 
		pSession->ReleaseSession();
		return;
	}
	memset(&pSession->_SendOverlapped, 0, sizeof(pSession->_SendOverlapped.Overlapped));
	DWORD sendbyte = 0;
	DWORD lpFlags = 0;
	WSABUF sendbuf[dfPACKETNUM];
	WORD i = 0;
	while (pSession->_SendQ.Size() > 0) {
		if (i >= dfPACKETNUM)
			break;
		pSession->_SendQ.Dequeue(&(pSession->_PacketArray[i]));
		sendbuf[i].buf = pSession->_PacketArray[i]->GetHeaderPtr();
		sendbuf[i].len = pSession->_PacketArray[i]->GetDataSize() + pSession->_PacketArray[i]->GetHeaderSize();
		//sendbuf[i].len = pSession->_PacketArray[i]->_iRear;
		i++;
	}
	pSession->_PacketCount = i;
	_Monitor_SendPacketTPS += i;
	retval = WSASend(pSession->_sock, sendbuf, pSession->_PacketCount, &sendbyte, lpFlags, (WSAOVERLAPPED*)&pSession->_SendOverlapped, NULL);
	if (retval == SOCKET_ERROR) {
		if (WSAGetLastError() != ERROR_IO_PENDING) {
			int WSA = WSAGetLastError();
			pSession->Disconnect();
			pSession->ReleaseSession();
		}
	}
}