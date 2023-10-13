#include "../NetRoot/NetServer/NetPacket.h"
#include "../NetRoot/NetServer/NetSessionID.h"
#include "LoginServer.h"

namespace server_baby
{
	class LoginServer_SC_Proxy
	{
	public:
		explicit LoginServer_SC_Proxy(LoginServer* server) : server_(nullptr)
		{
			server_ = server;
		}

		void ResLoginLogin(INT64 accountNum, BYTE status, WCHAR* ID, WCHAR* nickName, WCHAR* gameServerIP, USHORT gameServerPort, WCHAR* chatServerIP, USHORT charServerPort, NetSessionID sessionID)
		{
			NetPacket* msg = NetPacket::Alloc();

			*msg << (unsigned short)en_PACKET_CS_LOGIN_RES_LOGIN;
			*msg << accountNum;
			*msg << status;
			msg->EnqData((char*)ID, 20*2);
			msg->EnqData((char*)nickName, 20*2);
			msg->EnqData((char*)gameServerIP, 16*2);
			*msg << gameServerPort;
			msg->EnqData((char*)chatServerIP, 16*2);
			*msg << charServerPort;

			server_->SendPacket(sessionID, msg);
			NetPacket::Free(msg);
		}

	private:
		LoginServer* server_;
	};
}
