#include "LoginServer.h"
#include "../CommonProtocol.h"
#include "../PacketStruct.h"
#include "LoginServer_SC_Proxy.h"
#include "LoginServer_CS_Stub.h"
#include "../NetRoot/Common/SizedMemoryPool.h"
#include "../NetRoot/Common/PDHMonitor.h"
#include "../NetRoot/Common/Parser.h"
#include <cpp_redis/cpp_redis>

server_baby::LoginServer::LoginServer()
{
	proxy_ = new server_baby::LoginServer_SC_Proxy(this);
	stub_ = new server_baby::LoginServer_CS_Stub(this);

    RegisterStub(stub_);

	dbConnector_ = new DBConnector(L"127.0.0.1",
		L"root",
		L"1234",
		L"accountdb",
		3306);

	unsigned int redisThreadID = NULL;

	redisThread_ = (HANDLE)_beginthreadex(
		NULL,
		0,
		(_beginthreadex_proc_type)&RedisThread,
		(LPVOID)this,
		0,
		&redisThreadID);

	if (!redisThread_)
		ErrorQuit(L"MonitorThread Start Failed");

	redisEvent_ = CreateEvent(NULL, FALSE, NULL, NULL);

    int relayPort = 0;
    Parser::GetInstance()->GetValue("RelayServerPort", (int*)&relayPort);
    SystemLogger::GetInstance()->Console(L"NetServer", LEVEL_DEBUG, L"Relay Server Port : %d", relayPort);

    char IP[16] = "127.0.0.1";
    lanClient_.Start(IP, relayPort);
}

server_baby::LoginServer::~LoginServer()
{
	delete proxy_;
	delete stub_;
	delete dbConnector_;
}

bool server_baby::LoginServer::OnConnectionRequest(const SOCKADDR_IN* const addr){ return true; }
void server_baby::LoginServer::OnClientJoin(NetSessionID sessionID) {}
void server_baby::LoginServer::OnClientLeave(NetSessionID sessionID) {}
void server_baby::LoginServer::OnWorkerThreadBegin(){}
void server_baby::LoginServer::OnWorkerThreadEnd(){}
void server_baby::LoginServer::OnError(int errCode, WCHAR* err){}
void server_baby::LoginServer::OnRecv(NetPacketSet* packetQ) {}
void server_baby::LoginServer::OnSend(NetSessionID sessionID, int sendSize){}

void server_baby::LoginServer::OnMonitor(const MonitoringInfo* const info)
{ 
    HardwareMonitor::CpuUsageForProcessor::GetInstance()->UpdateCpuTime();
    ProcessMonitor::CpuUsageForProcess::GetInstance()->UpdateCpuTime();
    ProcessMonitor::MemoryForProcess::GetInstance()->Update();

    time_t timer;
    timer = time(NULL);

    LanPacket* packet = LanPacket::Alloc();

    *packet << static_cast<WORD>(en_PACKET_SS_MONITOR_DATA_UPDATE);
    *packet << static_cast<BYTE>(dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN);
    *packet << true;
    *packet << static_cast<int>(timer);

    lanClient_.SendPacket(packet);
    packet->Clear();

    *packet << static_cast<WORD>(en_PACKET_SS_MONITOR_DATA_UPDATE);
    *packet << static_cast<BYTE>(dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU);
    *packet << static_cast<int>(ProcessMonitor::CpuUsageForProcess::GetInstance()->ProcessTotal());
    *packet << static_cast<int>(timer);

    lanClient_.SendPacket(packet);
    packet->Clear();

    *packet << static_cast<WORD>(en_PACKET_SS_MONITOR_DATA_UPDATE);
    *packet << static_cast<BYTE>(dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM);
    *packet << static_cast<int>(ProcessMonitor::MemoryForProcess::GetInstance()->GetPrivateBytes(L"LoginServer") / eMEGA_BYTE);
    *packet << static_cast<int>(timer);

    lanClient_.SendPacket(packet);
    packet->Clear();

    *packet << static_cast<WORD>(en_PACKET_SS_MONITOR_DATA_UPDATE);
    *packet << static_cast<BYTE>(dfMONITOR_DATA_TYPE_LOGIN_SESSION);
    *packet << static_cast<int>(connectedMap_.Size_SharedLock());
    *packet << static_cast<int>(timer);

    lanClient_.SendPacket(packet);
    packet->Clear();

    *packet << static_cast<WORD>(en_PACKET_SS_MONITOR_DATA_UPDATE);
    *packet << static_cast<BYTE>(dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL);
    *packet << static_cast<int>(info->packetCount_);
    *packet << static_cast<int>(timer);

    lanClient_.SendPacket(packet);
    packet->Clear();

    *packet << static_cast<WORD>(en_PACKET_SS_MONITOR_DATA_UPDATE);
    *packet << static_cast<BYTE>(dfMONITOR_DATA_TYPE_LOGIN_AUTH_TPS);
    *packet << static_cast<int>(authTPS_);
    *packet << static_cast<int>(timer);

    lanClient_.SendPacket(packet);

    LanPacket::Free(packet);

    authTPS_ = 0;

    SystemLogger::GetInstance()->Console(L"LoginServer", LEVEL_DEBUG, L"Recv TPS : %d", info->recvTPS_);
    SystemLogger::GetInstance()->Console(L"LoginServer", LEVEL_DEBUG, L"Send TPS : %d", info->sendTPS_);
    SystemLogger::GetInstance()->Console(L"LoginServer", LEVEL_DEBUG, L"Accept TPS : %d", info->acceptTPS_);
    SystemLogger::GetInstance()->Console(L"LoginServer", LEVEL_DEBUG, L"Session Count : %d", info->sessionCount_);
}

void server_baby::LoginServer::DisconnectAbnormalPlayer(NetSessionID ID)
{
	Disconnect(ID);
}

bool server_baby::LoginServer::ReleasePlayer(NetSessionID ID)
{
    Player* player = nullptr;
    if (!connectedMap_.Find_SharedLock(&player, ID.total_))
        return false;

    connectedMap_.Release_ExclusiveLock(ID.total_);
    Player::Free(player);
    return true;
}

DWORD __stdcall server_baby::LoginServer::RedisThread(LPVOID arg)
{
	LoginServer* server = (LoginServer*)arg;

	WORD version = MAKEWORD(2, 2);
	WSADATA data;
	int ret = WSAStartup(version, &data);
	if (ret != 0)
		server->ErrorQuit(L"WSAStartUp Failed");

	cpp_redis::client client;
	client.connect();

	for(;;)
	{
		WaitForSingleObject(server->redisEvent_, INFINITE);


		RedisJob* job = nullptr;
		while (server->redisQ_.Dequeue(&job))
		{
			char accountNum[32] = { 0 };
			_itoa(job->accountNum, accountNum, 10);

			client.setex(accountNum, 30, job->sessionKey);
			client.sync_commit();	

            server->authTPS_++;

			server->proxy_->ResLoginLogin(
				job->accountNum,
				true,
				job->ID,
				job->nickName,
				server->gameServerIP_,
				server->gameServerPort_,
				server->chatServerIP_,
				server->chatServerPort_,
				job->sessionID);

			SizedMemoryPool::GetInstance()->Free(job);
		}


	}

	WSACleanup();
	return 0;
}

