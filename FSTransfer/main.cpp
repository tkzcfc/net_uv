#include "FSTransferPool.h"
#include <iostream>

void main()
{
	FSTransferPool* pool = new FSTransferPool();;

	char type;
	std::cin >> type;

	if (type == 'c')
	{
		char inputFileBuf[256];
		std::cout << "请输入要传输的文件名:";
		std::cin >> inputFileBuf;
		pool->download("39.105.20.204", 1005, inputFileBuf);
	}
	else if (type == 's')
	{
		pool->listen("0.0.0.0", 1005, false);
	}

	while (true)
	{
		pool->updateFrame();
		ThreadSleep(1);
	}
	net_uv::printMemInfo();
	system("pause");
}