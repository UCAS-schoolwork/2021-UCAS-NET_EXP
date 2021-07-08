#include <stdio.h>

int main()
{
    char s[9000000];
    char c;
    while((c=getchar())!=EOF)
        putchar(c);
    return 0;
}