#if defined (_MSC_VER) && (_MSC_VER >= 1000)
#pragma once
#endif
#ifndef _HEADER_SERIALSTREAM
#define _HEADER_SERIALSTREAM
//#define	_WD_STRING_SUPPORT
#ifdef	_WD_STRING_SUPPORT
#include "AG_String.h"
#endif
//!字节序
#if defined(__i386) || defined(_M_IX86) || defined (__x86_64)
#   define SERIAL_LITTLE_ENDIAN
#elif defined(__sparc) || defined(__sparc__) || defined(__hppa) || defined(__ppc__) || defined(_ARCH_COM)
#   define SERIAL_BIG_ENDIAN
#else
#   error "Unknown architecture"
#endif
#include <vector>
#define SS_START_SERIALIZE(Class) friend class SerialHelper; \
class SerialHelper \
							{ \
							typedef Class SerialClass; \
							public:
#define SS_START_READ()		template <class T> static void Read(T& stream,SerialClass & c) \
								{
#define _SS_READ(a)					stream.Read(c.a);
#define SS_END_READ				}
#define SS_START_WRITE()	template <class T> static void Write(T& stream,const SerialClass & c) \
								{
#define _SS_WRITE(a)				stream.Write(c.a);
#define SS_END_WRITE			}
#define SS_END_SERIALIZE	};
#define _SS_START_(Class)	SS_START_SERIALIZE(Class) \
	SS_START_READ()
#define _SS_SEP_()				SS_END_READ \
	SS_START_WRITE()
#define _SS_END_()				SS_END_WRITE \
	SS_END_SERIALIZE
#define _SS_WRITE_LOCAL(a)			stream.Write(a);
#define _SS_READ_LOCAL(a)			stream.Read(a);
#define _SS_WRITE_BUF(a)			stream.WriteBuffer(&c.a,sizeof(c.a));
#define _SS_READ_BUF(a)			stream.ReadBuffer(&c.a,sizeof(c.a));

#define _SS_Read_Call_(ClassType)	ClassType::SerialHelper::Read(stream,c)
#define _SS_Write_Call_(ClassType)	ClassType::SerialHelper::Write(stream,c)
/*
#define _SS_HEADER_DECLARE_(Class)	friend class SerialHelper; \
class SerialHelper \
{ \
public: \
typedef Class SerialClass;
static void Read(SerialStream& stream,SerialClass & c); \
static void Write(SerialStream& stream,const SerialClass & c); \
};
#define _SS_CPP_START_(Class)	void Class::SerialHelper Read(SerialStream& stream,Class::SerialHelper::SerialClass & c) \
{ \
#define _SS_CPP_SEP_(Class)		} \
void Class::SerialHelper Write(SerialStream& stream,const Class::SerialHelper::SerialClass & c) \
{ 
#define _SS_CPP_END_()			}
*/
#include <list>
#include <map>

/**
*序列化流	
*/
class SerialStream
{
public:
	typedef unsigned char Byte;
	typedef short	Short;
	typedef unsigned short UShort;
	typedef unsigned int   UInt;
	typedef int		Int;
	typedef float	Float;
	typedef bool	Bool;
	typedef __int64 Int64;
	typedef unsigned __int64 UInt64;
#ifdef _USE_64_LONG
	typedef __int64	iLong;
#else
	typedef long	Long;
#endif
	typedef unsigned long ULong;

	typedef std::vector<Byte> Container;
	Container _buffer;
	Container::iterator i;
protected:
	struct ChunkHeader 
	{
		unsigned long m_chunkHeaderPosition;
		unsigned long m_chunkSize;
	};
	typedef std::vector<ChunkHeader>	ChunkStack;
	ChunkStack m_chunkStack;
public:
	//!获得当前游标(指下一个Write会写入的游标),这个游标会是安全的.可以保留一段时间
	Container::size_type GetCurrentSafePos(void)
	{
		return _buffer.size();
	}
	//!返回游标指定的内存指针
	template<class T>
	T* GetPtrBySafePos(Container::size_type pos)
	{
		if (pos < _buffer.size())
		{
			return (T*)&_buffer[pos];
		}
		return 0;
	}
	//!写整型
	void WriteByPos(const ULong& v,Container::size_type pos)
	{
		/*Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(ULong));*/
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(ULong) - 1;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	template<typename T>
	void ChunkWrite(const T& value)
	{
		BeginChunk();
		Write(value);
		EndChunk();
	}
	template<typename T>
	void ChunkRead(T& value)
	{
		BeginChunkRead();
		Read(value);
		EndChunkRead();
	}
	void BeginChunk(void)
	{
		m_chunkStack.resize(m_chunkStack.size()+1);
		ChunkHeader& chunk = m_chunkStack.back();
		chunk.m_chunkHeaderPosition = static_cast<unsigned long>(_buffer.size());
		chunk.m_chunkSize = 0;
		Write(chunk.m_chunkSize);
	}
	void EndChunk(void)
	{
		if (!m_chunkStack.empty())
		{
			ChunkHeader& chunk = m_chunkStack.back();
			chunk.m_chunkSize = static_cast<unsigned long>(_buffer.size());
			chunk.m_chunkSize -= chunk.m_chunkHeaderPosition;
			WriteByPos(chunk.m_chunkSize,chunk.m_chunkHeaderPosition);
			m_chunkStack.erase(m_chunkStack.begin()+(m_chunkStack.size()-1));
		}
		else
		{
			//:fix me
		}
	}
	unsigned long BeginChunkRead(void)
	{
		m_chunkStack.resize(m_chunkStack.size()+1);
		ChunkHeader& chunk = m_chunkStack.back();
		chunk.m_chunkHeaderPosition = static_cast<unsigned long>(i - _buffer.begin());
		chunk.m_chunkSize = 0;
		Read(chunk.m_chunkSize);
		return chunk.m_chunkSize-sizeof(unsigned long);
	}
	void EndChunkRead(void)
	{
		if (!m_chunkStack.empty())
		{
			ChunkHeader& chunk = m_chunkStack.back();
			Container::size_type currentPosition = (i - _buffer.begin());
			Container::size_type currentReadSize = currentPosition - chunk.m_chunkHeaderPosition;
			if (currentReadSize < chunk.m_chunkSize)
			{
				SeekMoveRead(chunk.m_chunkSize - currentReadSize);
			}
			m_chunkStack.erase(m_chunkStack.begin()+(m_chunkStack.size()-1));
		}
		else
		{
			//:fix me
		}
	}
	SerialStream(Container::size_type reserveSize)
	{
		_buffer.reserve(reserveSize);
	}
	~SerialStream(void)
	{
	}
	void Resize(Container::size_type size)
	{
		_buffer.resize(size);
		i = _buffer.begin();
	}
	SerialStream(SerialStream& stream)
	{
		_buffer.assign(stream._buffer.begin(),stream._buffer.end());
		i=_buffer.begin();
	}
	SerialStream(char* buffer,Container::size_type size)
	{
		_buffer.resize(size);
		memcpy(&_buffer[0],buffer,size);
		i=_buffer.begin();
	}
	SerialStream(Byte* buffer,Container::size_type size)
	{
		_buffer.resize(size);
		memcpy(&_buffer[0],buffer,size);
		i=_buffer.begin();
	}
	void WriteStream(const SerialStream& stream)
	{
		Container::size_type pos=_buffer.size();
		_buffer.resize(stream._buffer.size()+pos);
		memcpy(&_buffer[pos],&stream._buffer[0],stream._buffer.size());
	}
	void WriteBuffer(const void* data,int size)
	{
		Container::size_type pos=_buffer.size();
		_buffer.resize(size+pos);
		memcpy(&_buffer[pos],data,size);
	}
	void ReadBuffer(void* data,int size)
	{
		void* buf = &(*i);
		i += size;
		memcpy(data,buf,size);
	}
	operator Byte* ()
	{
		return &(_buffer[0]);
	}
	operator char* ()
	{
		return reinterpret_cast <char*>(&_buffer[0]);
	}
	void* GetRawBuffer(void)
	{
		return static_cast<void*>(&_buffer[0]);
	}
	size_t Size(void)
	{
		return _buffer.size();
	}
	void Clear(void)
	{
		_buffer.clear();
		i=_buffer.begin();
	}
	void Rewind(void)
	{
		i = _buffer.begin();
	}
	void Assign(char* buf,int size)
	{
		Clear();
		WriteBuffer(buf,size);
		Rewind();
	}
	void SeekMoveRead(int offset)
	{
		i += offset;
	}
	
//	Container::size_type _shortClipStartPos;
//	//!开始一个short长度的分片,在结束时,将自动填充分片的长度
//	void BeginWriteShortClip(void)
//	{
//		_shortClipStartPos = _buffer.size();
//		short clipLength = 0;
//		Write(clipLength);
//	}
//	//!结束一个short长度的分片,写入分片的长度到BeginWriteShortClip时记录的位置
//	unsigned short EndWriteShortClip(void)
//	{
//		Container::size_type endPos = _buffer.size();
//		unsigned short v = endPos - _shortClipStartPos - sizeof(unsigned short);
//		Byte* dest = &_buffer[_shortClipStartPos];
//#ifdef SERIAL_BIG_ENDIAN
//		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(Short) - 1;
//		*dest++ = *src--;
//		*dest = *src;
//#else
//		const Byte* src = reinterpret_cast<const Byte*>(&v);
//		*dest++ = *src++;
//		*dest = *src;
//#endif
//		return v;
//	}
	void copyTo(void* to,size_t size)
	{
		memcpy(to,&(_buffer[0]),_buffer.size());
	}
	//!读整型
	void Read(Int& v)
	{
		if(_buffer.end() - i < static_cast<int>(sizeof(Int)))
		{//Error
		}
		const Byte* src = &(*i);
		i += sizeof(Int);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(Int) - 1;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!读整型
	void Read(Int64& v)
	{
		if(_buffer.end() - i < static_cast<Int64>(sizeof(Int64)))
		{//Error
		}
		const Byte* src = &(*i);
		i += sizeof(UInt64);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(Int64) - 1;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	void Read(UInt64& v)
	{
		if(_buffer.end() - i < static_cast<UInt64>(sizeof(UInt64)))
		{//Error
		}
		const Byte* src = &(*i);
		i += sizeof(UInt64);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(UInt64) - 1;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!读整型
	void Read(UInt& v)
	{
		if(_buffer.end() - i < static_cast<int>(sizeof(UInt)))
		{//Error
		}
		const Byte* src = &(*i);
		i += sizeof(UInt);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(UInt) - 1;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!读整型
	void Read(ULong& v)
	{
		if(_buffer.end() - i < static_cast<int>(sizeof(ULong)))
		{//Error
		}
		const Byte* src = &(*i);
		i += sizeof(UInt);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(ULong) - 1;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}

#ifdef _USE_64_LONG
	//!读整型
	void Read(iLong& v)
	{
		if(_buffer.end() - i < static_cast<int>(sizeof(iLong)))
		{//Error
		}
		const Byte* src = &(*i);
		i += sizeof(iLong);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(iLong) - 1;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
#endif
	//!读字节型
	void Read(Byte& v)
	{
		if(i >= _buffer.end())
		{//Error
		}
		v = *i++;
	}
	void Read(char& v)
	{
		if(i >= _buffer.end())
		{//Error
		}
		v = *i++;
	}
	//!读字节型
	void Read(Bool& v)
	{
		if(i >= _buffer.end())
		{//Error
		}
		v = *i++ ? true : false;
	}
	//!读浮点型
	void Read(Float& v)
	{
		if(_buffer.end() - i < static_cast<int>(sizeof(Float)))
		{//Error
		}
		const Byte* src = &(*i);
		i += sizeof(Float);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(Float) - 1;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!读16位整型
	void Read(Short& v)
	{
		if(_buffer.end() - i < static_cast<int>(sizeof(Short)))
		{//Error
		}
		const Byte* src = &(*i);
		i += sizeof(Short);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(Short) - 1;
		*dest-- = *src++;
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!读16位整型
	void Read(UShort& v)
	{
		if(_buffer.end() - i < static_cast<int>(sizeof(UShort)))
		{//Error
		}
		const Byte* src = &(*i);
		i += sizeof(UShort);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(UShort) - 1;
		*dest-- = *src++;
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!读长整型
	void Read(Long& v)
	{
		if(_buffer.end() - i < static_cast<int>(sizeof(Long)))
		{//Error

		}
		const Byte* src = &(*i);
		i += sizeof(Long);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(Long) - 1;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
#ifdef _USE_64_LONG
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
#endif
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
#ifdef _USE_64_LONG
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
#endif
		*dest = *src;
#endif
	}

	//!读长整型
	void Read(double& v)
	{
		if(_buffer.end() - i < static_cast<int>(sizeof(double)))
		{//Error

		}
		const Byte* src = &(*i);
		i += sizeof(double);
#ifdef SERIAL_BIG_ENDIAN
		Byte* dest = reinterpret_cast<Byte*>(&v) + sizeof(double) - 1;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest-- = *src++;
		*dest = *src;
#else
		Byte* dest = reinterpret_cast<Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!读串
	void Read(std::string& v)
	{
		Int len;
		readSize(len);
		if(_buffer.end() - i < len)
		{//Error
		}
		if(len > 0)
		{
			v.assign(reinterpret_cast<const char*>(&(*i)), len);
			i += len;
		}
		else
		{
			v.clear();
		}
	}
	void Read(std::wstring& v)
	{
		//std::wstring::value_type c = 0;
		v.clear();
		short c = 0;
		Read(c);
		while(c)
		{
			v.push_back(c);
			Read(c);
		}
		//v.push_back(c);
	}
#ifdef _WD_STRING_SUPPORT
	//!读串
	void Read(AG_String& v)
	{
		Int len;
		readSize(len);
		if(_buffer.end() - i < len)
		{//Error
		}
		if(len > 0)
		{
			//v.assign(reinterpret_cast<const char*>(&(*i)), len);
			v.Assign(reinterpret_cast<const char*>(&(*i)), len);
			i += len;
		}
		else
		{
			//v.clear();
		}
	}
#endif
	//!写整型
	void Write(const Int& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(Int));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(Int) - 1;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!写整型
	void Write(const Int64& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(Int64));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(Int64) - 1;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!写整型
	void Write(const UInt64& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(UInt64));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(UInt64) - 1;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!写整型
	void Write(const ULong& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(ULong));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(ULong) - 1;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!写整型
	void Write(const UInt& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(UInt));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(UInt) - 1;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
#ifdef _USE_64_LONG
	//!写整型
	void Write(const iLong& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(iLong));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(iLong) - 1;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
#endif
	//!写字节型
	void Write(const Byte& v)
	{
		_buffer.push_back(v);
	}
	void Write(const char& v)
	{
		_buffer.push_back(v);
	}
	//!写字节型
	void Write(const Bool& v)
	{
		_buffer.push_back(v);
	}
	//!写浮点型
	void Write(const Float& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(Float));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(Float) - 1;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!写16位整型
	void Write(const Short& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(Short));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(Short) - 1;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest = *src;
#endif
	}
	//!写16位整型
	void Write(const UShort& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(UShort));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(UShort) - 1;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest = *src;
#endif
	}

	//!写长整型
	void Write(const Long& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(Long));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(Long) - 1;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
#ifdef _USE_64_LONG
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
#endif
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
#ifdef _USE_64_LONG
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
#endif
		*dest = *src;
#endif
	}

	//!写长整型
	void Write(const double& v)
	{
		Container::size_type pos = _buffer.size();
		_buffer.resize(pos + sizeof(double));
		Byte* dest = &_buffer[pos];
#ifdef SERIAL_BIG_ENDIAN
		const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(double) - 1;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest++ = *src--;
		*dest = *src;
#else
		const Byte* src = reinterpret_cast<const Byte*>(&v);
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest = *src;
#endif
	}


	//!写串
	void Write(const std::string& v)
	{
		Int len = static_cast<Int>(v.size());
		writeSize(len);
		if(len > 0)
		{
			Container::size_type pos = _buffer.size();
			_buffer.resize(pos + len);
			memcpy(&_buffer[pos], v.c_str(), len);
		}
	}
	void Write(const std::wstring& v)
	{
		Int len = static_cast<Int>(v.size());
		//std::wstring::value_type c = 0;
		short c = 0;
		for (int i=0;i<len;i++)
		{
			c = v[i];
			if (!c)
			{
				break;
			}
			Write(c);
		}
		c = 0;
		Write(c);
	}
#ifdef _WD_STRING_SUPPORT
	void Write(const AG_String& v)
	{
		Int len = static_cast<Int>(v.Size());
		writeSize(len);
		if(len > 0)
		{
			Container::size_type pos = _buffer.size();
			_buffer.resize(pos + len);
			memcpy(&_buffer[pos], v.c_str(), len);
		}
	}
#endif
	//!读将值读到2个迭代器(Iterator)指定范围内
	template<class Iterator>
		void Read(Iterator start,Iterator end)
	{
		for(;start != end;start++)
		{
			Read(*start);
		}
	}
	//!将2个迭代器指定范围内的值写入
	template<class Iterator>
		void Write(Iterator start,Iterator end)
	{
		for(;start != end;start++)
		{
			Write(*start);
		}
	}
	//!读取vector
	template<class VT>
		void Read(std::vector<VT > & vec)
	{
		std::vector<VT >::size_type size = 0;
		Read(size);
		vec.resize(size);
		for(std::vector<VT >::size_type i = 0;i < size;i++)
		{
			Read(vec[i]);
		}
	}
	//!写入vector
	template<class VT>
		void Write(const std::vector<VT > & vec)
	{
		std::vector<VT >::size_type size = vec.size();
		Write(size);
		for(std::vector<VT >::size_type i = 0;i < size;i++)
		{
			Write(vec[i]);
		}
	}
	//!写入vector，带指针
	template<class VT>
		void Write(const std::vector<VT* > & vec)
	{
		std::vector<VT* >::size_type size = vec.size();
		Write(size);
		for(std::vector<VT >::size_type i = 0;i < size;i++)
		{
			Write(static_cast<const VT*>(vec[i]));
		}
	}
	//!读取vector，带指针
	template<class VT,class PCtor>
		void Read(std::vector<VT* > & vec,PCtor pctor)
	{
		std::vector<VT* >::size_type size = 0;
		Read(size);
		vec.resize(size);
		for(std::vector<VT >::size_type i = 0;i < size;i++)
		{
			vec[i] = pctor();
			Read(static_cast<VT*>(vec[i]));
		}
	}
	//!写入list
	template<class VT>
		void Write(const std::list<VT > & l)
	{
		std::list<VT >::size_type size = l.size();
		Write(size);
		for(std::list<VT >::const_iterator it = l.begin();it != l.end();++it)
		{
			Write(*it);
		}
	}
	//!读取list
	template<class VT>
		void Read(std::list<VT > & l)
	{
		std::list<VT >::size_type size = 0;
		Read(size);
		l.resize(size);
		for(std::list<VT >::iterator it = l.begin();it != l.end();++it)
		{
			Read(*it);
		}
	}
	//!写入list，带指针
	template<class VT>
		void Write(const std::list<VT* > & l)
	{
		std::list<VT* >::size_type size = l.size();
		Write(size);
		for(std::list<VT* >::const_iterator it = l.begin();it != l.end();++it)
		{
			Write(static_cast<const VT*>(*it));
		}
	}
	//!读取list，带指针
	template<class VT,class PCtor>
		void Read(std::list<VT* > & l,PCtor pctor)
	{
		std::list<VT* >::size_type size = 0;
		Read(size);
		l.resize(size);
		for(std::list<VT* >::iterator it = l.begin();it != l.end();++it)
		{
			VT* ptr = pctor();
			Read(static_cast<VT*>(ptr));
			*it = ptr;
		}
	}
	//!写入map
	template<class VTX,class VTY>
		void Write(const std::map<VTX,VTY > & m)
	{
		std::map<VTX,VTY >::size_type size = m.size();
		Write(size);
		for(std::map<VTX,VTY >::const_iterator it = m.begin();it != m.end();++it)
		{
			Write(it->first);
			Write(it->second);
		}
	}
    //!写入map
    template<class VTX,class VTY>
    void Read(const std::map<VTX,VTY > & m)
    {
        m.clear();
        std::map<VTX,VTY >::size_type size = m.size();
        Read(size);
        for(std::map<VTX,VTY >::size_type i = 0;i < size;i++)
        {

            std::pair<VTX,VTY > p;
            Read(p.first);
            Read(p.second);
            m.insert(p);
        }
    }
	//!读取map
	template<class VTX,class VTY,class VTPCtor>
		void Read(std::map<VTX,VTY > & m,VTPCtor pairCtor)
	{
        m.clear();
		std::map<VTX,VTY >::size_type size = 0;
		Read(size);
		for(std::map<VTX,VTY >::size_type i = 0;i < size;i++)
		{
			std::pair<VTX,VTY > p = pairCtor();
			Read(p.first);
			Read(p.second);
			m.insert(p);
		}
	}
	//!写入map第2个参数带指针
	template<class VTX,class VTY>
		void Write(const std::map<VTX,VTY* > & m)
	{
		std::map<VTX,VTY* >::size_type size = m.size();
		Write(size);
		for(std::map<VTX,VTY* >::const_iterator it = m.begin();it != m.end();++it)
		{
			Write(it->first);
			Write(static_cast<const VTY*>(it->second));
		}
	}
	//!读取map第2个参数带指针
	template<class VTX,class VTY,class VTPCtor>
		void Read(std::map<VTX,VTY* > & m,VTPCtor pairCtor)
	{
        m.clear();
		std::map<VTX,VTY* >::size_type size = 0;
		Read(size);
		for(std::map<VTX,VTY* >::size_type i = 0;i < size;i++)
		{
			std::pair<VTX,VTY* > p = pairCtor();
			Read(p.first);
			Read(static_cast<VTY*>(p.second));
			m.insert(p);
		}
	}
    //!读取map第2个参数带指针
    template<class VTX,class VTY>
    void Read(std::map<VTX,VTY* >&m)
    {
        m.clear();
        std::map<VTX,VTY* >::size_type size = 0;
        Read(size);
        for(std::map<VTX,VTY* >::size_type i = 0;i < size;i++)
        {
            std::pair<VTX,VTY* > p;
            Read(p.first);
            p.second = new VTY;
            Read(static_cast<VTY*>(p.second));
            m.insert(p);
        }
    }
    //!读取map第2个参数带指针
    template<class VTX,class VTY>
    void Read(std::map<VTX,VTY >&m)
    {
        m.clear();
        std::map<VTX,VTY>::size_type size = 0;
        Read(size);
        for(std::map<VTX,VTY>::size_type i = 0;i < size;i++)
        {
            std::pair<VTX,VTY> p;
            Read(p.first);
            Read(p.second);
            m.insert(p);
        }
    }
	template<typename P>
		void Write(const P* ptr)
	{
		Write(*ptr);
	}
	template<typename P>
		void Read(P* ptr)
	{
		Read(*ptr);
	}
	//!类T的序列化,类T必须包含有SerialHelper嵌套类定义,其中包含方法<p>void Read(SerialStream&)跟write
	template<class T>
		void Read(T& v)
	{
		T::SerialHelper::Read(*this,v);
	}
	template<class T>
		void Write(const T& v)
	{
		T::SerialHelper::Write(*this,v);
	}
	void readSize(Int& v)
	{
		Byte s=0;
		Read(s);
		if (s == 255) 
		{
			Read(v);
		}
		else
		{
			v=s;
		}
	}
	void writeSize(Int& v)
	{
		if (v < 255) 
		{
			Byte s=v;
			Write(s);
		}
		else
		{
			Write(static_cast<Byte>(255));
			Write(v);
		}
	}
};
#endif

