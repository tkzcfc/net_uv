#include "FSTransfer.h"
#include <vector>
#include <assert.h>
#include <chrono>
#include "net_uv/net_uv.h"

// 分片大小
#define FST_SLICES_SIZE 4216

enum FST_ErrorCode
{
	FST_E_NOT_FOUND = 1,	// 文件不存在
	FST_E_OPEN_FAIL,		// 打开文件失败
	FST_E_NO_MEMERY,		// 内存不足
	FST_E_FILE_EXSIT,		// 文件已存在
	FST_E_NAME_ERROR,		// 文件名错误
};

struct FST_Task
{
	// 总大小
	uint32_t totalSize;
	FILE* fp;
	// 已传输大小
	uint32_t transmittedSize;

	std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;
	uint32_t lastSize;
};

//////////////////////////////////////////////////////////////////////////

enum FST_NET_MSGID
{
	// 推送文件
	FST_NET_POST_FILE,
	// 推送文件结果
	FST_NET_POST_FILE_RESULT,

	// 传输分片
	FST_NET_TRANSFER_SLICES,
	// 传输分片结果
	FST_NET_TRANSFER_SLICES_RESULT,

	// 下载文件
	FST_NET_DOWNLOAD,

	// 校验文件
	FST_NET_CHECK,
};

struct FST_Net_Base
{
	FST_NET_MSGID id;
};

struct FST_Net_SendFileBegin : FST_Net_Base
{
	FST_Net_SendFileBegin()
	{
		id = FST_NET_POST_FILE;
	}
	char name[256];	// 名称
	uint32_t size;  // 大小
};

struct FST_Net_SendFileBeginResult : FST_Net_Base
{
	FST_Net_SendFileBeginResult()
	{
		id = FST_NET_POST_FILE_RESULT;
	}
	char name[256];
	uint32_t code;
	uint32_t recvSize;
};

struct FST_TransferSlices : FST_Net_Base
{
	FST_TransferSlices()
	{
		id = FST_NET_TRANSFER_SLICES;
	}
	uint32_t begin;
	uint32_t size;
};

struct FST_TransferSlicesResult : FST_Net_Base
{
	FST_TransferSlicesResult()
	{
		id = FST_NET_TRANSFER_SLICES_RESULT;
	}
	uint32_t begin;
};

struct FST_Net_Download : FST_Net_Base
{
	FST_Net_Download()
	{
		id = FST_NET_DOWNLOAD;
	}
	char name[256];
};

struct FST_Check : FST_Net_Base
{
	FST_Check()
	{
		id = FST_NET_CHECK;
	}
	uint32_t size;

	uint32_t begin1;
	uint32_t size1;
	uint32_t hash1;

	uint32_t begin2;
	uint32_t size2;
	uint32_t hash2;
};


//////////////////////////////////////////////////////////////////////////

#define FST_SEND(_DATA) m_output(this, (char*)&_DATA, sizeof(_DATA));
#define FST_SEND_SIZE(_DATA, _SIZE) m_output(this, (char*)_DATA, _SIZE);

//////////////////////////////////////////////////////////////////////////

FSTransfer::FSTransfer()
	: m_duty(Duty::None)
	, m_task(nullptr)
	, m_output(nullptr)
{
	m_readBuf = new char[FST_SLICES_SIZE];
}

FSTransfer::~FSTransfer()
{
	delete[] m_readBuf;
}

int32_t FSTransfer::postFile(const std::string& file)
{
	if (file.empty())
	{
		return FST_E_NOT_FOUND;
	}
	FILE* fp = fopen(file.c_str(), "rb");
	if (fp == NULL)
	{
		printf("can not open file\n");
		return FST_E_OPEN_FAIL;
	}
	fseek(fp, 0, SEEK_END);
	uint32_t fileSize = ftell(fp);
	fclose(fp);
	
	FST_Net_SendFileBegin sendData;
	sendData.size = fileSize;
	strcpy(sendData.name, file.c_str());
	FST_SEND(sendData);

	m_duty = Duty::Send;
	m_filename = file;
	
	return 0;
}

void FSTransfer::downLoadFile(const std::string& file)
{
	FST_Net_Download data;
	strcpy(data.name, file.c_str());
	FST_SEND(data);

	m_duty = Duty::Recv;
	m_filename = file;
}

void FSTransfer::online(bool isOnline)
{
	if (isOnline)
	{
		switch (m_duty)
		{
		case FSTransfer::Send:
		{
			postFile(m_filename);
		}
			break;
		case FSTransfer::Recv:
		{
			downLoadFile(m_filename);
		}
			break;
		case FSTransfer::None:
			break;
		default:
			break;
		}
	}
	else
	{
		clearTask();
	}
}

void FSTransfer::updateFrame()
{
	if (m_task == NULL)
		return;

	std::chrono::time_point<std::chrono::high_resolution_clock> curTime = std::chrono::high_resolution_clock::now();

	int32_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(curTime - m_task->lastTime).count();
	if (milliseconds <= 0)
	{
		return;
	}
	if (milliseconds > 1000)
	{
		int32_t totalSize = m_task->transmittedSize - m_task->lastSize;
		if (totalSize < 0)
		{
			totalSize = 0;
		}
		totalSize = totalSize * 1000;

		float percent = (float)m_task->transmittedSize / (float)m_task->totalSize;
		percent *= 100.0f;
		float speed = (float)totalSize / 1024.0f / milliseconds;
		printf("[%.02f] %fkb/1s\n", percent, speed);

		m_task->lastSize = m_task->transmittedSize;
		m_task->lastTime = curTime;
	}
}

void FSTransfer::input(char* data, int32_t len)
{
	FST_Net_Base* baseData = (FST_Net_Base*)data;
	switch (baseData->id)
	{
	case FST_NET_POST_FILE:
	{
		on_PostFile(data, len);
	}break;
	case FST_NET_POST_FILE_RESULT:
	{
		on_PostFileResult(data, len);
	}break;
	case FST_NET_TRANSFER_SLICES:
	{
		FST_TransferSlices* msg = (FST_TransferSlices*)data;
		recvFileData(msg->begin, (char*)&msg[1], msg->size);
	}break;
	case FST_NET_TRANSFER_SLICES_RESULT:
	{
		FST_TransferSlicesResult* msg = (FST_TransferSlicesResult*)data;
		sendFileData(msg->begin);
	}break;
	case FST_NET_DOWNLOAD:
	{
		FST_Net_Download* msg = (FST_Net_Download*)data;
		postFile(msg->name);
	}break;
	case FST_NET_CHECK:
	{
		on_Check(data, len);
	}break;
	default:
	{}break;
	}
}

void FSTransfer::on_PostFile(void* data, uint32_t len)
{
	if (m_duty == Duty::None)
	{
		m_duty = Duty::Recv;
	}
	if (m_duty != Duty::Recv)
	{
		return;
	}

	if (m_task)
	{
		return;
	}

	FST_Net_SendFileBegin* msg = (FST_Net_SendFileBegin*)data;

	FST_Net_SendFileBeginResult resultData;
	resultData.code = 0;
	strcpy(resultData.name, msg->name);

	if (strlen(msg->name) > 0)
	{
		resultData.recvSize = getFileSize(msg->name);
		if (msg->size == resultData.recvSize)
		{
			resultData.code = FST_E_FILE_EXSIT;
		}
		else
		{
			if (msg->size < resultData.recvSize)
			{
				std::remove(msg->name);
				resultData.recvSize = 0;
			}
			m_filename = msg->name;

			FILE* fp = fopen(msg->name, "wb+");
			if (fp == NULL)
			{
				resultData.code = FST_E_OPEN_FAIL;
			}
			else
			{
				m_task = std::make_shared<FST_Task>();
				m_task->fp = fp;
				m_task->totalSize = msg->size;
				m_task->lastTime = std::chrono::high_resolution_clock::now();
				m_task->lastSize = resultData.recvSize;
				m_task->transmittedSize = resultData.recvSize;
			}
		}
	}
	else
	{
		resultData.code = FST_E_NAME_ERROR;
	}
	FST_SEND(resultData);
}

void FSTransfer::on_PostFileResult(void* data, uint32_t len)
{
	if (m_duty == Duty::None)
	{
		m_duty = Duty::Send;
	}
	if (m_duty != Duty::Send)
	{
		return;
	}
	if (m_task)
	{
		return;
	}

	FST_Net_SendFileBeginResult* msg = (FST_Net_SendFileBeginResult*)data;

	if (msg->code == 0)
	{
		FILE* fp = fopen(msg->name, "rb");
		if (fp == NULL)
		{
			printf("open file: %s fail\n", msg->name);
			return;
		}

		fseek(fp, 0, SEEK_END);
		uint32_t curSize = ftell(fp);

		m_filename = msg->name;
		m_task = std::make_shared<FST_Task>();
		m_task->fp = fp;
		m_task->totalSize = curSize;
		m_task->lastTime = std::chrono::high_resolution_clock::now();
		m_task->lastSize = curSize;
		m_task->transmittedSize = curSize;

		sendFileData(msg->recvSize);
		return;
	}
	else if (msg->code == FST_E_FILE_EXSIT)
	{
		printf("file: %s is send finish\n", msg->name);
	}
	else if (msg->code == FST_E_NAME_ERROR)
	{
		printf("file: %s is name error\n", msg->name);
	}
	clearTask();
}

static const uint32_t FST_constCheckBlockSize = 1024;

void FSTransfer::on_Check(void* data, uint32_t len)
{
	FST_Check* msg = (FST_Check*)data;

	fseek(m_task->fp, 0, SEEK_END);
	uint32_t totalSize = ftell(m_task->fp);

	if (totalSize == msg->size)
	{
		char szBuf[FST_constCheckBlockSize];

		fseek(m_task->fp, msg->begin1, SEEK_SET);
		fread(szBuf, msg->size1, 1, m_task->fp);
		uint32_t hash1 = net_uv::net_getBufHash(szBuf, msg->size1);

		fseek(m_task->fp, msg->begin2, SEEK_SET);
		fread(szBuf, msg->size2, 1, m_task->fp);
		uint32_t hash2 = net_uv::net_getBufHash(szBuf, msg->size2);

		if (msg->hash1 == hash1 && msg->hash2 == hash2)
		{
			printf("check suc\n");
		}
		else
		{
			printf("check fail\n");
		}
	}
	else
	{
		printf("check fail\n");
	}
	clearTask();
}

void FSTransfer::sendFileData(uint32_t begin)
{
	m_task->transmittedSize = begin;

	int32_t readSize = m_task->totalSize - begin;
	assert(readSize >= 0);
	if (readSize > FST_SLICES_SIZE)
	{
		readSize = FST_SLICES_SIZE;
	}
	if (readSize <= 0)
	{
		fseek(m_task->fp, 0, SEEK_END);
		uint32_t totalSize = ftell(m_task->fp);

		FST_Check checkData;
		checkData.size = totalSize;

		uint32_t checkBlockSize = FST_constCheckBlockSize;
		if (totalSize < checkBlockSize)
		{
			checkBlockSize = totalSize;
		}
		char szBuf[FST_constCheckBlockSize];

		checkData.begin1 = 0;
		checkData.size1 = checkBlockSize;
		fseek(m_task->fp, checkData.begin1, SEEK_SET);
		fread(szBuf, checkData.size1, 1, m_task->fp);
		checkData.hash1 = net_uv::net_getBufHash(szBuf, checkData.size1);

		checkData.begin2 = totalSize - checkBlockSize;
		checkData.size2 = checkBlockSize;
		fseek(m_task->fp, checkData.begin2, SEEK_SET);
		fread(szBuf, checkData.size2, 1, m_task->fp);
		checkData.hash2 = net_uv::net_getBufHash(szBuf, checkData.size2);

		FST_SEND(checkData);
		clearTask();
		return;
	}
	fseek(m_task->fp, begin, SEEK_SET);
	fread(m_readBuf, readSize, 1, m_task->fp);

	uint32_t datalen = sizeof(FST_TransferSlices) + readSize;
	char* pData = new char[datalen];

	FST_TransferSlices* transferData = (FST_TransferSlices*)pData;
	new (transferData) FST_TransferSlices();
	transferData->begin = begin;
	transferData->size = readSize;
	memcpy(&transferData[1], m_readBuf, readSize);

	FST_SEND_SIZE(pData, datalen);
	delete[]pData;
}

uint32_t FSTransfer::getFileSize(const char* name)
{
	FILE* fp = fopen(name, "rb");
	if (fp == NULL)
	{
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	uint32_t fileSize = ftell(fp);
	fclose(fp);
	return fileSize;
}

void FSTransfer::recvFileData(uint32_t begin, char* data, uint32_t len)
{
	if (len > 0)
	{
		fseek(m_task->fp, begin, SEEK_SET);
		fwrite(data, len, 1, m_task->fp);
	}
	m_task->transmittedSize = begin + len;

	FST_TransferSlicesResult result;
	result.begin = m_task->transmittedSize;
	FST_SEND(result);
}

void FSTransfer::clearTask()
{
	if (m_task)
	{
		fclose(m_task->fp);
		m_task = NULL;
		printf("clearTask\n");
	}
}
