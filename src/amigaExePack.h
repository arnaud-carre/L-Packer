//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#pragma once
#include <stdint.h>
#include "byteStream.h"
#include "relocList.h"

static const uint32_t kMaxHunk = 64;

enum eMemoryType
{
	eChipMemory,
	eAnyMemory
};

enum eHunkType
{
	eHunkUnknown,
	eHunkCode,
	eHunkData,
	eHunkBss
};

struct	Hunk
{
	Hunk()
	{
		m_relocListCount = 0;
		m_allocSize = 0;
		m_dataSize = 0;
		m_data = nullptr;
		m_hunkType = eHunkUnknown;
		m_totalRelocCount = 0;
	};

	void Release()
	{
		m_relocListCount = 0;
		free(m_data);
		m_data = nullptr;
	};

	uint32_t m_id;
	eMemoryType m_memType;
	eHunkType m_hunkType;
	uint32_t m_allocSize;
	uint32_t m_dataSize;
	uint8_t* m_data;
	int m_totalRelocCount;
	int m_relocListCount;
	RelocList m_relocLists[kMaxHunk];
};


class AmigaExePack
{
public:
	AmigaExePack();
	~AmigaExePack();

	bool Load(ByteStream& binFile, bool verbose);
	bool MergeHunks(Hunk& out, eMemoryType memType);

	uint32_t GetHunkCount() const { return m_hunkCount; }
	const Hunk& GetHunk(uint32_t hunkId) const;

	void Print();

private:
	void	Release();
	bool Parse(ByteStream& bs, bool verbose);

	uint32_t m_hunkCount;
	Hunk	m_hunks[kMaxHunk];

};

