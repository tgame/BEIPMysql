// MysqlBlobExImPort.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "CnnPool.h"
#include "SerialStream.h"

#ifdef _DEBUG
#pragma comment(lib,"mysqlpp")
#else
#pragma comment(lib,"mysqlpp")
#endif

CDBConnectionPool* gs_conn=0;

struct DataSingleRow 
{
	vector<string>	m_fields;
	size_t size(void){
		return m_fields.size();
	}
	string& operator[](unsigned int i)
	{
		return m_fields[i];
	}
	_SS_START_(DataSingleRow)
		_SS_READ(m_fields)
	_SS_SEP_()
		_SS_WRITE(m_fields)
	_SS_END_()
};

struct DataFieldInfo 
{
	string m_name;
	//!插入sql时,需要转置
	bool	m_needEscape;
	_SS_START_(DataFieldInfo)
		_SS_READ(m_name)
		_SS_READ(m_needEscape)
	_SS_SEP_()
		_SS_WRITE(m_name)
		_SS_WRITE(m_needEscape)
	_SS_END_()
};

struct DataInfo 
{
	string m_sql;
	vector<DataFieldInfo>	m_rowInfo;
	vector<DataSingleRow>	m_rows;
	_SS_START_(DataInfo)
		_SS_READ(m_sql)
		_SS_READ(m_rowInfo)
		_SS_READ(m_rows)
	_SS_SEP_()
		_SS_WRITE(m_sql)
		_SS_WRITE(m_rowInfo)
		_SS_WRITE(m_rows)
	_SS_END_()
	void AppendFieldString(string& filedListString)
	{
		for(size_t i=0;i<m_rowInfo.size();i++)
		{
			filedListString += m_rowInfo[i].m_name;
			if (i!=m_rowInfo.size()-1)
			{
				filedListString += ",";
			}
		}
	}
	string BuildUpdateSql(DataSingleRow& row)
	{
		string rtn;
		size_t col = row.size();
		for(size_t i=0;i<col;++i)
		{
			rtn += m_rowInfo[i].m_name;
			rtn += "=";
			if(row[i].empty())
			{
				rtn += "NULL";
				if(i!=col-1)
					rtn += ",";

				continue;
			}
			if(m_rowInfo[i].m_needEscape)
			{
				rtn += "x'";
				unsigned long len = row[i].length();
				char *hexStr = (char*)malloc(len*2+1);
				mysql_hex_string(hexStr,row[i].c_str(),len);
				rtn +=  hexStr;
				rtn += "'";
				free(hexStr);
			}
			else
				rtn += row[i];		

			if(i!=col-1)
				rtn += ",";
		}

		//rtn += ");";
		return rtn;
	}
};
string BuildValueSql(DataSingleRow& row,const vector<DataFieldInfo>& escapeField);


void Export(const string& sql,const string& filename)
{
	DataInfo data;
	AutoReleaseConnection acon(gs_conn);
	mysqlpp::Connection* con = acon._connection;
	if (!con)
	{
		printf("sql connect failed!!");
		getchar();
		return;
	}
	mysqlpp::Query query = con->query();
	query<<sql;
	printf("exec sql:%s\r\n",query.str().c_str());
	
	mysqlpp::StoreQueryResult result =  query.store();

	if(result.size()<1)
	{
		printf("sql exec result is empty!!!");
		return;
	}
	

	for(size_t i=0;i<result.fields().size();++i)
	{
		const mysqlpp::Field& field = result.field(i);
		DataFieldInfo fieldInfo;
		fieldInfo.m_name=field.name();
		fieldInfo.m_needEscape=field.type().quote_q();
		data.m_rowInfo.push_back(fieldInfo);
		if (fieldInfo.m_needEscape)
		{
			printf("fetch field:%s quote_q\r\n",fieldInfo.m_name.c_str());
		}
		else
		{
			printf("fetch field:%s\r\n",fieldInfo.m_name.c_str());
		}
	}
	string vvalue;
	for (unsigned int rowIndex=0;rowIndex<result.size();rowIndex++)
	{
		const mysqlpp::Row& row = result[rowIndex];	
		DataSingleRow myrow;
		for(size_t i=0;i<row.size();++i)
		{
			const mysqlpp::String& fieldValue = row[i];
			
			vvalue.assign(fieldValue.c_str(),fieldValue.size());
			myrow.m_fields.push_back(vvalue);
		}
		data.m_rows.push_back(myrow);
	}
	SerialStream stream(1024);
	stream.Write(data);
	FILE* fp = fopen(filename.c_str(),"wb");
	if (fp)
	{
		fwrite(stream.GetRawBuffer(),stream.Size(),1,fp);
		fclose(fp);
		printf("Save all field into file :%s",filename.c_str());
		return;
	}
	printf("open file :%s failed!",filename.c_str());
}

void Import(const string& sql,const string& filename,const string& id,const string& table)
{
	DataInfo data;
	SerialStream stream(1024);
	FILE* fp = fopen(filename.c_str(),"rb");
	if (fp)
	{
		fseek(fp,0,SEEK_END);
		unsigned int l = ftell(fp);
		char* buf = new char[l];
		fseek(fp,0,SEEK_SET);
		fread(buf,l,1,fp);
		stream.Assign(buf,l);
		fclose(fp);
		stream.Read(data);
		delete []buf;
	}
	else
	{
		printf("open file failed :%s",filename.c_str());
		getchar();
		return;
	}
	string filedList;
	data.AppendFieldString(filedList);
	printf("file read row:%d\r\n",data.m_rows.size());
	printf("file filed:%s\r\n",filedList.c_str());
	if (!data.m_rows.empty())
	{
		string s = data.BuildUpdateSql(data.m_rows.front());
		printf("Test Sql:%s\r\n",s.c_str());
	}
	
	AutoReleaseConnection acon(gs_conn);
	mysqlpp::Connection* con = acon._connection;
	if (!con)
	{
		printf("sql connect failed!!");
		getchar();
		return;
	}
	string values;
	string pre_sql;
	for (unsigned int i=0;i<data.m_rows.size();i++)
	{
		DataSingleRow& dataRow=data.m_rows[i];
		mysqlpp::Query query = con->query();
		/*query<<"update "<<table<<" set (";
		query<<filedList<<") values(";
		values=BuildValueSql(dataRow,data.m_rowInfo);
		query<<values<<") where "<<sql<<";";*/
		query<<"update "<<table<<" set ";
		query<<data.BuildUpdateSql(dataRow);
		query<<" where "<<sql<<";";
		pre_sql=query.str();
		printf("exec sql:%s\r\n",pre_sql.c_str());
		mysqlpp::SimpleResult ret = query.execute();
		size_t affect_row = ret.rows();
		if (ret.rows() == 0)
		{
			printf("sql exec failed..is the where sql error:\r\n");
			printf("update %s where %s",table.c_str(),sql.c_str());
			getchar();
			return;
		}
		else
		{
			printf("sql import filed:%s,affect row:%d\r\n",filedList.c_str(),affect_row);
		}
	}
}

void ClearSaleDB(void);


string FetchArg(const char* ptr,const char endChar)
{
	string v;
	while (*ptr && *ptr != endChar)
	{
		v.push_back(*ptr);
		ptr++;
	}
	return v;
}

int _tmain(int argc, _TCHAR* argv[])
{
	string host;
	string user;
	string pwd;
	string dbname;
	string port_str;
	string sql_table;
	int port=3306;
	char op = 'e';
	string sql;
	string file;
	string import_id_filed;
	int isArgReaded=0;
	bool isClearSaleDB=false;
	for (int i=1;i<argc;i++)
	{
		if (argv[i][0] != '-')
		{
			file=argv[i];
			isArgReaded++;
			continue;
		}
		if (argv[i][1]=='h')
		{
			host=argv[i]+2;
			isArgReaded++;
		}
		else if (argv[i][1]=='u')
		{
			user=argv[i]+2;
			isArgReaded++;
		}
		else if (argv[i][1]=='p')
		{
			pwd=argv[i]+2;
			isArgReaded++;
		}
		else if (argv[i][1]=='d')
		{
			dbname=argv[i]+2;
			isArgReaded++;
		}
		else if (argv[i][1]=='b')
		{
			port_str=argv[i]+2;
			port=atoi(port_str.c_str());
			isArgReaded++;
		}
		else if (argv[i][1]=='e')
		{
			sql=FetchArg(argv[i]+2,'\"');
			isArgReaded++;
			op='e';
		}
		else if (argv[i][1]=='i')
		{
			const char* ptr = argv[i];
			sql=FetchArg(ptr+2,'\"');
			isArgReaded++;
			op='i';
		}
		else if (argv[i][1]=='f')
		{
			import_id_filed=argv[i]+2;
			isArgReaded++;
		}
		else if (argv[i][1]=='t')
		{
			sql_table=argv[i]+2;
			isArgReaded++;
		}
		else if (argv[i][1]='c')
		{
			op='c';
			isArgReaded++;
		}
	}
	if (isArgReaded<4)
	{
		printf("arg missed.\r\n");
		printf("usage: -hhost -uuser -ppwd -ddbname -bport\r\n");
		printf("\tExport:-e\"sql\" filename\r\n");
		printf("\tImport:-i\"wheresql\" -ttable filename");
		getchar();
		return 0;
	}
	
	try
	{
		printf("mysql connection:%s:%d User:%s,Password:%s,DB:%s\r\n",host.c_str(),port,user.c_str(),pwd.c_str(),dbname.c_str());
		gs_conn = new CDBConnectionPool(host.c_str(),port,user.c_str(),pwd.c_str(),dbname.c_str());
		if ('e'==op)
		{
			printf("Begin export:%s\r\n",sql.c_str());
			Export(sql,file);
		}
		else if ('i'==op)
		{
			printf("Begin import:update %s where %s\r\n",sql_table.c_str(),sql.c_str());
			Import(sql,file,import_id_filed,sql_table);
		}
		else if ('c'==op)
		{
			ClearSaleDB();
		}
	}
	catch (mysqlpp::Exception& e)
	{
		printf("\r\nerror :%s",e.what());
		getchar();
	}
	catch(std::exception &e)
	{
		printf("\r\nerror :%s",e.what());
		getchar();
	}
	return 0;
}


string BuildValueSql(DataSingleRow& row,const vector<DataFieldInfo>& escapeField)
{
	string rtn;
	size_t col = row.size();
	for(size_t i=0;i<col;++i)
	{

		if(row[i].empty())
		{
			rtn += "NULL";
			if(i!=col-1)
				rtn += ",";

			continue;
		}

		if(escapeField[i].m_needEscape)
		{
			rtn += "x'";
			unsigned long len = row[i].length();
			char *hexStr = (char*)malloc(len*2+1);
			mysql_hex_string(hexStr,row[i].c_str(),len);
			rtn +=  hexStr;
			rtn += "'";
			free(hexStr);
		}
		else
			rtn += row[i];		

		if(i!=col-1)
			rtn += ",";

	}

	//rtn += ");";
	return rtn;
}


#include "mysqlpp/lib/ssqls.h"

sql_create_5(
	SaleDBInfo, 1, 5, 
	mysqlpp::sql_int,fSaleID,
	mysqlpp::sql_varchar,fSellerName,
	mysqlpp::sql_int,fPrice,
	mysqlpp::sql_int, fHours,
	mysqlpp::sql_bigint,fItemGUID
	);
#define Call_Sql(c) {printf("Begin Sql:%s\r\n",query.str().c_str());}c;{printf("end sql");}


typedef __int64 GUID_Item_Type;
void SendMail(const std::string &playerName, const std::string &title ,GUID_Item_Type guid);

void ClearSaleDB(void)
{
	AutoReleaseConnection dbCon(gs_conn);
	if (NULL == dbCon._connection)
	{
		printf("mysql connect failed!");
		return;
	}
	try
	{
		mysqlpp::Query query = dbCon._connection->query();
		
		std::vector<SaleDBInfo> temp;
		query << "select * from tsaledb";
		Call_Sql(query.storein(temp));
		
		query.reset();
		query << "delete from tsaledb";
		Call_Sql(mysqlpp::SimpleResult ret = query.execute());
		printf("delete record:%d\r\n",ret.rows());

		for (int index = 0; index < temp.size() ; ++index)
		{
			SendMail(temp[index].fSellerName, "寄售超时",temp[index].fItemGUID);
		}
	}
	catch (mysqlpp::Exception& e)
	{
		e.what();
	}
	catch(std::exception &e)
	{
		e.what();
	}
}

size_t SafeMemoryCopy(void* dest,size_t destLen,const void* src,size_t srcSize)
{
	size_t size=min(destLen,srcSize);
	memcpy(dest,src,size);
	return size;
}

#define PLAYER_NAME_LEN 20
#define TITLE_LEN 40
#define ATTACHMENT_DATA_LEN 400
#define CONTENT_LEN 1000

void SendMail(const std::string &playerName, const std::string &title ,GUID_Item_Type guid)
{
	std::string m_senderName("寄售服务器");
	AutoReleaseConnection dbcon(gs_conn);
	if(!dbcon._connection)	return;
	mysqlpp::Query query = dbcon._connection->query();
	int mailType=0;
	query.reset();
	query<<"insert into Mailbox values(Null,\'";
	query<<m_senderName;
	query<<"\',\'";
	query<<playerName;
	query<<"\',";
	query<<0;
	query<<",";
	query<<static_cast<unsigned short>(mailType);
	query<<",";
	query<<0;
	query<<",\'";
	query<<title;
	query<<"\',";
	query<<guid;
	query<<",now(),date_add(now(),interval ";
	int timeoutTime=30;
	query<<timeoutTime;
	mysqlpp::String strContent;
	strContent.assign(title.c_str(), title.size());
	query<<" day),\'";
	query<<strContent.c_str();
	query<<"\');";

	Call_Sql(mysqlpp::SimpleResult _res = query.execute());
	if (_res.rows()==0)
	{
		printf("Send mail back failed:%s\r\n",query.str().c_str());
		getchar();
	}
}
