#include "LoginServer.h"
#include "MonitorClient.h"

#include <conio.h>
#include <Windows.h>
#include <process.h>
#include <Psapi.h>
#include <Pdh.h>

#include "NetLibrary/Tool/ConfigReader.h"
#include "NetLibrary/Logger/Logger.h"
#include "NetLibrary/Profiler/Profiler.h"
#include "NetLibrary/DBConnector/DBConnector.h"
#include "CommonProtocol.h"

#pragma comment(lib,"Pdh.lib")

LoginServer g_loginServer;

int main(void)
{
#ifdef PROFILE_ON
    LOGF(ELogLevel::System, L"Profiler: PROFILE_ON");
#endif

#pragma region config 파일 읽기
    const WCHAR* CONFIG_FILE_NAME = L"LoginServer.config";

    /*************************************** Config - Logger ***************************************/

    WCHAR inputLogLevel[10];

    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"LOG_LEVEL", inputLogLevel, sizeof(inputLogLevel)), L"ERROR: config file read failed (LOG_LEVEL)");

    if (wcscmp(inputLogLevel, L"DEBUG") == 0)
    {
        Logger::SetLogLevel(ELogLevel::System);
    }
    else if (wcscmp(inputLogLevel, L"ERROR") == 0)
    {
        Logger::SetLogLevel(ELogLevel::Error);
    }
    else if (wcscmp(inputLogLevel, L"SYSTEM") == 0)
    {
        Logger::SetLogLevel(ELogLevel::System);
    }
    else
    {
        ASSERT_LIVE(false, L"ERROR: invalid LOG_LEVEL");
    }

    LOGF(ELogLevel::System, L"Logger Log Level = %s", inputLogLevel);

    /*************************************** Config - NetServer ***************************************/

    uint32_t inputPortNumber;
    uint32_t inputMaxSessionCount;
    uint32_t inputConcurrentThreadCount;
    uint32_t inputWorkerThreadCount;
    uint32_t inputSetTcpNodelay;
    uint32_t inputSetSendBufZero;

    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"PORT", &inputPortNumber), L"ERROR: config file read failed (PORT)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"MAX_SESSION_COUNT", &inputMaxSessionCount), L"ERROR: config file read failed (MAX_SESSION_COUNT)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"CONCURRENT_THREAD_COUNT", &inputConcurrentThreadCount), L"ERROR: config file read failed (CONCURRENT_THREAD_COUNT)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"WORKER_THREAD_COUNT", &inputWorkerThreadCount), L"ERROR: config file read failed (WORKER_THREAD_COUNT)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"TCP_NODELAY", &inputSetTcpNodelay), L"ERROR: config file read failed (TCP_NODELAY)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"SND_BUF_ZERO", &inputSetSendBufZero), L"ERROR: config file read failed (SND_BUF_ZERO)");

    LOGF(ELogLevel::System, L"CONCURRENT_THREAD_COUNT = %u", inputConcurrentThreadCount);
    LOGF(ELogLevel::System, L"WORKER_THREAD_COUNT = %u", inputWorkerThreadCount);

    if (inputSetTcpNodelay != 0)
    {
        g_loginServer.SetTcpNodelay(true);
        LOGF(ELogLevel::System, L"myChatServer.SetTcpNodelay(true)");
    }

    if (inputSetSendBufZero != 0)
    {
        g_loginServer.SetSendBufferSizeToZero(true);
        LOGF(ELogLevel::System, L"myChatServer.SetSendBufferSizeToZero(true)");
    }

    /*************************************** Config - LoginServer ***************************************/

    WCHAR inputGameServerIP[16];
    uint32_t inputGameServerPort;
    WCHAR inputChatServerIP[16];
    uint32_t inputChatServerPort;

    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"GAME_SERVER_IP", inputGameServerIP, 16), L"ERROR: config file read failed (GAME_SERVER_IP)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"GAME_SERVER_PORT", &inputGameServerPort), L"ERROR: config file read failed (GAME_SERVER_PORT)");
    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"CHAT_SERVER_IP", inputChatServerIP, 16), L"ERROR: config file read failed (CHAT_SERVER_IP)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"CHAT_SERVER_PORT", &inputChatServerPort), L"ERROR: config file read failed (CHAT_SERVER_PORT)");

    g_loginServer.SetGameServerConnectionInfo(inputGameServerIP, inputGameServerPort);
    g_loginServer.SetChatServerConnectionInfo(inputChatServerIP, inputChatServerPort);

    LOGF(ELogLevel::System, L"GameServer = %s:%u", inputGameServerIP, inputGameServerPort);
    LOGF(ELogLevel::System, L"ChatServer = %s:%u", inputChatServerIP, inputChatServerPort);

    // Timeout config input

    uint32_t inputTimeoutCheckInterval;
    uint32_t inputTimeoutMax;

    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"TIMEOUT_CHECK_INTERVAL", &inputTimeoutCheckInterval), L"ERROR: config file read failed (TIMEOUT_CHECK_INTERVAL)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"TIMEOUT_MAX", &inputTimeoutMax), L"ERROR: config file read failed (TIMEOUT_MAX)");

    g_loginServer.SetTimeoutCheckInterval(inputTimeoutCheckInterval);
    g_loginServer.SetTimeoutMaxTime(inputTimeoutMax);

    // DB Connection config input

    WCHAR inputDBIP[64];
    WCHAR inputDBUser[64];
    WCHAR inputDBPassword[64];
    WCHAR inputDBName[64];
    uint32_t inputDBPort;

    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"DB_IP", inputDBIP, 64), L"ERROR: config file read failed (DB_IP)");
    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"DB_USER", inputDBUser, 64), L"ERROR: config file read failed (DB_USER)");
    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"DB_PASSWORD", inputDBPassword, 64), L"ERROR: config file read failed (DB_PASSWORD)");
    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"DB_NAME", inputDBName, 64), L"ERROR: config file read failed (DB_NAME)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"DB_PORT", &inputDBPort), L"ERROR: config file read failed (DB_PORT)");

    g_loginServer.SetDBConnectionInfo(inputDBIP, inputDBUser, inputDBPassword, inputDBName, inputDBPort);

    LOGF(ELogLevel::System, L"DB IP = %s", inputDBIP);
    LOGF(ELogLevel::System, L"DB User = %s", inputDBUser);
    LOGF(ELogLevel::System, L"DB Password = %s", inputDBPassword);
    LOGF(ELogLevel::System, L"DB Name = %s", inputDBName);
    LOGF(ELogLevel::System, L"DB Port = %d", inputDBPort);

    // MonitoringServer config input

    WCHAR inputMonitoringServerIP[16];
    uint32_t inputMonitoringServerPort;

    ASSERT_LIVE(ConfigReader::GetString(CONFIG_FILE_NAME, L"MONITORING_SERVER_IP", inputMonitoringServerIP, 16), L"ERROR: config file read failed (MONITORING_SERVER_IP)");
    ASSERT_LIVE(ConfigReader::GetInt(CONFIG_FILE_NAME, L"MONITORING_SERVER_PORT", &inputMonitoringServerPort), L"ERROR: config file read failed (MONITORING_SERVER_PORT)");

    MonitorClient monitorClient(std::wstring(inputMonitoringServerIP, 16), inputMonitoringServerPort);

    if (false == monitorClient.Connect())
    {
        LOGF(ELogLevel::Error, L"ERROR: MonitoringServer Connect Failed");
    }
    else
    {
        monitorClient.SendLoginPacket();
    }

#pragma endregion

#pragma region 모니터링 서버 정보 얻기 PDH 사전 작업

    WCHAR processName[MAX_PATH];
    WCHAR query[MAX_PATH];
    PDH_HQUERY queryHandle;
    PDH_HCOUNTER privateBytes;

    // 프로세스 이름 얻기
    ASSERT_LIVE(::GetProcessImageFileName(::GetCurrentProcess(), processName, MAX_PATH) != 0, L"GetProcessImageFileName() Failed");
    _wsplitpath_s(processName, NULL, 0, NULL, 0, processName, MAX_PATH, NULL, 0);

    // PDH 쿼리 핸들 생성
    PdhOpenQuery(NULL, NULL, &queryHandle);

    // PDH 리소스 카운터 생성
    wsprintf(query, L"\\Process(%s)\\Private Bytes", processName);
    ASSERT_LIVE(PdhAddCounter(queryHandle, query, NULL, &privateBytes) == ERROR_SUCCESS, L"PdhAddCounter Error");

    // 첫 갱신
    PdhCollectQueryData(queryHandle);

#pragma endregion

    DBConnector::InitializeLibrary();

    g_loginServer.SetMaxPayloadLength(74);

    g_loginServer.Start(static_cast<uint16_t>(inputPortNumber), inputMaxSessionCount, inputConcurrentThreadCount, inputWorkerThreadCount);

    HANDLE mainThreadTimer = ::CreateWaitableTimer(NULL, FALSE, NULL);
    ASSERT_LIVE(mainThreadTimer != INVALID_HANDLE_VALUE, L"mainThreadTimer create failed");

    LARGE_INTEGER timerTime{};
    timerTime.QuadPart = -1 * (10'000 * static_cast<LONGLONG>(1'000));
    ::SetWaitableTimer(mainThreadTimer, &timerTime, 1'000, nullptr, nullptr, FALSE);

    for (;;)
    {
        ::WaitForSingleObject(mainThreadTimer, INFINITE);

        if (_kbhit())
        {
            int input = _getch();
            if (input == 'Q' || input == 'q')
            {
                g_loginServer.Shutdown();
                break;
            }
            else if (input == 'S' || input == 's')
            {
#ifdef PROFILE_ON
                const WCHAR* PROFILE_FILE_NAME = L"Profile_LoginServer.txt";
                PROFILE_SAVE(PROFILE_FILE_NAME);
                LOGF(ELogLevel::System, L"%s saved", PROFILE_FILE_NAME);
#endif
            }
        }

        MonitoringVariables monitoringInfo = g_loginServer.GetMonitoringInfo();

        // Pdh - private bytes
        ::PdhCollectQueryData(queryHandle);

        PDH_FMT_COUNTERVALUE privateBytesValue;
        ::PdhGetFormattedCounterValue(privateBytes, PDH_FMT_LONG, NULL, &privateBytesValue);

        // 모니터링 서버에 모니터링 정보 전송
        if (monitorClient.IsConnected())
        {
            int32_t timeStamp = static_cast<int32_t>(time(nullptr));
            monitorClient.Send_MONITOR_DATA_UPDATE(en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN, 1, timeStamp);
            monitorClient.Send_MONITOR_DATA_UPDATE(en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU, static_cast<int32_t>(monitoringInfo.ProcessTimeTotal), timeStamp);
            monitorClient.Send_MONITOR_DATA_UPDATE(en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM, privateBytesValue.longValue / 1'000'000, timeStamp);
            monitorClient.Send_MONITOR_DATA_UPDATE(en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_LOGIN_SESSION, g_loginServer.GetSessionCount(), timeStamp);
            monitorClient.Send_MONITOR_DATA_UPDATE(en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_LOGIN_AUTH_TPS, monitoringInfo.RecvMessageTPS, timeStamp);
            monitorClient.Send_MONITOR_DATA_UPDATE(en_PACKET_SS_MONITOR_DATA_UPDATE::dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL, Serializer::GetTotalPacketCount(), timeStamp);
        }

        wprintf(L"\n");
        //LOG_CURRENT_TIME();
        wprintf(L"[ LoginServer Running (S: profile save)(Q: quit)]\n");
        wprintf(L"=================================================\n");
        wprintf(L"Session Count        = %u / %u\n", g_loginServer.GetSessionCount(), g_loginServer.GetMaxSessionCount());
        wprintf(L"Accept Total         = %llu\n", g_loginServer.GetTotalAcceptCount());
        wprintf(L"Disconnected Total   = %llu\n", g_loginServer.GetTotalDisconnectCount());
        wprintf(L"Packet Pool Size     = %u\n", Serializer::GetTotalPacketCount());
        wprintf(L"---------------------- TPS ----------------------\n");
        wprintf(L"Accept TPS           = %9u (Avg: %9u)\n", monitoringInfo.AcceptTPS, monitoringInfo.AverageAcceptTPS);
        wprintf(L"Send Message TPS     = %9u (Avg: %9u)\n", monitoringInfo.SendMessageTPS, monitoringInfo.AverageSendMessageTPS);
        wprintf(L"Recv Message TPS     = %9u (Avg: %9u)\n", monitoringInfo.RecvMessageTPS, monitoringInfo.AverageRecvMessageTPS);
        wprintf(L"Send Pending TPS     = %9u (Avg: %9u)\n", monitoringInfo.SendPendingTPS, monitoringInfo.AverageSendPendingTPS);
        wprintf(L"Recv Pending TPS     = %9u (Avg: %9u)\n", monitoringInfo.RecvPendingTPS, monitoringInfo.AverageRecvPendingTPS);
        wprintf(L"----------------------- CPU ---------------------\n");
        wprintf(L"Total  = Processor: %6.3f / Process: %6.3f\n", monitoringInfo.ProcessorTimeTotal, monitoringInfo.ProcessTimeTotal);
        wprintf(L"User   = Processor: %6.3f / Process: %6.3f\n", monitoringInfo.ProcessorTimeUser, monitoringInfo.ProcessTimeUser);
        wprintf(L"Kernel = Processor: %6.3f / Process: %6.3f\n", monitoringInfo.ProcessorTimeKernel, monitoringInfo.ProcessTimeKernel);
    }

    ::CloseHandle(mainThreadTimer);

    return 0;
}