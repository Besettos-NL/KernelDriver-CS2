#include "math/vector.hpp"



class C_CSPlayerPawn
{
public:
	int health, team;

	Vector Position;

	uintptr_t pCSPlayerPawn;
};

inline C_CSPlayerPawn CCSPlayerPawn;


class Reader
{
public:
	DWORD pid = 0;
	uintptr_t client = 0;

	std::vector<C_CSPlayerPawn> playerList;

	void ThreadLoop();
private:
	void FilterPlayers();
};

inline Reader reader;
