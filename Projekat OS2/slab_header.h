#pragma once

#include "slab.h"
#include "cache.h"

typedef unsigned int buffer;
#define slab_buffer(slabp) \
       ((buffer *)(((slab_header*)slabp)+1))



struct slab_header
{
	slab_header *next, *priv;
	kmem_cache_s* myCache;
	//unsigned int offset;
	void* mem;
	buffer free;
	int objInUse;

};

void slab_heder_init(void*, kmem_cache_s*);
void* slab_alloc(slab_header*);
void put_obj(void *obj);
void updateLists(slab_header *from, slab_header *to, slab_header *slab);