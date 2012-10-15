#ifndef WING_MAIL_CNNPOOL_H
#define WING_MAIL_CNNPOOL_H


struct SQLInfo
{
	string ip;
	int port;
	string username;
	string password;
};

struct PoolParam
{
	string ip;
	int port;
	string username;
	string password;
	string database;

	PoolParam(const string& ip,int port,const string& user,
		const string& psd,const string& db):
	ip(ip),port(port),username(user),password(psd),database(db)
	{
	}

	PoolParam(){}
};


class CDBConnectionPool
	:public mysqlpp::ConnectionPool
{
public:
	CDBConnectionPool(
		const char* ip
		,int port
		,const char* user
		,const char* pwd
		,const char* dbname
		,const unsigned int uMaxIdleTime = 15*60)
		:m_uMaxIdleTime(uMaxIdleTime)
	{
		m_strDBHostName = ip;
		m_port		= port;
		m_strDBUserName = user;
		m_strDBPassword = pwd;
		m_strDBName     = dbname;
	}
	~CDBConnectionPool(void)
	{
		clear();
	}
	CDBConnectionPool(const PoolParam& param
		,const unsigned int uMaxIdleTime = 15*60)
		:m_uMaxIdleTime(uMaxIdleTime)
	{
		m_strDBHostName = param.ip;
		m_port		= param.port;
		m_strDBUserName = param.username;
		m_strDBPassword = param.password;
		m_strDBName     = param.database;
	}

	PoolParam GetPoolParam() const
	{
		return PoolParam(m_strDBHostName,m_port,
			m_strDBUserName,m_strDBPassword,m_strDBName);
	}


protected:
	virtual mysqlpp::Connection* create()
	{
		mysqlpp::Connection *con = new mysqlpp::Connection;
		try
		{
			con->connect(m_strDBName.c_str(),m_strDBHostName.c_str(),m_strDBUserName.c_str(),m_strDBPassword.c_str(),m_port);
		}
		catch (std::exception& e)
		{
			delete con;
			con=NULL;
			_error = e.what();
		}
		return con;
	}

	virtual void destroy(mysqlpp::Connection* pCnn)
	{
		if(pCnn)
		{
			if(pCnn->ping())
			{
				pCnn->disconnect();
			}
			delete pCnn;
		}			
	}

	virtual unsigned int max_idle_time(void)
	{
		return m_uMaxIdleTime;
	}
	
	
private:
	std::string	m_strDBHostName;
	std::string	m_strDBUserName;
	std::string	m_strDBPassword;
	std::string	m_strDBName;
	unsigned int	m_port;

	std::string _error;

	unsigned int m_uMaxIdleTime;
};


struct AutoReleaseConnection
{
public:
	AutoReleaseConnection(CDBConnectionPool* cpool)
	{
		_pool = cpool;
		_connection = NULL;

		_connection = _pool->grab();

		if (_connection && !_connection->ping())
		{
			_pool->release(_connection);
			_pool->shrink();
			_connection = _pool->grab();
		}
		if (_connection && !_connection->ping())
		{
		}
	}
	~AutoReleaseConnection()
	{
		if (_connection)
		{
			_pool->release(_connection);		
		}
	}

	mysqlpp::Connection* _connection;

private:
	CDBConnectionPool* _pool;
	 
};
 

typedef CDBConnectionPool CnnPool;
#endif //WING_MAIL_CNNPOOL_H