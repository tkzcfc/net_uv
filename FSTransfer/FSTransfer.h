#pragma once

#include <inttypes.h>
#include <string>
#include <map>
#include <functional>
#include <memory>

class FSTransfer;

using FST_Output = std::function<void(FSTransfer* fst, char* data, uint32_t len)>;

struct FST_Task;

class FSTransfer
{
public:

	FSTransfer();

	virtual ~FSTransfer();

	int32_t postFile(const std::string& file);

	void downLoadFile(const std::string& file);

	void input(char* data, int32_t len);

	void online(bool isOnline);
	
	inline void setOutput(FST_Output output);

	void updateFrame();

protected:

	void on_PostFile(void* data, uint32_t len);

	void on_PostFileResult(void* data, uint32_t len);

	void on_Check(void* data, uint32_t len);

	uint32_t getFileSize(const char* name);

	void sendFileData(uint32_t begin);

	void recvFileData(uint32_t begin, char* data, uint32_t len);

	void clearTask();

protected:
	FST_Output m_output;

	std::shared_ptr<FST_Task> m_task;

	std::string m_filename;

	enum Duty
	{
		Send,
		Recv,
		None
	};
	Duty m_duty;

	char* m_readBuf;
};

void FSTransfer::setOutput(FST_Output output)
{
	m_output = output;
}

