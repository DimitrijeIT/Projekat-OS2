#include "buddy.h"
#include <iostream>
#include "power2.h"
#include "slab.h"
//All functions are thread safe
#define EOB -1

buddy_s* BUDDY = nullptr;

void buddy_init(void *space, int block_num) {

	if (space == nullptr) {
		std::cout << "\n ERROR: Invalid memory adress passed to buddy \n";
		exit(1);
	}

	BUDDY = (buddy_s *) space;

	int numOfBlckforBuddy = (sizeof(buddy_s) + sizeof(blocksP) * roundDownpower2(block_num))/BLOCK_SIZE;
	if (numOfBlckforBuddy == 0) numOfBlckforBuddy = 1;
	BUDDY->numOfBlocks = block_num - numOfBlckforBuddy;

	BUDDY->maxPower2 = roundDownpower2(block_num - numOfBlckforBuddy);

	//Buddy will allocate blocks from this adress
	//Adress is corrected = first BLOCK_SIZE bits are 0
	size_t mem = (size_t) ((char*)space + BLOCK_SIZE * numOfBlckforBuddy);
	mem += BLOCK_SIZE - 1;
	size_t mask = BLOCK_SIZE - 1;
	mem &= ~mask;
	BUDDY->myMemory = (void *)mem;

	BUDDY->cacheHead = nullptr;

	BUDDY->mutexLock = CreateMutex(NULL, false, NULL);
	BUDDY->globalLock = CreateMutex(NULL, false, NULL);

	//Init blocks list
	for (int i = 0; i < BUDDY->maxPower2; i++) {
		buddy_blocks(BUDDY)[i] = EOB;
	}
	buddy_blocks(BUDDY)[BUDDY->maxPower2] = 0;
	*((int*)blockByIndex(0)) = EOB;

	//Init array of Size-N caches
	for (int i = 0; i <N; i++) {
		BUDDY->sizeNCaches[i] = nullptr;
	}
}

void* buddy_alloc(int size) {

	int num_of_blc = size % BLOCK_SIZE == 0 ? size / BLOCK_SIZE : size / BLOCK_SIZE + 1;
	int index = roundUPpower2(num_of_blc);
	if (index > BUDDY->maxPower2) return nullptr;

	WaitForSingleObject(BUDDY->mutexLock, INFINITE);

	int block = buddy_blocks(BUDDY)[index];

	if (block == -1) {
		//No free block of that size
		int i = 1;

		//Find next largest to split
		for (i = 1; index + i <= BUDDY->maxPower2; i++) {
			if (buddy_blocks(BUDDY)[i + index] != -1)break;
		}

		if (i + index > BUDDY->maxPower2) {
			std::cout << "Not enough memory";
			return nullptr;
		}

		//Split blocks
		for (; i > 0; i--) {
			int blockNum = buddy_blocks(BUDDY)[i + index];
			void* blockAdr = blockByIndex(blockNum);

			//Move pointer to next in list
			buddy_blocks(BUDDY)[i + index] = *((int*)blockAdr);
		
			int newIndex = i + index - 1;
			int oldNum = buddy_blocks(BUDDY)[newIndex];
			
			//In smaller list update pointer to current block
			buddy_blocks(BUDDY)[newIndex] = blockNum;
		
			//Update point in block to point to new buddy
			//adr - adress of new buddy
			char * adr = (char*)blockAdr + BLOCK_SIZE * toPower2(newIndex);
			int a = IndexOfBlock(adr);
			*((int*)blockAdr) = a;
			//New buddy to point to old in list
			*((int*)adr) = oldNum;
		}
		//New block for alloc
		block = buddy_blocks(BUDDY)[index];
	}

	void * newBlock = blockByIndex(block);
	buddy_blocks(BUDDY)[index] = *(int*)newBlock;

	ReleaseMutex(BUDDY->mutexLock);
	 
	return newBlock;
}

void* blockByIndex(int index) {
	if (index > BUDDY->numOfBlocks || index < 0) {
		return nullptr;
	}
	return (char*) BUDDY->myMemory + BLOCK_SIZE * index;
}

int IndexOfBlock(void* adr) {
	if (adr == nullptr || adr < BUDDY->myMemory || adr >((char*) BUDDY->myMemory + BLOCK_SIZE * BUDDY ->numOfBlocks)) 
		return -1;
	return ((char*)adr - (char*)BUDDY->myMemory) / BLOCK_SIZE;
} 

void buddy_dealloc(void *adr, int size_) {
	//Checks
	int num = IndexOfBlock(adr);
	if (num == -1) return;
	
	int index = roundUPpower2(size_);
	if (index < 0 || index> BUDDY->maxPower2) return;

	WaitForSingleObject(BUDDY->mutexLock, INFINITE);

	int old = buddy_blocks(BUDDY)[index];
	
	//Update lists
	*(int*)adr = old;
	buddy_blocks(BUDDY)[index] = num;

	//Do grouping

	int cur_num = old;
	int cur_index = index;
	int prev_num = -1;

	void * adress = adr;
	int chunck_num = num;

	void * cur_block_adress = nullptr;
	bool again = true;
	while (again) {
		//Go througih one list
		while (cur_num != -1) {
			//Try to find buddy
			cur_block_adress = blockByIndex(cur_num);
			if ((char*)adress + toPower2(cur_index) * BLOCK_SIZE == cur_block_adress) {
				break;
			}
			if ((char*)cur_block_adress + toPower2(cur_index) * BLOCK_SIZE == adress) {
				//Swap places
				*(int*)adress = -1;
				adress = cur_block_adress;
				chunck_num = cur_num;
				break;
			}
			//Go to next in list 
			prev_num = cur_num;
			cur_num = *(int*)cur_block_adress;
		}//While(cur_num != -1)

		if (cur_num == -1) again = false;
		else {	
			//Update current list 
			if (prev_num != -1) {
				*(int*)blockByIndex(prev_num) = *(int*)blockByIndex(cur_num);
				//Must remove first in the list. It is the one we just have inserted
				buddy_blocks(BUDDY)[cur_index] = *(int*)blockByIndex(buddy_blocks(BUDDY)[cur_index]);
			}
			else {
				buddy_blocks(BUDDY)[cur_index] = *(int*)blockByIndex(cur_num);
			}

			int new_cur = buddy_blocks(BUDDY)[cur_index + 1];

			//Put in list above dealocating chunck 
			buddy_blocks(BUDDY)[cur_index + 1] = chunck_num;
			*(int *)adress = new_cur;

			//Update for next iteration 
			cur_index++;
			cur_num = new_cur;
		}

	}

	ReleaseMutex(BUDDY->mutexLock);
}

void destroyBuddy() {
	if (BUDDY != nullptr) {

		//Destroy sizeN caches
		for (int i = 0; i < N; i++) {
			kmem_cache_destroy(BUDDY->sizeNCaches[i]);
			BUDDY->sizeNCaches[i] = nullptr;
		}

		BUDDY->cacheHead = nullptr;
		BUDDY->myMemory = nullptr;

		BUDDY->maxPower2 = 0;
		BUDDY->numOfBlocks = 0;

		CloseHandle(BUDDY->mutexLock);
		CloseHandle(BUDDY->globalLock);
	}

	BUDDY = nullptr;
}