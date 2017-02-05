#pragma once

#include "cache.h"
#include <windows.h>

#define N (13)

typedef unsigned int blocksP;
#define buddy_blocks(blocks) \
       ((blocksP *)(((buddy_s*)blocks)+1))

struct buddy_s {

	int numOfBlocks;
	int maxPower2;
//	blocksP blocks;
	void *myMemory;

	kmem_cache_s* cacheHead;
	kmem_cache_s* sizeNCaches[N];

	HANDLE mutexLock;
	HANDLE globalLock;
};

void buddy_init(void *space, int block_num);

void* blockByIndex(int);
int IndexOfBlock(void*);
void* buddy_alloc(int);
void buddy_dealloc(void *adr, int size_);
void destroyBuddy();
