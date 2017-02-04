#include "buddy.h"
#include <iostream>
#include "power2.h"
#include "slab.h"

buddy_s* BUDDY = nullptr;

void buddy_init(void *space, int block_num) {

	if (space == nullptr) {
		std::cout << "\n ERROR: Invalid memory adress passed to buddy \n";
		exit(1);
	}

	BUDDY = (buddy_s *) space;

	int numOfBlckforBuddy = (sizeof(buddy_s) + sizeof(int) * roundDownpower2(block_num))/BLOCK_SIZE;
	if (numOfBlckforBuddy == 0) numOfBlckforBuddy = 1;

	BUDDY->numOfBlocks = block_num - numOfBlckforBuddy;
	BUDDY->maxPower2 = roundDownpower2(block_num - numOfBlckforBuddy);
	BUDDY->blocks = (int*)((char*)space + sizeof(buddy_s));
	BUDDY->myMemory = (char*)space + BLOCK_SIZE * numOfBlckforBuddy;
	BUDDY->cacheHead = nullptr;

	BUDDY->mutexLock = CreateMutex(NULL, false, NULL);


	//Init blocks list
	for (int i = 0; i < BUDDY->maxPower2; i++) {
		BUDDY->blocks[i] = -1;
	}
	*((int*)blockByIndex(0)) = -1;

	//Init array of Size-N caches
	for (int i = 0; i <N; i++) {
		BUDDY->sizeNCaches[i] = nullptr;
	}
}

void* buddy_alloc(int size) {
	int index = roundUPpower2(size);
	if (index > BUDDY->maxPower2) return nullptr;

	WaitForSingleObject(BUDDY->mutexLock, INFINITE);

	int block = BUDDY->blocks[index];

	if (block == -1) {
		//No free block of that size
		int i = 1;

		//Find next largest to split
		for (i = 1; i + index < BUDDY->maxPower2; i++) {
			if (BUDDY->blocks[i + index] != -1)break;
		}

		if (i + index == BUDDY->maxPower2) {
			std::cout << "Not enough memory";
			return nullptr;
		}

		//Split blocks
		for (; i > 0; i--) {
			int blockNum = BUDDY->blocks[i + index];
			void* blockAdr = blockByIndex(blockNum);

			//Move pointer to next in list
			BUDDY->blocks[i + index] = *(int*)blockAdr;
		
			int oldNum = BUDDY->blocks[i + index - 1];

			//int newSize = (i + index) / 2;
			int newIndex = i + index - 1;

			//In smaller list update pointer to current block
			BUDDY->blocks[newIndex] = blockNum;
		
			//Update point in block to point to new buddy
			*((int*)blockAdr) = IndexOfBlock((char*)blockAdr + BLOCK_SIZE * toPower2(newIndex));
			//New buddy to point to new in list
			*((int*)(char*)blockAdr + BLOCK_SIZE * toPower2(newIndex)) = oldNum;
		}
		//New block for alloc
		block = BUDDY->blocks[index];
	}

	void * newBlock = blockByIndex(block);
	BUDDY->blocks[index] = *(int*)newBlock;

	ReleaseMutex(BUDDY->mutexLock);
	 
	return newBlock;
}

void* blockByIndex(int index) {
	if (index > BUDDY->numOfBlocks) return nullptr;

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

	int old = BUDDY->blocks[index];
	
	//Update lists
	*(int*)adr = old;
	BUDDY->blocks[index] = num;

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
				*(int*)adress = -1; //Not nessesery 

				adress = cur_block_adress;
				chunck_num = cur_num;
				break;
			}
			//Go to next in list 
			prev_num = cur_num;
			cur_num = *(int*)cur_block_adress;
		}

		if (cur_num == -1) again = false;
		else {	

			//Update current list 
			if (prev_num != -1) {
				*(int*)blockByIndex(prev_num) = *(int*)blockByIndex(cur_num);
				//Must remove first in list. It is the one we just inserted
				BUDDY->blocks[cur_index] = *(int*)blockByIndex(BUDDY->blocks[cur_index]);
			}
			else {
				BUDDY->blocks[cur_index] = *(int*)blockByIndex(cur_num);
			}

			int new_cur = BUDDY->blocks[cur_index + 1];

			//Put in list above dealocating chunck 
			BUDDY->blocks[cur_index + 1] = chunck_num;
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
		BUDDY->blocks = nullptr;
		BUDDY->cacheHead = nullptr;
		BUDDY->myMemory = nullptr;

		BUDDY->maxPower2 = 0;
		BUDDY->numOfBlocks = 0;

		for (int i = 0; i < N; i++) {
			BUDDY->sizeNCaches[i] = nullptr;
		}

		CloseHandle(BUDDY->mutexLock;
	}

	BUDDY = nullptr;
}