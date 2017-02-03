#include "power2.h"
#include <math.h>
int roundUPpower2(int x) {
	if (x == 0) return 0;
	int a = (int)log2(x);
	return (x % 2 == 0) ? a : a + 1;
}

int roundDownpower2(int x) {
	if (x == 0) return 0;
	int a = (int)log2(x);
	return a;
}

int toPower2(int x) {
	return (int)pow(2, x);
}