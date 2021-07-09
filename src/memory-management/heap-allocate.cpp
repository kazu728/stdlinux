#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	char *str = (char *)malloc(100);
	if (str == NULL)
	{
		exit(EXIT_FAILURE);
	}

	gets(str);
	puts(str);
	free(str);

	return 0;
}