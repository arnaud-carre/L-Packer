//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#include <assert.h>
#include <time.h>
#include "packEngine.h"
#include "lpacker.h"
#include "jobSystem.h"
#include "bootstrap.h"
#include "external/salvador/libsalvador.h"
#include "external/zopfli/deflate.h"
#include "external/lz4/lz4hc.h"

#define		D_HACKY_TEST		0

static const char* sAlgoName[kMaxAlgorithm] =
{
	"LZ4",
	"ZX0",
	"DEFLATE",
};

bool OutputBlob::packLz4()
{
	int outBufferSize = LZ4_compressBound(originalSize) + 1; // always 1 more in case of even size align
	packed = (uint8_t*)malloc(outBufferSize);
	if (packed)
	{
		int rc = LZ4_compress_HC((const char*)input, (char*)packed, originalSize, outBufferSize, LZ4HC_CLEVEL_MAX);
		if (rc > 0)
		{
			packedSize = int(rc);
			safeDist = 0;
			ok = true;
		}
		else
		{
			free(packed);
			packed = nullptr;
		}
	}
	return ok;
}

bool OutputBlob::packZx0()
{
	size_t outBufferSize = salvador_get_max_compressed_size(originalSize);
	packed = (uint8_t*)malloc(outBufferSize);
	if (packed)
	{
		salvador_stats stats;
		size_t rc = salvador_compress((const unsigned char*)input, (unsigned char*)packed, originalSize, outBufferSize, FLG_IS_INVERTED, 0, 0, nullptr, &stats);
		if (rc != -1)
		{
			packedSize = int(rc);
			safeDist = stats.safe_dist;
			ok = true;
		}
		else
		{
			free(packed);
			packed = nullptr;
		}
	}
	return ok;
}

bool OutputBlob::packDeflate()
{
	ZopfliOptions options;
	ZopfliFormat output_type = ZOPFLI_FORMAT_DEFLATE;

	//	extern bool gQuickMode;
	ZopfliInitOptions(&options);

	size_t packedSize64 = 0;
	packed = nullptr;
	ZopfliCompress(&options, output_type, (const unsigned char*)input, originalSize, &packed, &packedSize64);

	packedSize = int(packedSize64);
	return (packed != nullptr);
}


static bool packingJob(void* base, int index)
{
	OutputBlob& blob = ((OutputBlob*)base)[index];
	bool ret = false;

	clock_t t0 = clock();
	switch (blob.algo)
	{
		case kZx0:
			ret = blob.packZx0();
			break;
		case kDeflate:
			ret = blob.packDeflate();
			break;
		case kLz4:
			ret = blob.packLz4();
			break;
		default:
			assert(false);
			break;
	}

	if ( ret )
	{
		clock_t t1 = clock();
		float t = (float(t1 - t0) * 1000.f) / float(CLOCKS_PER_SEC);
		blob.timeMs = int(t);

		if (blob.verbose)
			printf("Job #%d: \"%s\" packed %d to %d bytes in %d ms\n", index, sAlgoName[blob.algo], blob.originalSize, blob.packedSize, blob.timeMs);

#if D_HACKY_TEST
		if (blob.algo == kZx0)
		{
			printf("Hacky test, repack ZX0 using deflate...\n");
			OutputBlob out;
			out.algo = kDeflate;
			out.input = blob.packed;
			out.originalSize = blob.packedSize;
			out.packed = nullptr;
			out.packedSize = 0;
			out.timeMs = 0;
			out.verbose = blob.verbose;

			out.packDeflate();

			printf("Repack from %d to %d bytes!\n", blob.packedSize, out.packedSize);

			free(out.packed);

		}
#endif

	}
	return ret;
}

int RoundTo(int size, int align)
{
	return (size + align -1) & (-align);
}

int ComputeFinalExeSize(Platform_e platform, const Bootstrap* bootstrap, const OutputBlob* packedChunks, int chunkCount, const Parameters& params)
{
	int size = 0;
	switch (platform)
	{
		case Platform_e::eAtari:
		{
			assert(1 == chunkCount);
			size += bootstrap->size + RoundTo(packedChunks[0].packedSize + params.text.GetSize(), 4);
			size += kAtariPrgAddSize;
			break;
		}
		case Platform_e::eAmiga:
			size += RoundTo(bootstrap->size + params.text.GetSize(),4);
			for (int c = 0; c < chunkCount; c++)
				size += 2 * 4 + RoundTo(packedChunks[c].packedSize, 4);
			size += kAmigaElfAddSize;
			break;
		case Platform_e::eData:
			assert(1 == chunkCount);
			size += bootstrap->size + packedChunks[0].packedSize;
			break;
		default:
			break;
	}
	return size;
}


int multiPackAndKeepBest(Platform_e platform, const InputBlob* inputArray, int count, OutputBlob* outputArray, const Parameters& params)
{

	OutputBlob* blobs = (OutputBlob*)malloc(count * kMaxAlgorithm * sizeof(OutputBlob));
	memset(blobs, 0, count * kMaxAlgorithm * sizeof(OutputBlob));
	int algoCount = 0;
	for (int a = 0; a < kMaxAlgorithm; a++)
	{
		if (params.algoMask & (1 << a))
		{
			for (int i = 0; i < count; i++)
			{
				OutputBlob& blob = *(blobs + algoCount*count + i);
				blob.algo = ePackingAlgorithm_t(a);
				blob.input = inputArray[i].data;
				blob.originalSize = inputArray[i].size;
				blob.verbose = params.verbose;
			}
			algoCount++;
		}
	}
	assert(algoCount > 0);
	int totalSize = 0;
	for (int i=0;i<count;i++)
		totalSize += inputArray[i].size;

	// multi-thread pack each block
	JobSystem js;
	printf("Testing %d packing algorithm(s) on %d bytes...\n", algoCount, totalSize);
	if (params.dataMode)
		printf("(DATA Mode, no executable)\n");
	js.RunJobs(blobs, algoCount*count, packingJob);
	if (algoCount*count != js.Complete())
	{
		printf("ERROR: some packing jobs failed\n");
		return -1;
	}

	int bestBlockIdx = -1;
	uint32_t bestSize = ~0;
	ePackingAlgorithm_t bestPackingAlgo = kLz4;

	for (int i=0;i<algoCount;i++)
	{
		const ePackingAlgorithm_t packAlgo = blobs[i * count].algo;
		const Bootstrap* bootstrap = GetRightBootstrap(platform, packAlgo, params.flash, params.IsText());
		if (nullptr == bootstrap)
			return -1;


		uint32_t total = ComputeFinalExeSize(platform, bootstrap, blobs + i * count, count, params);
		int totalTime = 0;
		for (int c = 0; c < count; c++)
			totalTime += blobs[i * count + c].timeMs;

//		if (params.verbose)
			printf("%7s packs to %d bytes (including %d header bytes) in %.02fsec\n", sAlgoName[packAlgo], total, bootstrap->size, float(totalTime) / 1000.f );

		if ((params.targetSize > 0) && (int(total) <= params.targetSize<<10))
		{
			bestBlockIdx = i;
			bestPackingAlgo = packAlgo;
			bestSize = total;
			break;
		}

		if ( total < bestSize )
		{
			bestBlockIdx = i;
			bestPackingAlgo = packAlgo;
			bestSize = total;
		}
	}

	if (params.targetSize > 0)
	{
		if ( int(bestSize) > params.targetSize<<10)
		{
			printf("ERROR: None of the %d packing algorithms succeed to reach %dKiB target\n", algoCount, params.targetSize);
			return -1;
		}
		else
		{
			assert(bestBlockIdx >= 0);
			printf("Best algorithm fiting in %dKiB target: %s\n", params.targetSize, sAlgoName[bestPackingAlgo]);
		}
	}
	else
	{
		assert(bestBlockIdx >= 0);
		printf("Best packing algorithm: %s\n", sAlgoName[bestPackingAlgo]);
	}

	// copy best results
	for (int i = 0; i < count; i++)
		outputArray[i] = blobs[bestBlockIdx * count + i];

	free(blobs);

	return bestSize;
}
