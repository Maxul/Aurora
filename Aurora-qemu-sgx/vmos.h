ram_addr_t gpa = 0;
const char *hdb_arg;

//#define _VMOS

#ifdef _VMOS

#include <libgen.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sem.h>

#define MAGIC "#*"
#define OFFSET 0x10
#define PRJ_ID 0xf

#define KMEM_SIZE (1<<22) // 4MB
#define SHARED_MEM_SIZE KMEM_SIZE

#define SHM_IN_OFF 2
#define GUEST_IN_OFF SHM_IN_OFF
#define SHM_OUT_OFF 1024
#define GUEST_OUT_OFF SHM_OUT_OFF

#define INPUT_SIZE (SHM_OUT_OFF - SHM_IN_OFF)
#define OUTPUT_SIZE (KMEM_SIZE - GUEST_OUT_OFF)

#define gk_addr (gpa + gpa_off) // guest kernel address

#ifdef _VMOS
#  define DBGprintf printf
#else
#  define DBGprintf
#endif

#define DBGExit(fmt) { DBGprintf(fmt); exit(EXIT_FAILURE); }
#define DBGError(err) { perror(err); exit(EXIT_FAILURE); }

static pthread_t tid_shm;



static int flag_appvm = 0;
static uint64_t gpa_off = 0; /* should be uint64_t both in QEMU and Guest Driver */

static int sem_id_close;

static int sem_id;
static int shm_id;
static char *shm_addr; /* points to the shared memory*/

union semun {
    int val;
    unsigned short *array;
    struct semid_ds *buf;
} sem_union;

static int semOpen(int sem_id)
{
    sem_union.val = 1;
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1) {
        DBGprintf("semOpen failed\n");
        return 0;
    }
    return 1;
}

static int semClose(int sem_id)
{
    if (semctl(sem_id, 0, IPC_RMID, sem_union) == -1) {
        DBGprintf("semClose failed\n");
        return 0;
    }
    return 1;
}

static int semLock(int sem_id)
{
    struct sembuf sem_b;

    sem_b.sem_num = 0;
    sem_b.sem_op = -1;
    sem_b.sem_flg = SEM_UNDO;

    if (semop(sem_id, &sem_b, 1) == -1) {
        DBGprintf("semLock failed\n");
        return 0;
    }
    return 1;
}

static int semUnlock(int sem_id)
{
    struct sembuf sem_b;

    sem_b.sem_num = 0;
    sem_b.sem_op = 1;
    sem_b.sem_flg = SEM_UNDO;

    if (semop(sem_id, &sem_b, 1) == -1) {
        DBGprintf("semUnlock failed\n");
        return 0;
    }
    return 1;
}

static void allocate_shared_memory(void)
{
    /* calculate key using '-hdb' argument */
    key_t key = ftok(hdb_arg, PRJ_ID);
    if (key == -1)
        DBGError("ftok error");

    sem_id = semget(key, 1, IPC_CREAT | 0600);
    if (!semOpen(sem_id))
        DBGExit("Failed to initialize semaphore\n");

    /* create shared memory segment with this key */
    shm_id = shmget(key, SHARED_MEM_SIZE, IPC_CREAT | 0600);
    if (-1 == shm_id)
        DBGError("shmget error");

    /* attach this segment to its address space */
    shm_addr = (char *)shmat(shm_id, NULL, 0);
    if ((void *)-1 == shm_addr)
        DBGError("shmat error");

    /* initialize it */
    bzero(shm_addr, SHARED_MEM_SIZE);
    *(char *)(shm_addr + 0) = '-';
    *(char *)(shm_addr + 1) = '0';
}

static void locate_guest_memory(void)
{
    /* waiting for Virtual Physical Address to be ready */
    while (0 != memcmp((void *)gpa, MAGIC, sizeof(MAGIC))) {
        sleep(1);
        //DBGprintf(" .");
        fflush(stdout);
    }

    /* Obtain VirtPhyAddr written by Guest Driver */
    gpa_off = *(int32_t *)(gpa + OFFSET);
    DBGprintf("==> 0x%lx\n", gpa_off);
}

/* thread operating the shared memory */
static void *shm_op(void *arg)
{
    /* First of all, we are to establish our identity to supervisor for ID check. */

    /* Then we have prepared a shared memory and semaphore
        to operate it with server correctly, */
    /* hdb --> key --> shm && sem */
    allocate_shared_memory();
    locate_guest_memory();

    /* Inform Guest Agent that QEMU has found the Address */
    *(char *)(gk_addr) = '0';

    for (;;)
    {
        /* Obtain an Event from Frontend */
        semLock(sem_id);

        if ('+' == *(char *)(shm_addr))
        {
            memcpy(gk_addr + GUEST_IN_OFF, shm_addr + SHM_IN_OFF, INPUT_SIZE);
            /* A New Event has Occurred, notify inside */
            *(char *)(gk_addr) = '+';
#if 0
            /* if loop count >=1000 then break and ignore this event*/
            int eventloop_count = 0;
            for (;;)
            {
                /* Backend has Recieved or Otherwise Ignore it */
                if (*(char *)(gk_addr + 2) == 'A' || ++eventloop_count >= 1000)
                {
                    /* Discard this Event */
                    *(char *)(shm_addr + 2) = 'A';
                    break;
                }
            }
#endif
            *(char *)(shm_addr) = '-';
        }

        semUnlock(sem_id);
        /* 虽然是连续操作，但是不建议去掉这个锁，否则会影响反应实时性 */

        /* Push Guest Window Info to Frontend */
        semLock(sem_id);
        /* Backend has sent && frontend has gotten Previous Data */
        if (*(char *)(gk_addr + 1) == '1' && *(char *)(shm_addr + 1) == '0')
        {
            /* Copy from GHA to shared memory region */
            bzero((void *)(shm_addr + SHM_OUT_OFF), OUTPUT_SIZE);
            memcpy((void *)(shm_addr + SHM_OUT_OFF), (void *)(gk_addr + GUEST_OUT_OFF), OUTPUT_SIZE);

            /* Tell Frontend that QEMU has sent */
            *(char *)(shm_addr + 1) = '1';
            /* Tell Backend that QEMU has copied */
            *(char *)(gk_addr + 1) = '0';

            //static long long unsigned i = 0;
            //DBGprintf("Copy From Monitor to Mirror: 0x%0llx.\n", i++);
        }
        semUnlock(sem_id);
        // NO SLEEP -> the key to accelerate!!!
    }
    return NULL;
}
#endif /* _VMOS */

