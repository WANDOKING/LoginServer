#pragma once

#include <cstring>

#include "NetLibrary/NetServer/NetServer.h"
#include "CommonProtocol.h"
#include "NetLibrary/DataStructure/LockFreeQueue.h"
#include <Windows.h>

class LoginServer : public NetServer
{
public:

#pragma warning(push)
#pragma warning(disable: 26495) // 변수 초기화 경고 제거
    LoginServer() = default;
#pragma warning(pop)

    virtual ~LoginServer() = default;

public: // 서버 시작 전 세팅 함수들

    // 타임 아웃 체크 간격
    inline void	SetTimeoutCheckInterval(const uint32_t interval) { mTimeoutCheckInterval = interval; }

    // 타임 아웃 시간
    inline void	SetTimeoutMaxTime(const uint32_t maxTime) { mTimeoutMaxTime = maxTime; }

    // DB 연결 정보 설정
    inline void SetDBConnectionInfo(const WCHAR* DBIP, const WCHAR* DBUser, const WCHAR* DBPassword, const WCHAR* DBName, uint16_t DBPort)
    {
        wcscpy_s(mDBIP, 64, DBIP);
        wcscpy_s(mDBUser, 64, DBUser);
        wcscpy_s(mDBPassword, 64, DBPassword);
        wcscpy_s(mDBName, 64, DBName);
        mDBPort = DBPort;
    }

    // 게임 서버 연결 정보 설정
    inline void SetGameServerConnectionInfo(const std::wstring& ip, const uint16_t port)
    {
        wcscpy_s(mGameServerIP, 16, ip.c_str());
        mGameServerPort = port;
    }

    // 채팅 서버 연결 정보 설정
    inline void SetChatServerConnectionInfo(const std::wstring& ip, const uint16_t port)
    {
        wcscpy_s(mChatServerIP, 16, ip.c_str());
        mChatServerPort = port;
    }

public:

    // 서버 시작
    virtual void Start(
        const uint16_t port,
        const uint32_t maxSessionCount,
        const uint32_t iocpConcurrentThreadCount,
        const uint32_t iocpWorkerThreadCount) override;

    // 서버 종료 (모든 스레드가 종료될 때 까지 블락됨)
    virtual void Shutdown(void) override;

    // NetServer을(를) 통해 상속됨
    virtual void OnAccept(const uint64_t sessionID) override;
    virtual void OnReceive(const uint64_t sessionID, Serializer* packet) override;
    virtual void OnRelease(const uint64_t sessionID) override;

private: // 메세지 관련

    inline static Serializer* create_CS_LOGIN_RES_LOGIN(
        const int64_t accountNo, 
        const uint8_t status, 
        const WCHAR id[], 
        const WCHAR nickName[], 
        const WCHAR gameServerIP[],
        const USHORT gameServerPort,
        const WCHAR chatServerIP[],
        const USHORT chatServerPort)
    {
        Serializer* packet = Serializer::Alloc();

        *packet << (WORD)en_PACKET_CS_LOGIN_RES_LOGIN << accountNo << status;
        packet->InsertByte((const char*)id, sizeof(WCHAR) * 20);
        packet->InsertByte((const char*)nickName, sizeof(WCHAR) * 20);
        packet->InsertByte((const char*)gameServerIP, sizeof(WCHAR) * 16);
        *packet << gameServerPort;
        packet->InsertByte((const char*)chatServerIP, sizeof(WCHAR) * 16);
        *packet << chatServerPort;

        return packet;
    }

    void process_CS_LOGIN_REQ_LOGIN(const uint64_t sessionID, const int64_t accountNo, const char sessionKey[]);

private:

    struct ConnectTimeInfo
    {
        uint64_t SessionID;
        uint32_t Tick;
    };

    static unsigned int timeoutThread(void* loginServerParam);

private:
    WCHAR       mGameServerIP[16];
    uint16_t    mGameServerPort;
    WCHAR       mChatServerIP[16];
    uint16_t    mChatServerPort;

    WCHAR       mDBIP[64];          // DB IP
    WCHAR       mDBUser[64];        // DB 유저 이름
    WCHAR       mDBPassword[64];    // DB 비밀번호
    WCHAR       mDBName[64];        // DB 스키마 이름
    uint16_t    mDBPort;            // DB Port

    bool        mbTimeoutThreadShutdownRegistered;
    HANDLE      mTimeoutThread;
    HANDLE      mTimeoutCheckEvent;
    uint32_t	mTimeoutCheckInterval;
    uint32_t	mTimeoutMaxTime;
    LockFreeQueue<ConnectTimeInfo> mConnectTimeInfos;
};