#include "meow_intrinsics.h"
#include "meow_hash.h"

static u64 globalHashSeed = 0;

static u32 HashMemory(void *buffer, u32 bufferSize)
{
	meow_hash hash = MeowHash_Accelerated(globalHashSeed, bufferSize, buffer);
	u32 result = MeowU32From(hash, 0);
	return(result);
}

#define HashString(buffer, size) (HashMemory((void *)buffer, (u32)size))

#define HashStringInPlace(string) (HashMemory((void *)string, sizeof(string)-1))
