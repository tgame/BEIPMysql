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
			vvalue=row[i];
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
		fread(buf,stream.Size(),1,fp);
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
	
	AutoReleaseConnection acon(gs_conn);
	mysqlpp::Connection* con = acon._connection;
	if (!con)
	{
		printf("sql connect failed!!");
		getchar();
		return;
	}
	string filedList;
	data.AppendFieldString(filedList);
	string values;
	for (unsigned int i=0;i<data.m_rows.size();i++)
	{
		DataSingleRow& dataRow=data.m_rows[i];
		mysqlpp::Query query = con->query();
		query<<"update "<<table<<" set (";
		query<<filedList<<") values(";
		values=BuildValueSql(dataRow,data.m_rowInfo);
		query<<values<<") where "<<sql<<";";
		mysqlpp::SimpleResult ret = query.execute();
		if (ret.rows() == 0)
		{
			printf("sql exec failed..is the where sql error:\r\n");
			printf("update %s where %s",table.c_str(),sql.c_str());
			getchar();
			return;
		}
	}
}

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
	}
	if (isArgReaded<7)
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
	}
	catch (exception e)
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
