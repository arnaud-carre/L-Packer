//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#pragma once
#include <stdint.h>
#include <stdio.h>

class ByteStream
{
public:
	ByteStream();
	~ByteStream();

	bool LoadFromFile(const char* fname);
	bool SaveToFile(const char* fname);
	bool eof() const { return m_pos >= m_size; }

	bool IsAmigaExecutable() const;
	bool IsAtariExecutable() const;

	uint32_t GetSize() const { return m_size; }
	uint32_t GetPos() const { return m_pos; }

	bool AddData(const void* data, uint32_t size);
	bool AddStream(const ByteStream& in);
	bool SetStream(const ByteStream& in);
	bool AddStreamSlice(const ByteStream& in, uint32_t readSize);
	void Add32BE(uint32_t offset, uint32_t inc);

	bool PackZx0();
	bool PackDeflate();

	uint32_t GetLong(uint32_t offset) const;
	uint16_t GetWord(uint32_t offset) const;
	uint8_t GetByte(uint32_t offset) const;

	void SetLong(uint32_t offset, uint32_t value);
	void SetWord(uint32_t offset, uint16_t value);
	bool SearchAndPatchLongs(uint32_t offsetStart, uint32_t searchSize, uint32_t searchValue, uint32_t patchValue, bool all = false);
	bool SearchAndPatchWords(uint32_t offsetStart, uint32_t searchSize, uint16_t searchValue, uint16_t patchValue, bool all = false);

	void Align(int align);
	uint8_t ReadByte();
	uint16_t ReadWord();
	uint32_t ReadLong();
	void SeekForward(uint32_t seek);
	void Seek(uint32_t seek);

	void AddByte(uint8_t v);
	void AddWord(uint16_t v);
	void AddLong(uint32_t v);
	void Complete(uint32_t size, int align);

	void Write(FILE* h);
	const uint8_t* GetRawArray() const { return m_data; }
	const void* GetData(uint32_t readSize) const;

private:
	void Release();

	uint8_t* m_data;
	uint32_t m_pos;
	uint32_t m_size;
	uint32_t m_capacity;
};
