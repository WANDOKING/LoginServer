#include "NetLibrary/NetServer/NetUtils.h"
#include "LoginServer.h"

#include "NetLibrary/DBConnector/DBConnector.h"

#include <cpp_redis/cpp_redis>
#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")

void LoginServer::Start(const uint16_t port, const uint32_t maxSessionCount, const uint32_t iocpConcurrentThreadCount, const uint32_t iocpWorkerThreadCount)
{
	NetServer::Start(port, maxSessionCount, iocpConcurrentThreadCount, iocpWorkerThreadCount);

	mTimeoutCheckEvent = ::CreateWaitableTimer(NULL, FALSE, NULL);
	ASSERT_LIVE(mTimeoutCheckEvent != INVALID_HANDLE_VALUE, L"mTimeoutCheckEvent create failed");

	LARGE_INTEGER timerTime{};
	timerTime.QuadPart = -1 * (10'000 * static_cast<LONGLONG>(mTimeoutCheckInterval));
	::SetWaitableTimer(mTimeoutCheckEvent, &timerTime, mTimeoutCheckInterval, nullptr, nullptr, FALSE);

	mTimeoutThread = reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, timeoutThread, this, 0, nullptr));
}

void LoginServer::Shutdown(void)
{
	mbTimeoutThreadShutdownRegistered = true;

	::WaitForSingleObject(mTimeoutThread, INFINITE);

	::CloseHandle(mTimeoutCheckEvent);
	::CloseHandle(mTimeoutThread);

	NetServer::Shutdown();
}

void LoginServer::OnAccept(const uint64_t sessionID)
{
	mConnectTimeInfos.Enqueue(ConnectTimeInfo{ sessionID, ::timeGetTime() });
}

void LoginServer::OnReceive(const uint64_t sessionID, Serializer* packet)
{
	WORD messageType;

	*packet >> messageType;

	switch (messageType)
	{
	case en_PACKET_TYPE::en_PACKET_CS_LOGIN_REQ_LOGIN:
	{
		int64_t accountNo;
		char sessionKey[64];

		constexpr uint32_t PACKET_SIZE = sizeof(messageType) + sizeof(accountNo) + sizeof(sessionKey);
		if (packet->GetUseSize() != PACKET_SIZE)
		{
			Disconnect(sessionID);
			break;
		}

		*packet >> accountNo;
		packet->GetByte(sessionKey, sizeof(sessionKey));

		process_CS_LOGIN_REQ_LOGIN(sessionID, accountNo, sessionKey);
	}
	break;
	default:
		Disconnect(sessionID);
	}
	
	Serializer::Free(packet);
}

void LoginServer::OnRelease(const uint64_t sessionID)
{

}

void LoginServer::process_CS_LOGIN_REQ_LOGIN(const uint64_t sessionID, const int64_t accountNo, const char sessionKey[])
{
	WCHAR userID[20];
	WCHAR userNick[20];
	
	thread_local DBConnector connector(mDBIP, mDBUser, mDBPassword, mDBName, mDBPort); // 1스레드 1커넥션
	thread_local cpp_redis::client redisClient;										   // 1스레드 1커넥션

	if (false == connector.IsConnected())
	{
		if (false == connector.Connect())
		{
			LOGF(ELogLevel::System, L"DB Connect Failed (error code = %d)", connector.GetLastError());
			CrashDump::Crash();
		}
	}

	if (false == redisClient.is_connected())
	{
		redisClient.connect();
	}
	
	ASSERT_LIVE(connector.Query(L"SELECT * FROM account WHERE accountno = %lld", accountNo), L"Query Failed");

	// 테스트 환경에서 로그인이 실패하는 경우는 없음
	MYSQL_ROW result = connector.FetchRowOrNull();
	ASSERT_LIVE(result != nullptr, L"no accountNo in account table");

	MultiByteToWideChar(UNICODE, 0, result[1], static_cast<int>(strlen(result[1])) + 1, userID, sizeof(userID));
	MultiByteToWideChar(UNICODE, 0, result[3], static_cast<int>(strlen(result[3])) + 1, userNick, sizeof(userID));

	connector.FreeResult();

	SOCKADDR_IN address;
	WCHAR chatServerIP[16];

	if (false == GetSessionAddress(sessionID, &address))
	{
		return;
	}

	std::wstring ip = NetUtils::GetIpAddress(address);

	if (ip == L"10.0.1.2")
	{
		wcscpy_s(chatServerIP, 16, L"10.0.1.1"); // 더미 1
	}
	else if (ip == L"10.0.2.2")
	{
		wcscpy_s(chatServerIP, 16, L"10.0.2.1"); // 더미 2
	}
	else
	{
		wcscpy_s(chatServerIP, 16, mChatServerIP);
	}

	Serializer* packet = create_CS_LOGIN_RES_LOGIN(
		accountNo,
		en_PACKET_CS_LOGIN_RES_LOGIN::dfLOGIN_STATUS_OK,
		userID,
		userNick,
		mGameServerIP,
		mGameServerPort,
		chatServerIP,
		mChatServerPort);

	ASSERT_LIVE(connector.Query(L"SELECT * FROM sessionKey WHERE accountno = %lld", accountNo), L"Query Failed");
	connector.FreeResult();

	ASSERT_LIVE(connector.Query(L"SELECT * FROM status WHERE accountno = %lld", accountNo), L"Query Failed");
	connector.FreeResult();

	// 토큰 서버에 토큰 저장
	redisClient.setex(std::to_string(accountNo), 20, std::string(sessionKey, 64));
	redisClient.sync_commit();

	SendPacket(sessionID, packet);

	Serializer::Free(packet);
}

unsigned int LoginServer::timeoutThread(void* loginServerParam)
{
	LOGF(ELogLevel::System, L"LoginServer Timeout Thread Start (ID : %d)", ::GetCurrentThreadId());

	LoginServer* server = reinterpret_cast<LoginServer*>(loginServerParam);

	for (;;)
	{
		::WaitForSingleObject(server->mTimeoutCheckEvent, INFINITE);

		if (server->mbTimeoutThreadShutdownRegistered)
		{
			break;
		}

		ConnectTimeInfo connectionTimeInfo;

		while (server->mConnectTimeInfos.TryDequeue(connectionTimeInfo))
		{
			uint32_t currentTick = ::timeGetTime();

			uint32_t gap = currentTick - connectionTimeInfo.Tick;

			if (gap < server->mTimeoutMaxTime)
			{
				::Sleep(server->mTimeoutMaxTime - gap);
			}

			server->Disconnect(connectionTimeInfo.SessionID);
		}
	}

	LOGF(ELogLevel::System, L"LoginServer Timeout Thread End (ID : %d)", ::GetCurrentThreadId());

	return 0;
}
