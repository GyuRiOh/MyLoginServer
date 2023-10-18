#pragma once
#include "NetRoot/NetServer/NetSessionID.h"
#include "Player/Player.h"

namespace MyNetwork
{
	struct RedisJob
	{
		char sessionKey[64] = { 0 };
		WCHAR nickName[20] = { 0 };
		WCHAR ID[20] = { 0 };
		INT64 accountNum = NULL;
		Player* player = nullptr;
		NetSessionID sessionID;
	};
}
