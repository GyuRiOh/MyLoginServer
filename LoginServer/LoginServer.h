#pragma once
#include "../NetRoot/NetServer/NetServer.h"
#include "../NetRoot/NetServer/NetSessionID.h"
#include "../NetRoot/Common/DBConnector.h"
#include "../Player/Player.h"
#include "../Player/PlayerMapWithLock.h"
#include "../NetRoot/Common/JobQueue.h"
#include "../RedisJob.h"
#include "LoginClient.h"

namespace MyNetwork
{
    class LoginServer_SC_Proxy;
    class LoginServer_CS_Stub;

	class LoginServer final : public NetRoot 
	{
	public:
		explicit LoginServer();
		~LoginServer();

    private:
        bool OnConnectionRequest(const SOCKADDR_IN* const addr) override; //Accept 직후. return false시 클라이언트 거부, true시 접속 허용
        void OnClientJoin(NetSessionID sessionID) override; //Accept 후 접속 처리 완료 후 호출.
        void OnClientLeave(NetSessionID sessionID) override; //Release 후 호출
        void OnRecv(NetPacketSet* packetList) override; //패킷 수신 완료 후
        void OnSend(NetSessionID sessionID, int sendSize) override; //패킷 송신 완료 후
        void OnWorkerThreadBegin() override; //워커스레드 GQCS 하단에서 호출
        void OnWorkerThreadEnd() override; //워커스레드 1루프 종료 후
        void OnError(const int errCode, WCHAR* const err) override;   
        void OnMonitor(const MonitoringInfo* const info) override;
        void OnStart() override {};
        void OnStop() override {};
            
        void DisconnectAbnormalPlayer(NetSessionID ID);
        bool ReleasePlayer(NetSessionID ID); 

    private:
        static DWORD WINAPI RedisThread(LPVOID arg);
    private:
        UserMapWithLock<Player*> connectedMap_;
        WCHAR gameServerIP_[16] = L"106.245.38.107";
        WCHAR chatServerIP_[16] = L"106.245.38.107";
        WCHAR chatServerIP2_[16] = L"10.0.1.1";
        WCHAR chatServerIP3_[16] = L"10.0.2.1";
        LoginServer_SC_Proxy* proxy_ = nullptr;
        LoginServer_CS_Stub* stub_ = nullptr;


        long authTPS_ = 0;
        USHORT gameServerPort_ = 11920;
        USHORT chatServerPort_ = 11914;
        DBConnector* dbConnector_ = nullptr;

        LockFreeEnqJobQ<RedisJob*, 4112> redisQ_;

        HANDLE redisEvent_ = INVALID_HANDLE_VALUE;
        HANDLE redisThread_ = INVALID_HANDLE_VALUE;

        LoginClient lanClient_;

        friend class LoginServer_SC_Proxy;
        friend class LoginServer_CS_Stub;
	};
}