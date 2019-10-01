#define SMRAM_SIZE (0x00010000)
#define SMRAM_START ((void *)0x000a0000)
#define SMRAM_END (SMRAM_START + SMRAM_SIZE)

// is_within_smram()
// Parameters:
//      addr - the start address of the buffer
//      size - the size of the buffer
// Return Value:
//      1 - the buffer is strictly within the smram
//      0 - the whole buffer or part of the buffer is not within the smram,
//          or the buffer is wrap around
//

int is_within_smram(const void *addr, int size)
{
    unsigned int start = (unsigned int)addr;
    unsigned int end = 0;
    
    unsigned int smram_start = (unsigned int)SMRAM_START;
    unsigned int smram_end = (unsigned int)SMRAM_END - 1;
    
    if (size > 0) {
        end = start + size - 1;
    } else {
        end = start;
    }
    
    if ((start <= end) && (smram_start <= start) && (end <= smram_end)) {
        return 1;
    }
    return 0;
}
