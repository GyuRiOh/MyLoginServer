#pragma once
#include "../NetRoot/Common/MemTLS.h"
#include "../NetRoot/NetServer/NetSessionID.h"
#include "../PacketStruct.h"

using namespace std;
namespace server_baby
{
	class Player
	{
		//락오브젝트 추가하기
		explicit Player() = delete;
		~Player() = delete;
	public:

		static int GetUsedCount();
		static int GetCapacity();
		static Player* Alloc(const NetSessionID NetSessionID);
		static bool Free(Player* player);

		NetSessionID GetSessionID();

		INT64 GetAccountNumber() const;
		void SetAccountNum(INT64 num);
		void ReserveDestroy() noexcept { isDestroyReserved_ = true; }
		bool isDestroyReserved() const noexcept { return isDestroyReserved_; }


	private:
		void Initialize(const NetSessionID NetSessionID);
		void Destroy();

	private:
		INT64 accountNum_;
		NetSessionID sessionID_;
		bool isDestroyReserved_;
		static server_baby::MemTLS<Player>* playerPool_;
	};

	inline int Player::GetUsedCount()
	{
		return playerPool_->GetTotalUseCount();
	}

	inline int Player::GetCapacity()
	{
		return playerPool_->GetTotalCapacity();
	}

	inline Player* Player::Alloc(const NetSessionID NetSessionID)
	{
		Player* player = playerPool_->Alloc();
		player->Initialize(NetSessionID);
		return player;
	}

	inline bool Player::Free(Player* player)
	{
		player->Destroy();
		return playerPool_->Free(player);
	}

	inline NetSessionID Player::GetSessionID()
	{
		return sessionID_;
	}

	inline INT64 Player::GetAccountNumber() const
	{
		return accountNum_;
	}

	inline void Player::SetAccountNum(INT64 num)
	{
		accountNum_ = num;
	}

	inline void server_baby::Player::Initialize(const NetSessionID NetSessionID)
	{
		accountNum_ = NULL;
		sessionID_ = NetSessionID;
		isDestroyReserved_ = false;
	}

	inline void server_baby::Player::Destroy()
	{
		accountNum_ = NULL;
		sessionID_ = 0;
		isDestroyReserved_ = false;
	}

}