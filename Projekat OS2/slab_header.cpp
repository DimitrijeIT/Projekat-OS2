#pragma once
#include "slab_header.h"
#include "slab.h"
#include "cache.h"
#include <iostream>

#define EOB -1
#define ALLOCATED -2

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

	//Add to free list of slabs
	updateLists(NO, FREE, slab);

	updateStats(slab->myCache, slab->myCache->numObjInSlot, 0, 1);
	
}

void* slab_alloc(slab_header* slab) {

	//Is slab full
	if (slab->free == EOB) {
		std::cout << "\n  EOB NADJEN ";
		std::cout << "Obj u slabu " <<slab->objInUse << " Kes dozvolio " << slab->myCache->numObjInSlot;
		return nullptr;
	}

	if (slab->free < 0) {
		std::cout << " NEKAKO FREE MANJI OD NULE \n \n \n";
		return nullptr;
	}
	
	//Pointer to free object
	void * obj = (void * ) ((char* )slab->mem + slab->free * slab->myCache->objectSize);
	int obj_index = slab->free;
	slab->free = slab_buffer(slab)[slab->free];	
	slab_buffer(slab)[obj_index] = ALLOCATED;

	//If slab was empry or slab is now full, update lists in cache
	int old = slab->objInUse;
	slab->objInUse++;

	if (old == 0) {
		if(slab->objInUse != slab->myCache->numObjInSlot ) updateLists(FREE, PARTIAL, slab);
		else updateLists(FREE, FULL, slab);
	}
	else if (slab->objInUse == slab->myCache->numObjInSlot) {
		updateLists(PARTIAL, FULL, slab);
	}

	slab->myCache->growing = true;
	updateStats(slab->myCache, 0, 1, 0);

	return obj;
}

int put_obj(void *obj) {
	//Find beginnning of block
	void * blck = (void*) (((size_t)obj)&(~((size_t)BLOCK_SIZE - 1)));

	slab_header * slab = (slab_header*)blck;

	//Index in slab list 
	int index = ((char*)obj - (char*)slab->mem) / slab->myCache->objectSize;

	if (slab_buffer(slab)[index] != ALLOCATED) {
		std::cout << "Object is not allocated";
		return -1;
	}

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
		if(old != slab->myCache->numObjInSlot) updateLists(PARTIAL, FREE, slab);
		else updateLists(FULL, FREE, slab);
	}
	else if (old == slab->myCache->numObjInSlot) {
		updateLists(FULL,PARTIAL, slab);
	}
	updateStats(slab->myCache, 0, -1, 0);
	return 0;
}

int put_obj_const (const void *obj) {
	//Find beginnning of block
	void * blck = (void*)(((size_t)obj)&(~((size_t)BLOCK_SIZE - 1)));

	slab_header * slab = (slab_header*)blck;

	//Index in slab list 
	int index = ((char*)obj - (char*)slab->mem) / slab->myCache->objectSize;

	if (slab_buffer(slab)[index] != ALLOCATED) {
		std::cout << "Object is not allocated";
		return -1;
	}
	//Update buffer
	slab_buffer(slab)[index] = slab->free;
	slab->free = index;

	//Update lists
	int old = slab->objInUse;
	slab->objInUse--;
	if (slab->objInUse == 0) {
		if (old != slab->myCache->numObjInSlot) updateLists(PARTIAL, FREE, slab);
		else updateLists(FULL, FREE, slab);
	}
	else if (old == slab->myCache->numObjInSlot) {
		updateLists(FULL, PARTIAL, slab);
	}
	updateStats(slab->myCache, 0, -1, 0);
	return 0;
}
/*void updateLists(slab_header *from, slab_header *to, slab_header *slab) {
	if (slab->next) slab->next->priv = slab->priv;
	if (slab->priv) slab->priv->next = slab->next;
	if (from != nullptr && from == slab) from = slab->next;

	slab->next = to;
	if (to != nullptr) to->priv = slab;
	to = slab;
	slab->priv = nullptr;
}
*/
void updateLists(slab_list FROM, slab_list TO, slab_header *slab) {
	if (slab->next) slab->next->priv = slab->priv;
	if (slab->priv) slab->priv->next = slab->next;

	kmem_cache_s* cache = slab->myCache;

	switch (FROM)
	{
	case FREE:
		if (cache->slab_free == slab) cache->slab_free = slab->next;
		std::cout << "Uklonio iz liste free \n \n ";
		break;
	case PARTIAL:
		if (cache->slabs_partial == slab) cache->slabs_partial = slab->next;
		break;
	case FULL:
		if (cache->slab_full == slab) cache->slab_full = slab->next;
		break;
	}

	slab_header *to = nullptr;
	switch (TO)
	{
	case FREE:
		if (cache->slab_free) cache->slab_free->priv = slab;
		to = cache->slab_free;
		cache->slab_free = slab;
		break;
	case PARTIAL:
		if (cache->slabs_partial)cache->slabs_partial->priv = slab;
		to = cache->slabs_partial;
		cache->slabs_partial = slab;
		break;
	case FULL:
		if (cache->slab_full) cache->slab_full->priv = slab;
		to = cache->slab_full;
		cache->slab_full = slab;
		break;
	}

	slab->next = to;
	slab->priv = nullptr;
}

void slab_destroy(slab_header* slab) {
	if (slab == nullptr) return;

	//Remove from cache list
	if (slab->objInUse == 0) {
		std::cout << "\n \n U slabu svi slobodno ";
		updateLists(FREE, NO, slab);
	}
	else if (slab->objInUse == slab->myCache->numObjInSlot) {
		std::cout << " \n \n U slabu NISU svi slobodno ";
		updateLists(FULL, NO, slab);
	}
	else {
		std::cout << " \n \n U slabu NISU svi slobodno ";
		updateLists(PARTIAL, NO, slab);
	}

	//Call destuctor
	if (slab->myCache->dtor) {
		void *obj = slab->mem;
		for (int i = 0; i < slab->myCache->numObjInSlot; i++) {
			slab->myCache->dtor(obj);
			obj = (void*)((char*)obj + slab->myCache->objectSize);
		}
	}

	updateStats(slab->myCache, -(slab->myCache->numObjInSlot), 0, -1);
	
	slab->mem = nullptr;
	slab->free = EOB;
	slab->objInUse = 0;
	//slab->myCache = nullptr;
}

