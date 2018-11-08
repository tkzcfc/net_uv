#pragma once

#include "../base/Common.h"
#include "../base/Socket.h"
#include "../base/Runnable.h"
#include "../base/Misc.h"
#include "../base/Session.h"
#include "../base/SessionManager.h"
#include "../base/Mutex.h"
#include "../base/Client.h"
#include "../base/Server.h"
#include "../kcp/KCPClient.h"
#include "../kcp/KCPServer.h"
#include "../udp/UDPSocket.h"
#include "../thirdparty/rapidjson/writer.h"
#include "../thirdparty/rapidjson/reader.h"
#include "../thirdparty/rapidjson/document.h"


#define P2P_U_MAKE_UINT64()

#define MAKEDWORDLONG(l, h) ((ULONGLONG)(((DWORD)(l)) | ((DWORDLONG)((DWORD)(h))) << 32))

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define LOWORD(l)           ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w)           ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))

