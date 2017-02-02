#pragma once
#include "slab_header.h"
#include "slab.h"
#include "power2.h"
#include "cache.h"
#define EOB -1


void slab_heder_init(void* space, kmem_cache_s* cache) {

	slab_header * slab = (slab_header*) space;

	slab->myCache = cache;
	
	
	//Where memory for obj starts, offsetted for L1 cache line size
	slab->mem = (char*)space + cache->numObjInSlot * sizeof(buffer) + sizeof(slab_header) + cache->nextOff;

	cache->nextOff = cache->nextOff + CACHE_L1_LINE_SIZE > cache->offset ? 0 : cache->nextOff + CACHE_L1_LINE_SIZE;

	//Init list of objects
	for (int i = 0; i < cache->numObjInSlot; i++) {
		slab_buffer(slab)[i] = i + 1;
	}
	slab_buffer(slab)[cache->numObjInSlot-1] = EOB;
	slab->free = 0;

	//If constructor passed initiaclize objects
	if (cache->ctor != nullptr) {
		void *obj = slab->mem;
		for (int i = 0; i < cache->numObjInSlot; i++) {
			cache->ctor(obj);
			obj = (void*) ((char*)obj + cache->objectSize);
		}
	}

	slab->next = nullptr;
	slab->priv = nullptr;
	slab->objInUse = 0;

	updateLists(nullptr, slab->myCache->slab_free, slab);

	updateStats(slab->myCache, slab->myCache->numObjInSlot, 0, 1);
	
}

void* slab_alloc(slab_header* slab) {
	if (slab->free == EOB) {
		return nullptr;
	}
	void * obj = (char* )slab->mem + slab->free * slab->myCache->objectSize;
	slab->free = slab_buffer(slab)[slab->free];	
	int old = slab->objInUse;
	slab->objInUse++;

	if (old == 0) {
		updateLists(slab->myCache->slab_free, slab->myCache->slabs_partial, slab);
	}
	else if (slab->objInUse == slab->myCache->numObjInSlot) {
		updateLists(slab->myCache->slabs_partial, slab->myCache->slab_full, slab);
	}

	slab->myCache->growing = true;

	updateStats(slab->myCache, 0, 1, 0);

	return obj;
}

void put_obj(void *obj) {
	//Find beginnning of block
	void * blck = (void*) ADDR_TO_BLOCK(obj);
	slab_header * slab = (slab_header*)blck;

	//Index in slab list 
	int index = ((char*)obj - (char*)slab->mem) / slab->myCache->objectSize;

	//Update buffer
	slab_buffer(slab)[index] = slab->free;
	slab->free = index;

	//Call desctructor
	if (slab->myCache->dtor != nullptr) {
		slab->myCache->dtor(obj);
	}
	//Call constructor
	if (slab->myCache->ctor != nullptr) {
		slab->myCache->ctor(obj);
	}

	//Update lists
	int old = slab->objInUse;
	slab->objInUse--;
	if (slab->objInUse == 0) {
		updateLists(slab->myCache->slabs_partial, slab->myCache->slab_free, slab);
	}
	else if (old == slab->myCache->numObjInSlot) {
		updateLists(slab->myCache->slab_full, slab->myCache->slabs_partial, slab);
	}
	updateStats(slab->myCache, 0, -1, 0);
}

void updateLists(slab_header *from, slab_header *to, slab_header *slab) {
	if (slab->next) slab->next->priv = slab->priv;
	if (slab->priv) slab->priv->next = slab->next;
	if (from != nullptr && from == slab) from = slab->next;

	slab->next = to;
	if (to != nullptr)to->priv = slab;
	to = slab;
	slab->priv = nullptr;
/*
if (slab->next) slab->next->priv = slab->priv;
if (slab->priv) slab->priv->next = slab->next;
if (slab->myCache->slabs_partial == slab) slab->myCache->slabs_partial = slab->next;

slab->next = slab->myCache->slab_free;
if (slab->myCache->slab_free != nullptr)slab->myCache->slab_free->priv = slab;
slab->myCache->slab_free = slab;
slab->priv = nullptr;
*/
}
