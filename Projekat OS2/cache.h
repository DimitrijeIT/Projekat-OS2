#pragma once

#include <Windows.h>
#define NAME_LEN 20

struct slab_header;
enum Errors { NoErros, BuddyNoSpace, CacheNotEmpry , NoCache};
typedef struct kmem_cache_s {
	kmem_cache_s * next;
	kmem_cache_s * priv;

	slab_header *slab_free;
	slab_header *slabs_partial;
	slab_header *slab_full;

	size_t objectSize;

	char name[NAME_LEN];

	int numObjInSlot;
	int blockForSlab;
	unsigned int nextOff;
	unsigned int offset;

	HANDLE mutexLock;

	void(*ctor)(void *);
	void(*dtor)(void *);

	//Statistics
	int numTotalAloc;
	int inUse;
	int numSlabs;
	int sizeInBlocks;
	double percentage;

	bool growing;
	bool shrinked;
	Errors error_code;
} kmem_cache_t;


kmem_cache_t *cache_create(const char *name, size_t size,
	void(*ctor)(void *),
	void(*dtor)(void *));
void *cache_alloc(kmem_cache_t *cachep);
int cache_shrink(kmem_cache_t *cachep);
void *small_buffer(size_t size);
void small_buffer_destroy(const void *objp);

void updateStats(kmem_cache_s* cache, int totalAloc, int  inUse, int numSlabs);
void allocSlab(kmem_cache_s* cache);