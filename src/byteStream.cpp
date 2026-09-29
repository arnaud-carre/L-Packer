//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "byteStream.h"

#define NUM_RELOC_CONTEXTS 256

ByteStream::ByteStream()
{
	m_data = nullptr;
	Release();
}

ByteStream::~ByteStream()
{
	Release();
}

void ByteStream::Release()
{
	free(m_data);
	m_pos = 0;
	m_capacity = 0;
	m_size = 0;
	m_data = nullptr;
}

void ByteStream::AddByte(uint8_t v)
{
	assert(m_pos == m_size);
	if (m_size >= m_capacity)
	{
		m_capacity += 64 * 1024;
		m_data = (uint8_t*)realloc(m_data, m_capacity);
	}
	m_pos++;
	m_data[m_size++] = v;
}

uint8_t ByteStream::ReadByte()
{
	assert(m_pos < m_size);
	if ( m_pos < m_size )
		return m_data[m_pos++];
	return 0xff;
}

void ByteStream::SeekForward(uint32_t seek)
{
	if ( m_pos + seek <= m_size )
		m_pos += seek;
	else
		m_pos = m_size;
}

void ByteStream::Seek(uint32_t seek)
{
	assert(seek <= m_size);
	m_pos = seek;
}

uint16_t ByteStream::ReadWord()
{
	uint16_t r = (uint16_t(ReadByte()) << 8);
	r |= ReadByte();
	return r;
}

uint32_t ByteStream::ReadLong()
{
	uint32_t r = (uint32_t(ReadWord()) << 16);
	r |=  ReadWord();
	return r;
}

void ByteStream::AddWord(uint16_t v)
{
	AddByte(uint8_t(v >> 8));
	AddByte(uint8_t(v & 0xff));
}

void ByteStream::AddLong(uint32_t v)
{
	AddWord(uint16_t(v >> 16));
	AddWord(uint16_t(v & 0xffff));
}

void ByteStream::Write(FILE* h)
{
	fwrite(m_data, 1, m_pos, h);
}

bool ByteStream::LoadFromFile(const char* fname)
{
	Release();
	bool ok = false;
	FILE* h = fopen(fname, "rb");
	if ( h )
	{
		fseek(h, 0, SEEK_END);
		m_capacity = ftell(h);
		m_size = m_capacity;
		m_pos = 0;
		m_data = (uint8_t *)malloc(m_capacity);
		fseek(h, 0, SEEK_SET);
		size_t ni = fread(m_data, 1, size_t(m_capacity), h);
		fclose(h);
		ok = (ni == m_capacity);
	}
	if ( !ok )
		printf("ERROR: Unable to load file \"%s\"\n", fname);
	return ok;
}

bool ByteStream::SaveToFile(const char* fname)
{
	bool ok = false;
	FILE* h = fopen(fname, "wb");
	if ( h )
	{
		size_t ni = fwrite(m_data, 1, size_t(m_size), h);
		fclose(h);
		ok = (ni == m_size);
	}
	if ( !ok )
		printf("ERROR: During writing file \"%s\"\n", fname);
	return ok;
}

const void* ByteStream::GetData(uint32_t readSize) const
{
	if (m_pos + readSize > m_size)
		return nullptr;
	return m_data + m_pos;
}

uint32_t ByteStream::GetLong(uint32_t offset) const
{
	assert(offset <= m_size - 4);
	assert(0 == (offset & 1));
	uint8_t* data = m_data + offset;
	uint32_t rv = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3] << 0);
	return rv;
}

uint16_t ByteStream::GetWord(uint32_t offset) const
{
	assert(offset <= m_size - 2);
	assert(0 == (offset & 1));
	uint8_t* data = m_data + offset;
	uint16_t rv = (data[0] << 8) | (data[1] << 0);
	return rv;
}

uint8_t ByteStream::GetByte(uint32_t offset) const
{
	assert(offset <= m_size - 1);
	return m_data[offset];
}

void ByteStream::SetWord(uint32_t offset, uint16_t value)
{
	assert(offset <= m_size - 2);
	assert(0 == (offset & 1));
	uint8_t* data = m_data + offset;
	data[0] = value >> 8;
	data[1] = value >> 0;
}

void ByteStream::SetLong(uint32_t offset, uint32_t value)
{
	SetWord(offset, uint16_t(value >> 16));
	SetWord(offset+2, uint16_t(value));
}


void ByteStream::Add32BE(uint32_t offset, uint32_t inc)
{
	SetLong(offset, GetLong(offset) + inc);
}

bool ByteStream::SearchAndPatchLongs(uint32_t offsetStart, uint32_t searchSize, uint32_t searchValue, uint32_t patchValue, bool all)
{
	if (searchSize < 4)
		return false;
	if (offsetStart + searchSize -4 > m_size)
		return false;

	int n = 0;
	for (uint32_t offset=offsetStart;offset<=offsetStart+searchSize-4;offset+=2)
	{
		if ( GetLong(offset) == searchValue )
		{
			SetLong(offset, patchValue);
			n++;
			if (!all)
				return true;
		}
	}
	if (0 == n)
	{
		printf("ERROR: SearchAndPatchLongs fail, corrupted LPacker?\n");
		return false;
	}
	return true;
}

bool ByteStream::SearchAndPatchWords(uint32_t offsetStart, uint32_t searchSize, uint16_t searchValue, uint16_t patchValue, bool all)
{
	if (searchSize < 2)
		return false;
	if (offsetStart + searchSize -2 > m_size)
		return false;

	int n = 0;
	for (uint32_t offset=offsetStart;offset<=offsetStart+searchSize-2;offset+=2)
	{
		if ( GetWord(offset) == searchValue )
		{
			SetWord(offset, patchValue);
			n++;
			if (!all)
				return true;
		}
	}
	if (0 == n)
	{
		printf("ERROR: SearchAndPatchWords fail, corrupted LPacker?\n");
		return false;
	}
	return true;
}

void ByteStream::Complete(uint32_t size, int align)
{
	while (size & (align - 1))
	{
		AddByte(rand());
		size++;
	}
}

bool ByteStream::AddData(const void* data, uint32_t size)
{
	bool ok = false;
	assert(m_pos == m_size);
	if ( data )
	{
		m_capacity = m_size + size;
		m_data = (uint8_t *)realloc(m_data,m_capacity);
		m_size = m_capacity;
		memcpy(m_data+m_pos, data, size);
		m_pos = m_size;
		ok = true;
	}
	return ok;
}

void ByteStream::Align(int align)
{
	assert(m_pos == m_size);
	while (m_size % align)
		AddByte(rand());
}

bool ByteStream::AddStreamSlice(const ByteStream& in, uint32_t readSize)
{
	bool ok = false;
	const void* indata = in.GetData(readSize);
	if (indata)
		ok = AddData(indata, readSize);
	return ok;
}

bool ByteStream::SetStream(const ByteStream& in)
{
	Release();
	return AddData(in.GetRawArray(), in.GetSize());
}

bool ByteStream::AddStream(const ByteStream& in)
{
	uint32_t readSize = in.GetSize();
	return AddStreamSlice(in, readSize);
}

bool ByteStream::IsAmigaExecutable() const
{
	if ( m_size >= 4 )
		return (0x3f3 == GetLong(0));
	return false;
}

bool ByteStream::IsAtariExecutable() const
{
	if (m_size >= 0x1c)
	{
		if (0x601a == GetWord(0))
		{
			uint32_t endOffset = 0x1c;
			endOffset += GetLong(2);	// text section
			endOffset += GetLong(6);	// data section
			endOffset += GetLong(14);	// symbol table size

			if (endOffset <= m_size)
				return true;
		}
	}
	return false;
}

