#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "helpers.h"

#define SYS_IMPOSTER_PREAD64 445

long lookup_bpf(int db_fd, struct Query *query) {
    /* Set up buffers and query */
    char *buf = aligned_alloc(0x1000, 0x1000);
    char *scratch = aligned_alloc(0x1000, 0x1000);
    memset(buf, 0, 0x1000);
    memset(scratch, 0, 0x1000);

    struct Query *scratch_query = (struct Query *) scratch;
    scratch_query->key = query->key;

    /* Syscall to invoke BPF function that we loaded out-of-band previously */
    long ret = syscall(SYS_IMPOSTER_PREAD64, db_fd, buf, scratch, BLK_SIZE, 0);
    *query = *scratch_query;

    free(buf);
    free(scratch);
    return ret;
}

/* Helper function that terminates the program is pread fails */
void checked_pread(int fd, void *buf, size_t size, long offset) {
    ssize_t bytes_read = pread(fd, buf, size, offset);
    if (bytes_read < 0) {
        perror("checked_pread: ");
        exit(1);
    }
    if (bytes_read != size) {
        fprintf(stderr, "partial read %ld bytes of Node\n", bytes_read);
        exit(1);
    }
}

/**
 * Finds the file offset for the next node to traverse to in the B+ tree
 * @param key
 * @param node
 * @return B+ tree encoded byte offset into the db file
 */
ptr__t nxt_node(unsigned long key, Node *node) {
    for (size_t i = 1; i < node->num; ++i) {
        if (key < node->key[i]) {
            return node->ptr[i - 1];
        }
    }
    /* Key wasn't smaller than any of node->key[x], so take the last ptr */
    return node->ptr[node->num - 1];
}

/**
 * Verifies that [key] exists in the leaf [node]
 * @param key
 * @param node
 * @return 1 if [key] exists, else 0
 */
int key_exists(unsigned long const key, Node const *node) {
    for (int i = 0; i < node->num; ++i) {
        if (node->key[i] == key) {
            return 1;
        }
    }
    return 0;
}