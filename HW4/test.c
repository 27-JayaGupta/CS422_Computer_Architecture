#include <stdio.h>
#include <stdlib.h>

#define SIZE ((1 << 21)/sizeof(int))
#define ITERS 80

int a[SIZE];

int main (void)
{
	register int i, j, x=0;

	for (i=0; i<SIZE; i++) a[i] = i;

	for (j=0; j<ITERS; j++) for (i=SIZE-1; i>=0; i--) x += a[i];

	printf("%d\n", x);
	return 0;
}
