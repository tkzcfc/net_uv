#include "../TestCommon.h"

void main()
{
	REGISTER_EXCEPTION("p2pTurn.dmp");

	P2PTurn* turn = new P2PTurn();
	turn->start("0.0.0.0", P2P_CONNECT_PORT);

	while (true)
	{
		if(KEY_DOWN(VK_ESCAPE))
			break;
		Sleep(1);
	}
	delete turn;

	printMemInfo();

	system("pause");
}
