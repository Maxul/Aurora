#include <stdio.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define WAIT_FOR_INTERRUPT

#define SVM_SIZE ((1<<20))

int main()
{
    int uiofd, intr_cnt = 0;
    char *map_addr;
    fd_set readset;

    uiofd = open("/dev/uio0", O_RDWR);
    if (uiofd < 0) {
        perror("uio open:");
        return errno;
    }

    map_addr = mmap(NULL, SVM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, uiofd, 0);

    write(uiofd, &intr_cnt, sizeof(intr_cnt));
    write(uiofd, &intr_cnt, sizeof(intr_cnt));
    write(uiofd, &intr_cnt, sizeof(intr_cnt));

#if 0
    // Hammer away!
    while (1) {
#ifdef WAIT_FOR_INTERRUPT
        FD_ZERO(&readset);
        FD_SET(uiofd, &readset);
        if (select(uiofd + 1, &readset, NULL, NULL, NULL) <= 0) {
            perror("select:");
            break;
        }
        if (FD_ISSET(uiofd, &readset)) {
            if (read(uiofd, &intr_cnt, 4) != 4) {
                perror("uio read:");
                break;
            }
            printf("intr count %d\n", intr_cnt);
            memset(map_addr, 0, SVM_SIZE);
        }
#else
        memset(map_addr, 0, SVM_SIZE);
#endif
    }
#endif
    munmap(map_addr, SVM_SIZE);
    close(uiofd);
    return 0;
}

