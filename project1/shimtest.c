#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>



int main() {
	char *a1 = malloc(110);
	char *a2 = malloc(112);
	char *a3 = malloc(119);
	char *a4 = malloc(120);
	char *a5 = malloc(1);
	char *a6 = malloc(10);
	char *a7 = malloc(1);
	char *a8 = malloc(30);
	
	free(a2);

}
