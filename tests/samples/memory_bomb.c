#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLOCK_SIZE (1024 * 1024)

int main(void) {
    void **blocks = NULL;
    size_t count = 0;

    while (1) {
        void *p = malloc(BLOCK_SIZE);
        if (p == NULL) {
            perror("malloc");
            break;
        }

        /*
         * 必须实际写入内存，否则 malloc 可能只是分配虚拟地址，
         * 不一定真正占用物理内存。
         */
        memset(p, 0xAB, BLOCK_SIZE);

        void **new_blocks = realloc(blocks, sizeof(void *) * (count + 1));
        if (new_blocks == NULL) {
            perror("realloc");
            free(p);
            break;
        }

        blocks = new_blocks;
        blocks[count] = p;
        count++;

        printf("allocated %zu MB\n", count);
        fflush(stdout);

        usleep(50000);
    }

    /*
     * 防止程序正常退出后内存立刻释放。
     * 如果 cgroup 限制没生效，可以方便观察。
     */
    sleep(1000);

    for (size_t i = 0; i < count; i++) {
        free(blocks[i]);
    }
    free(blocks);

    return 0;
}