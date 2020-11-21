
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
	while (1) {
		retval = GetQueuedCompletionStatus(_hcp, &cbTransferred, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&Overlapped, INFINITE);
		//if (retval == false) {
		//	DWORD dwError = GetLastError();
		//	/*if (dwError != 64)
		//		Log(L"GQCS_CHAT", LEVEL_WARNING, (WCHAR*)L"SessionID: %lld, Overlapped Type: %d, Overlapped Error:%lld, WSAGetLastError: %d\n",
		//			pSession->SessionID, Overlapped->Type, Overlapped->Overlapped.Internal, dwError);*/
		//}
		//종료 처리
		if (Overlapped == NULL) {
			return 0;
		}
		/*if (&pSession->_RecvOverlapped != Overlapped && &pSession->_SendOverlapped != Overlapped) {
			CCrashDump::Crash();
		}*/
		//연결 끊기
		if (cbTransferred == 0) {
			pSession->Disconnect();
		}
		//Recv, Send 동작
		else {
			//recv인 경우
			if (Overlapped->Type == RECV) {
				//데이터 받아서 SendQ에 넣은 후 Send하기
				pSession->_RecvQ.MoveRear(cbTransferred);
				while (1) {
					iSessionUseSize = pSession->_RecvQ.GetUseSize();
					//헤더 사이즈 이상이 있는지 확인
					if (iSessionUseSize < NETHEADER)
						break;
					//데이터가 있는지 확인
					pSession->_RecvQ.Peek((char*)&stPacketHeader, NETHEADER);
					//헤더 코드 검증
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
					//여기서 OnRecv호출 전에 Decode를 통해 데이터 변조 유무 체크
					RecvPacket->Decode();
					if (RecvPacket->_CheckSum != (BYTE) * (RecvPacket->GetHeaderPtr() + 4)) {
						pSession->Disconnect();
						RecvPacket->Free();
						break;
					}
					//Decode된 Packet을 ComplateQ에 넣기
					pSession->_ComplateQ.Enqueue(RecvPacket);
					InterlockedIncrement(&_Monitor_RecvPacketTPS);
				}

				//Recv 요청하기
				RecvPost(pSession);

			}
			//send 완료 경우
			else if (Overlapped->Type == SEND) {
				for (int i = 0; i < pSession->_PacketCount; i++) {
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
		_Monitor_AcceptTPS;
		//빈 세션이 없을 경우 연결 끊기
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
	//AcceptQueue 확인 후 연결 생성
	CGameSession* pSession;
	while (1) {
		//한 프레임당 Accept횟수를 정해놓을까??
		while (_AcceptSocketQ.Size() != 0) {
			st_ACCEPT* pAcceptSocket;
			_AcceptSocketQ.Dequeue(&pAcceptSocket);
			int curIdx;
			_IndexSession.Pop(&curIdx);
			//있어선 안됨
			if (_SessionList[curIdx]->_Mode != MODE_NONE)
				CCrashDump::Crash();
			_SessionList[curIdx]->_sock = pAcceptSocket->sock;
			_SessionList[curIdx]->_clientaddr = pAcceptSocket->clientaddr;
			_SessionList[curIdx]->_SessionID = ++_SessionIDCnt;
			_SessionList[curIdx]->_SessionIndex = curIdx;
			memset(&_SessionList[curIdx]->_RecvOverlapped, 0, sizeof(_SessionList[curIdx]->_RecvOverlapped));
			memset(&_SessionList[curIdx]->_SendOverlapped, 0, sizeof(_SessionList[curIdx]->_SendOverlapped));
			_SessionList[curIdx]->_RecvOverlapped.Type = RECV;
			_SessionList[curIdx]->_SendOverlapped.Type = SEND;
			_SessionList[curIdx]->_RecvOverlapped.SessionID = _SessionList[curIdx]->_SessionID;
			_SessionList[curIdx]->_SendOverlapped.SessionID = _SessionList[curIdx]->_SessionID;
			_SessionList[curIdx]->_IOCount = 1;
			_SessionList[curIdx]->_RecvQ.ClearBuffer();
			_SessionList[curIdx]->_SendQ.Clear();
			_SessionList[curIdx]->_ComplateQ.Clear();
			_SessionList[curIdx]->_ReleaseFlag = TRUE;
			_SessionList[curIdx]->_SendFlag = TRUE;
			_SessionList[curIdx]->_PacketCount = 0;
			_SessionList[curIdx]->_bAuthToGameFlag = false;
			InterlockedIncrement(&_Monitor_SessionAll);
			CreateIoCompletionPort((HANDLE)_SessionList[curIdx]->_sock, _hcp, (ULONG_PTR)_SessionList[curIdx], 0);
			_SessionList[curIdx]->_Mode = MODE_AUTH;
			_Monitor_SessionAuth++;
			_SessionList[curIdx]->OnAuth_ClientJoin();
			RecvPost(_SessionList[curIdx]);
			_AcceptSocketPool.Free(pAcceptSocket);
			_SessionList[curIdx]->ReleaseSession();
		}

		//MODE_AUTH에 대한 Session 로직 처리
		for (int i = 0; i < _MaxSession; i++) {
			pSession = _SessionList[i];
			if (pSession->_Mode == MODE_AUTH) {
				//여기서 Packet 처리할때 GAME_PACKET, AUTH_PACKET 구분하기...
				while (pSession->_ComplateQ.Size() != 0) {
					//모드의 전환이 된 후 Recv를 받아 packet을 저장한 경우를 위한 조건
					if (pSession->_bAuthToGameFlag)
						break;
					CPacket* pRecvPacket;
					pSession->_ComplateQ.Dequeue(&pRecvPacket);
					//OnAuth_Packet에서 Auth_To_Game모드 전환까지 해야 할까?
					pSession->OnAuth_Packet(pRecvPacket);
					pRecvPacket->Free();
				}
			}
		}

		for (int i = 0; i < _MaxSession; i++) {
			pSession = _SessionList[i];
			if (pSession->_Mode == MODE_AUTH) {
				//release할 상대 체크
				if (pSession->_ReleaseFlag == false) {
					pSession->OnAuth_ClientLeave();
					pSession->_Mode = MODE_WAIT_RELEASE;
					_Monitor_SessionAuth--;
				}
				//Game모드로 전환 필요한 애 체크
				else if (pSession->_bAuthToGameFlag == true) {
					pSession->_Mode = MODE_AUTH_TO_GAME;
					_Monitor_SessionAuth--;
				}
			}
		}
		
		//나중에 가변으로 바꾸기
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
			//해당 모드의 경우만 메세지 보내기
			if (pSession->_Mode == MODE_AUTH || pSession->_Mode == MODE_GAME) {
				if(pSession->_SendQ.Size()>0)
					SendPost(pSession);
			}
		}
		//나중에 조정하기
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
			//auth에서 넘어온 애 세팅
			if (pSession->_Mode == MODE_AUTH_TO_GAME) {
				pSession->_Mode = MODE_GAME;
				pSession->OnGame_ClientJoin();
				_Monitor_SessionGame++;
			}

			//game에 들어온애 처리
			if (pSession->_Mode == MODE_GAME) {
				//여기서 Packet 처리할때 GAME_PACKET, AUTH_PACKET 구분하기...
				while (pSession->_ComplateQ.Size() != 0) {
					pSession->_ComplateQ.Dequeue(&pRecvPacket);
					pSession->OnGame_Packet(pRecvPacket);
					pRecvPacket->Free();
				}
			}
		}
		//컨텐츠 로직
		OnGame_Update();

		//release 할 대상 체크
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

		//나중에 조정
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

		//IOCP 생성
		_hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, NumIOCP);
		if (_hcp == NULL) {
			wprintf(L"CreateIoCompletionPort() %d\n", GetLastError());
			return 1;
		}

		_MaxSession = MaxSession;
		CreateSession();
		for (int i = _MaxSession - 1; i >= 0; i--)
			_IndexSession.Push(i);

	//PacketBuffer 초기화
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
	//Recv 요청하기
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
		//이 경우엔 이미 Release될 애기 때문에 
		pSession->ReleaseSession();
		return;
	}
	memset(&pSession->_SendOverlapped, 0, sizeof(pSession->_SendOverlapped.Overlapped));
	DWORD sendbyte = 0;
	DWORD lpFlags = 0;
	WSABUF sendbuf[dfPACKETNUM];
	WORD i = 0;
	/*while (pSession->_SendQ.Size() > 0) {
		if (pSession->_PacketCount >= dfPACKETNUM)
			break;
		pSession->_SendQ.Dequeue(&(pSession->_PacketArray[pSession->_PacketCount]));
		sendbuf[pSession->_PacketCount].buf = pSession->_PacketArray[pSession->_PacketCount]->GetHeaderPtr();
		sendbuf[pSession->_PacketCount].len = pSession->_PacketArray[pSession->_PacketCount]->GetDataSize() + pSession->_PacketArray[pSession->_PacketCount]->GetHeaderSize();
		pSession->_PacketCount++;
	}*/
	while (pSession->_SendQ.Size() > 0) {
		if (i >= dfPACKETNUM)
			break;
		pSession->_SendQ.Dequeue(&(pSession->_PacketArray[i]));
		sendbuf[i].buf = pSession->_PacketArray[i]->GetHeaderPtr();
		sendbuf[i].len = pSession->_PacketArray[i]->GetDataSize() + pSession->_PacketArray[i]->GetHeaderSize();
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