#pragma once
typedef unsigned int buffer;
#define slab_buffer(slabp) \
       ((buffer *)(((slab_header*)slabp)+1))

struct kmem_cache_s;

enum slab_list { FREE, PARTIAL, FULL, NO };

struct slab_header
{
	slab_header *next, *priv;
	kmem_cache_s* myCache;

	//Pointer to first object stored in this slab
	void* mem;

	//Index of free object in slab
	buffer free;

	//Statistics
	int objInUse;
};

void slab_heder_init(void*, kmem_cache_s*);
void* slab_alloc(slab_header*);
//Put back one object
int put_obj(void *obj);
int put_obj_const(const void *obj);
//void updateLists(slab_header *from, slab_header *to, slab_header *slab);
void updateLists(slab_list FROM, slab_list TO, slab_header *slab);
void slab_destroy(slab_header* slab);