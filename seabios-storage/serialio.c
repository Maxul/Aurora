#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

void serial_debug_puts(char *s, int len)
{
    for (int i=0; i<len; ++i)
    {
        outb(s[i], 0x3f8);
    }
    outb('\n', 0x3f8);
}

int main()
{
#define SIZE (1<<16)
    static char str[SIZE];
    
    memset(str, 'X', SIZE);
    
    if (0 != ioperm(0x3f8, 1, 1)) {
        err(EXIT_FAILURE, "ioperm");
    }
    
    serial_debug_puts(str, strlen(str));
    
    return 0;
}
