#include "slab.h"
#include "buddy.h"
#include "cache.h"
#include "slab_header.h"
#include <iostream>

extern buddy_s* BUDDY;

void kmem_init(void *space, int block_num) {
	buddy_init(space, block_num);
}

kmem_cache_t *kmem_cache_create(const char *name, size_t size,
	void(*ctor)(void *),
	void(*dtor)(void *)) {

	kmem_cache_t* ret = cache_create(name, size, ctor, dtor);
	return ret;
}

void *kmem_cache_alloc(kmem_cache_t *cachep) {
	WaitForSingleObject(cachep->mutexLock, INFINITE);
	void * ret = cache_alloc(cachep);
	ReleaseMutex(cachep->mutexLock);
	return ret;
}

void kmem_cache_free(kmem_cache_t *cachep, void *objp) {
	WaitForSingleObject(cachep->mutexLock, INFINITE);
	put_obj(objp);
	ReleaseMutex(cachep->mutexLock);
}

int kmem_cache_shrink(kmem_cache_t *cachep) {
	WaitForSingleObject(cachep->mutexLock, INFINITE);
	int ret = cache_shrink(cachep);
	ReleaseMutex(cachep->mutexLock);
	return ret;
}

void kmem_cache_destroy(kmem_cache_t *cachep) {
	cache_destroy(cachep);
}

void *kmalloc(size_t size) {
	return small_buffer(size);
}

void kfree(const void *objp) {
	small_buffer_destroy(objp);
}

void kmem_cache_info(kmem_cache_t *cachep){
	WaitForSingleObject(cachep->mutexLock, INFINITE);
	WaitForSingleObject(BUDDY->globalLock, INFINITE);
	std::cout << "\n \n Name			: " << cachep->name << "\n Object Size		: " << cachep->objectSize << "\n Cache size in blocks	: "
		<< cachep->sizeInBlocks << "\n Num of slabs		: " << cachep->numSlabs << "\n Num of object per slab	: "
		<< cachep->numObjInSlot << "\n Procenat popunjenosti	: " << cachep->percentage << "\n";
	ReleaseMutex(BUDDY->globalLock);
	ReleaseMutex(cachep->mutexLock);
}

int kmem_cache_error(kmem_cache_t *cachep) {
	if (cachep == nullptr) {
		std::cout << "cachep: is nullptr";
		return NoCache;
	}

	WaitForSingleObject(cachep->mutexLock, INFINITE);
	switch (cachep->error_code) {
		case NoErros: 
			std::cout << "No errors";
			break;
		case BuddyNoSpace: 
			std::cout << "Buddy can not find enought space";
			break;
		case CacheNotEmpry:
			std::cout << "User has not freed all objects ";
			break;
		default:
			std::cout << "Unknown error";
			break;
	}
	ReleaseMutex(cachep->mutexLock);
	return cachep->error_code;
}


