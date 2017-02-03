#include "slab.h"
#include "buddy.h"
#include "cache.h"
#include "slab_header.h"
#include <iostream>

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
	put_obj(objp);
}

int kmem_cache_shrink(kmem_cache_t *cachep) {
	
	WaitForSingleObject(cachep->mutexLock, INFINITE);
	int ret = cache_shrink(cachep);
	ReleaseMutex(cachep->mutexLock);
	return ret;
}

void kmem_cache_destroy(kmem_cache_t *cachep) {
	kmem_cache_shrink(cachep);

	if (cachep->slabs_partial != nullptr || cachep->slab_full != nullptr) {
		cachep->error_code = CacheNotEmpry;
	}
}

void *kmalloc(size_t size) {
	return small_buffer(size);
}

void kmem_cache_info(kmem_cache_t *cachep){

	WaitForSingleObject(cachep->mutexLock, INFINITE);
	std::cout << " Name :" << cachep->name << "\n Object Size : " << cachep->objectSize << "\n Cache size in blocks : "
		<< cachep->sizeInBlocks << "\n Num of slabs : " << cachep->numSlabs << "\n Num of object per slab : "
		<< cachep->numObjInSlot << "\n Procenat popunjenosti : " << cachep->percentage;
	ReleaseMutex(cachep->mutexLock);
}

