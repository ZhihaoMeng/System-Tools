#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

int main() {
	//char *a1 = malloc(61191);
	char *a2 = malloc(112);
	char *a3 = malloc(119);
	char *a4 = malloc(120);
	char *a5 = malloc(1);
	char *a6 = malloc(10);
	char *a7 = malloc(555);
	char *a8 = malloc(100);
	char *r8 = realloc(a8, 566);
	char *a9 = calloc(5,32);
	//free(a1);
	free(a2);
	free(a3);
	free(a4);
	free(a5);
	free(a6);
	free(a7);
	free(a8);

}