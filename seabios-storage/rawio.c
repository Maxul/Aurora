#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
// O_DIRECT
#define __USE_GNU 1
#include <fcntl.h>
#include <stdint.h>

#include <errno.h>
#include <sys/mman.h>

#define READFILE_BUFFER_SIZE (1 * 1024)
char fullpath[20] = "/dev/raw/raw1";

int main() {
  int fd = open(fullpath, O_DIRECT | O_CREAT | O_RDWR | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (fd < 0) {
    printf("Fail to open output protocol file: \'%s\'", fullpath);
    return -1;
  }

  uint32_t bufferLen = (READFILE_BUFFER_SIZE);
  char *buffer = (char *)mmap(0, bufferLen, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (MAP_FAILED == buffer) {
    printf("mmap error!errno=%d\n", errno);
    return -1;
  }

  printf("mmap first address:%p\n", buffer);
  uint32_t address = (uint32_t)buffer;
  printf("address=%u;address%4096=%u\n", address, address % 4096);

  uint32_t pos = 0, len = 0;
  len = snprintf((char *)(buffer + pos), 128, "AURORAXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n");
  pos += len;

  if (pos) {
    buffer[pos] = '\0';
    printf("pos=%d,buffer=%s\n", pos, buffer);
    uint32_t wLen = (pos + 4095) & ~4095U;
    write(fd, buffer, wLen);
    truncate(fullpath, pos);
    fsync(fd);
  }
  if (buffer) {
    munmap(buffer, bufferLen);
    buffer = NULL;
  }
  if (fd > 0) {
    close(fd);
  }
  return 0;
}
