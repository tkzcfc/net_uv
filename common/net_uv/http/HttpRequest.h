#pragma once

#include "HttpCommon.h"

NS_NET_UV_BEGIN

class HttpRequest
{
public:
	enum Method
	{
		kInvalid, kGet, kPost, kHead, kPut, kDelete
	};
	enum Version
	{
		kUnknown, kHttp10, kHttp11
	};

	HttpRequest();

	bool HttpRequest::setMethod(const char* start, const char* end);

	const char* HttpRequest::methodString() const;

	void HttpRequest::addHeader(const char* start, const char* colon, const char* end);

	std::string HttpRequest::getHeader(const std::string& field) const;

	void HttpRequest::swap(HttpRequest& that);

public:

	inline void setVersion(Version v);

	inline Version getVersion() const;

	inline Method method() const;

	inline void setPath(const char* start, const char* end);

	inline  const string& path() const;

	inline  void setQuery(const char* start, const char* end);

	inline  const string& query() const;

	inline const std::map<std::string, std::string>& headers() const;

private:
	Method m_method;
	Version m_version;
	std::string m_path;
	std::string m_query;
	std::map<std::string, std::string> m_headers;
};


void HttpRequest::setVersion(Version v)
{
	m_version = v;
}

HttpRequest::Version HttpRequest::getVersion() const
{
	return m_version;
}

HttpRequest::Method HttpRequest::method() const
{
	return m_method;
}

void HttpRequest::setPath(const char* start, const char* end)
{
	m_path.assign(start, end);
}

const string& HttpRequest::path() const
{
	return m_path;
}

void HttpRequest::setQuery(const char* start, const char* end)
{
	m_query.assign(start, end);
}

const string& HttpRequest::query() const
{
	return m_query;
}

const std::map<std::string, std::string>& HttpRequest::headers() const
{
	return m_headers;
}

NS_NET_UV_END