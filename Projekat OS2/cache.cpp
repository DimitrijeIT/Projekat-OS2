#include "cache.h"
#include "slab.h"
#include "buddy.h"
#include "slab_header.h"
#include "power2.h"
#include <iostream>

#define MAX_SIZE 17
#define MIN_SIZE 5

extern buddy_s* BUDDY;

kmem_cache_t *cache_create(const char *name, size_t size,
	void(*ctor)(void *),
	void(*dtor)(void *)) {

	int s = sizeof(kmem_cache_s) / BLOCK_SIZE;
	int numOfBlocksForChacheStruct = sizeof(kmem_cache_s) % BLOCK_SIZE == 0 ? s : s + 1;

	//Calculate number of object per slab
	size_t availbale_space = BLOCK_SIZE - sizeof(slab_header) - sizeof(buffer);
	//sizeof(buffer) we will have at list one object in slab

	int leftOver = 0; //Space that is unused
	int blocksForSlab = 1; //At list one is needed 
	int obj_in_slab = 1; //At list one object per slab
	if (size < availbale_space) {
		//At list one object can fit in one BLOCK 
		while (obj_in_slab * sizeof(buffer) + obj_in_slab*size < availbale_space) {
			//obj_in_slab * sizeof(buffer) - for list of object in slab 
			//obj_in_slab*size - space for objects 
			obj_in_slab++;
		}
		obj_in_slab--;		//Off by one 
		leftOver = availbale_space - (obj_in_slab* size + (obj_in_slab - 1) * sizeof(buffer)); 
		// -1 - already allocted space for one element in buffer in availbale_space
	}
	else {
		//Obj to big to fit in one block 
		while (size > availbale_space)
		{
			availbale_space += BLOCK_SIZE;
			blocksForSlab++;
		}
		leftOver = availbale_space - size;
	}

	//Allock one block for data 
	//void * mem = buddy_alloc(numOfBlocksForChacheStruct * BLOCK_SIZE + BLOCK_SIZE);

	//Allocate space for cache structure
	void * mem = buddy_alloc(numOfBlocksForChacheStruct * BLOCK_SIZE);
	kmem_cache_s* cache_s = (kmem_cache_s*)mem;

	//Init struct

	//Add to list of caches
	cache_s->priv = nullptr;
	cache_s->next = BUDDY->cacheHead;
	if (cache_s->next)cache_s->next->priv = cache_s;
	BUDDY->cacheHead = cache_s;

	//Init list of slabs 
	cache_s->slabs_partial = nullptr;
	cache_s->slab_free = nullptr;
	cache_s->slab_full = nullptr;

	cache_s->objectSize = size;
	cache_s->ctor = ctor;
	cache_s->dtor = dtor;

	cache_s->numObjInSlot = obj_in_slab;
	cache_s->blockForSlab = blocksForSlab;
	cache_s->nextOff = 0;
	cache_s->offset = leftOver;

	cache_s->mutexLock = CreateMutex(NULL, false, NULL);

	//Statistics
	cache_s->numSlabs = 0;
	cache_s->numTotalAloc = 0;
	cache_s->inUse = 0;
	cache_s->percentage = 0;
	cache_s->sizeInBlocks = numOfBlocksForChacheStruct;

	cache_s->growing = false;
	cache_s->shrinked = false;
	cache_s->error_code = NoErros;

	strncpy_s(cache_s->name, name, NAME_LEN);
	return cache_s;
}

void *cache_alloc(kmem_cache_t *cachep) {
	if (cachep == nullptr) return nullptr;

	//Is there any partial full slabs
	if (cachep->slabs_partial != nullptr) {
		return slab_alloc(cachep->slabs_partial);
	}
	else if (cachep->slab_free != nullptr) {
		//Is there any free slabs
		return slab_alloc(cachep->slab_free);
	}
	else
	{
		//No partal and not free slabs -> allocate one new slab
		allocSlab(cachep);
		//Check for error
		if(cachep->slab_free) return slab_alloc(cachep->slab_free);
		else return nullptr;
	}
}

int cache_shrink(kmem_cache_t *cachep) {
	if (cachep->slab_free == nullptr) return 0;
	if (cachep->shrinked == false || (cachep->shrinked == true && cachep->growing == false)) {
		//cachep->shrinked == false								- first call 
		// cachep->shrinked == true && cachep->growing == false - called but cache is not growing

		//Go through list of free slabs and dealloc slabs
		int cnt = 0;
		for (slab_header* cur = cachep->slab_free, *tmp = nullptr; cur != nullptr; ) {
			tmp = cur;
			cur = cur->next;

			int numOfBlocks = tmp->myCache->blockForSlab;
			slab_destroy(cur); //Updates stats
			buddy_dealloc(tmp, numOfBlocks);

			cnt++;
		}

		cachep->slab_free = nullptr;
		cachep->shrinked = true;
		cachep->growing = false;
		return cnt;
	}
	return 0;
}

void *small_buffer(size_t size) {

	//Check for size 
	int pow = roundUPpower2(size);
	if (pow > MAX_SIZE) return nullptr;
	if (pow < MIN_SIZE) pow = MIN_SIZE;

	if (BUDDY->sizeNCaches[pow - MIN_SIZE] == nullptr) {
		//Create new cache
		char name[20];
		sprintf_s(name, sizeof(name), "size%d", pow);
		BUDDY->sizeNCaches[pow - MIN_SIZE] = cache_create(name, pow * BLOCK_SIZE, nullptr, nullptr);
	}

	//Allocate new slab for that size
	void * ret = cache_alloc(BUDDY->sizeNCaches[pow - MIN_SIZE]);
	return ret;
}

void small_buffer_destroy(const void *objp) {

	int error = put_obj_const(objp);
	if (error) {
		return;
	}
	void * blck = (void*)(((size_t)objp)&(~((size_t)BLOCK_SIZE - 1)));
	slab_header * slab = (slab_header*)blck;

	slab_destroy(slab);
}

void updateStats(kmem_cache_s* cache, int totalAloc, int  inUse, int numSlabs) {
	if (cache == nullptr) return;
	cache->numTotalAloc += totalAloc;
	cache->inUse += inUse;
	cache->numSlabs += numSlabs;
	cache->percentage = ((double)cache->inUse / cache->numTotalAloc) * 100;

	if (numSlabs < 0) {
		cache->sizeInBlocks -= cache->blockForSlab;
	}
	else if (numSlabs > 0) {
		cache->sizeInBlocks += cache->blockForSlab;
	}
}

void allocSlab(kmem_cache_s* cache) {
	//Allocate space from Buddy for one slab
	void * mem = buddy_alloc(BLOCK_SIZE * cache->blockForSlab);
	if (mem == nullptr) {
		std::cout << "\n \n \n Buddy vratio null u allocSlab \n \n";

		cache->error_code = BuddyNoSpace;
		return;
	}
	slab_heder_init(mem, cache);
}
