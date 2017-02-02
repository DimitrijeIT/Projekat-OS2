#pragma once
#include <math.h>


#define ADDR_TO_BLOCK(addr) ((size_t)addr)&(~((size_t)BLOCK_SIZE-1))

bool isPower2(int x) {
	return x % 2 == 0;
}

int roundUPpower2(int x) {
	if (x == 0) return 0;
	int a = (int)log2(x);
	return (x % 2 == 0)? a : a + 1;
}

int roundDownpower2(int x) {
	if (x == 0) return 0;		
	int a = (int) log2(x);
	return a;
}