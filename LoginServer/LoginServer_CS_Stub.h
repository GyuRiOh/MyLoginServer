#include <Windows.h>
#include "../NetRoot/NetServer/NetSessionID.h"
#include "../CommonProtocol.h"
#include "LoginServer.h"
#include "../NetRoot/Common/SizedMemoryPool.h"
#include "../NetRoot/NetServer/NetStub.h"

#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")

namespace MyNetwork
{
	class LoginServer_CS_Stub final : public NetStub
	{
	public:
		explicit LoginServer_CS_Stub(LoginServer* server) : server_(nullptr)
		{
			server_ = server;
		}

		void OnRecv(NetPacketSet* msgPack) override
		{
			if (!server_->stub_->NetStub::PacketProc(msgPack))
				server_->DisconnectAbnormalPlayer(msgPack->GetSessionID());
		}

		void OnWorkerClientJoin(NetSessionID sessionID) override
		{
			Player* player = Player::Alloc(sessionID);
			server_->connectedMap_.Insert_ExclusiveLock(player, sessionID.total_);
		}

		void OnWorkerClientLeave(NetSessionID sessionID) override
		{
			server_->ReleasePlayer(sessionID);
		}

		bool PacketProc(NetSessionID sessionID, NetDummyPacket* msg)
		{
			WORD type;
			*msg >> type;
			switch (type)
			{
			case en_PACKET_CS_LOGIN_REQ_LOGIN:
			{
				INT64 accountNo;
				char sessionKey[64] = {0};
				*msg >> accountNo;
				msg->DeqData((char*)sessionKey, 64);
				return ReqLoginLogin(accountNo, sessionKey, sessionID);
			}
			}
			return false;
		}

		bool ReqLoginLogin(INT64 accountNo, char* sessionKey, NetSessionID sessionID)
		{
			Player* player = nullptr;
			if (!server_->connectedMap_.Find_SharedLock(&player, sessionID.total_))
			{
				SystemLogger::GetInstance()->Console(L"ReqLoginLogin", LEVEL_DEBUG, L"Connected Map Find Failed");
				return false;
			}

			//레디스Job 세팅
			RedisJob* job = reinterpret_cast<RedisJob*>(
				SizedMemoryPool::GetInstance()->Alloc(sizeof(RedisJob)));
			job->accountNum = accountNo;
			job->sessionID = sessionID;
			memmove(job->sessionKey, sessionKey, 64);

			//DB에서 닉네임, ID 읽어오기
			server_->dbConnector_->Query(
				L"SELECT userid, usernick FROM accountdb.account WHERE accountno = %d;", accountNo);
			MYSQL_ROW result = server_->dbConnector_->FetchRow();

			size_t ret = 0;
			mbstowcs_s(&ret, job->ID, 20, result[0], 20);
			mbstowcs_s(&ret, job->nickName, 20, result[1], 20);

			server_->dbConnector_->FreeResult();

			if (accountNo < 1000000)
			{
				server_->proxy_->ResLoginLogin(
					job->accountNum,
					true,
					job->ID,
					job->nickName,
					server_->gameServerIP_,
					server_->gameServerPort_,
					server_->chatServerIP_,
					server_->chatServerPort_,
					job->sessionID);

				SizedMemoryPool::GetInstance()->Free(job);

			}
			else
			{

				job->player = player;

				//생성한 job을 레디스큐에 인큐
				if (!server_->redisQ_.Enqueue(job))
				{
					SystemLogger::GetInstance()->LogText(
						L"EnqueueRedisJob", LEVEL_ERROR, L"Enq Failed!");
					CrashDump::Crash();
				}

				SetEvent(server_->redisEvent_);

			}

			return true;
		}


	private:
		LoginServer* server_;
	};
}
