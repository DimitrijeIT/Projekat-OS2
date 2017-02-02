#include "slab.h"
#include "buddy.h"
#include "cache.h"
#include "slab_header.h"
#include "power2.h"

#include <iostream>

#define MAX_SIZE 17
#define MIN_SIZE 5


enum Errors { NoErros, BuddyNoSpace, CacheNotEmpry};

void kmem_init(void *space, int block_num) {
	buddy_init(space, block_num);
}

kmem_cache_t *kmem_cache_create(const char *name, size_t size,
	void(*ctor)(void *),
	void(*dtor)(void *)) {

	int s = sizeof(kmem_cache_s) / BLOCK_SIZE;
	int numOfBlocksForChacheStruct = sizeof(kmem_cache_s) % BLOCK_SIZE == 0 ? s : s+1;
	//numOfBlocksForChacheStruct = numOfBlocksForChacheStruct%BLOCK_SIZE == 0 ? numOfBlocksForChacheStruct : numOfBlocksForChacheStruct + 1;

	int availbale_space = BLOCK_SIZE - sizeof(slab_header) - sizeof(buffer);
	int leftOver = 0;
	int blocksForSlab = 1;
	int obj_in_slab = 1;
	if (size < availbale_space) {
		while ( obj_in_slab * sizeof(buffer) + obj_in_slab*size < availbale_space) {
			obj_in_slab++;
		}
		obj_in_slab--;
		leftOver = availbale_space - (obj_in_slab* size + (obj_in_slab - 1) * sizeof(buffer));
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

	//Just allock blocks for struct
	void * mem = buddy_alloc(numOfBlocksForChacheStruct * BLOCK_SIZE);
	kmem_cache_s* cache_s = (kmem_cache_s*)mem;


	//Init struct
	cache_s->priv = nullptr;
	cache_s->next = BUDDY->cacheHead;
	if (cache_s->next)cache_s->next->priv = cache_s;
	BUDDY->cacheHead = cache_s;

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

void *kmem_cache_alloc(kmem_cache_t *cachep) {
	if (cachep == nullptr) return nullptr;

	if (cachep->slabs_partial != nullptr) {
		return slab_alloc(cachep->slabs_partial);
	}
	else if (cachep->slab_free != nullptr) {
		return slab_alloc(cachep->slab_free);
	}
	else
	{
		allocSlab(cachep);
		return slab_alloc(cachep->slab_free);
	}
}

void kmem_cache_free(kmem_cache_t *cachep, void *objp) {
	put_obj(objp);
}

int kmem_cache_shrink(kmem_cache_t *cachep) {
	if (cachep->slab_free == nullptr) return 0;
	if (cachep->shrinked == false || (cachep->shrinked == true && cachep->growing == false) ){
		slab_header* cur = cachep->slab_free;
		slab_header* tmp = nullptr;
		int cnt = 0;
		while (cur != nullptr) {
			tmp = cur;
			cur = cur->next;
			buddy_dealloc(tmp, BLOCK_SIZE);
			cnt++;
			updateStats(cachep, -cachep->numObjInSlot, 0, -1);
		}
		cachep->slab_free = nullptr;
		cachep->shrinked = true;
		cachep->growing = false;
		return cnt;
	}
	return 0;
}

void kmem_cache_destroy(kmem_cache_t *cachep) {
	kmem_cache_shrink(cachep);

	if (cachep->slabs_partial != nullptr || cachep->slab_full != nullptr) {
		cachep->error_code = CacheNotEmpry;
	}
}

void *kmalloc(size_t size) {
	int pow = roundUPpower2(size);
	if (pow > MAX_SIZE) return nullptr;
	if (pow < MIN_SIZE) pow = MIN_SIZE;

	if (BUDDY->sizeNCaches[pow - MIN_SIZE] == nullptr) {
		char name[20];
		sprintf_s(name, sizeof(name), "size%d", pow);
		BUDDY->sizeNCaches[pow - MIN_SIZE] = kmem_cache_create(name, pow * BLOCK_SIZE, nullptr, nullptr);
	}
	
	void * ret = kmem_cache_alloc(BUDDY->sizeNCaches[pow - MIN_SIZE]);

	return ret;
}

void updateStats(kmem_cache_s* cache, int totalAloc, int  inUse, int numSlabs){
	if (cache == nullptr) return;
	cache->numTotalAloc += totalAloc;
	cache->inUse += inUse;
	cache->numSlabs += numSlabs;
	cache->percentage = ((double)cache->numTotalAloc / cache->inUse )* 100;

	if (numSlabs < 0) {
		cache->sizeInBlocks -= cache->blockForSlab;
	}
	else if (numSlabs > 0) {
		cache->sizeInBlocks += cache->blockForSlab;
	}
}

void allocSlab(kmem_cache_s* cache) {
	void * mem = buddy_alloc(BLOCK_SIZE * cache->blockForSlab);
	if (mem == nullptr) {
		cache->error_code = BuddyNoSpace;
		return;
	}
	slab_heder_init(mem, cache);
}

void kmem_cache_info(kmem_cache_t *cachep){

	std::cout << " Name :" << cachep->name << "\n Object Size : " << cachep->objectSize << "\n Cache size in blocks : "
		<< cachep->sizeInBlocks << "\n Num of slabs : " << cachep->numSlabs << "\n Num of object per slab : "
		<< cachep->numObjInSlot << "\n Procenat popunjenosti : " << cachep->percentage;


}

void kfree(const void *objp) {
	
}
