#define _XOPEN_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <error.h>
#include <sys/mman.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <hwloc.h>
#ifdef _OPENMP
#include <omp.h>
#endif

struct config {
    unsigned int node;
    unsigned int node_set;
    unsigned int continuously;
} config;

char * now () {
    char *ret = (char *) malloc(200);
    assert(ret != NULL);

    time_t t = time(NULL);
    assert(t != ((time_t) -1));

    struct tm *tmp = localtime(&t);
    strftime(ret, 200, "%F %T", tmp);
    return ret;
}

void logmsg (const char *fmt, ...) {
    char *n = now();
    (void) fprintf(stderr, "%s: ", n);
    free(n);

    va_list args;
    va_start(args, fmt);
    (void) vfprintf(stderr, fmt, args);
    fflush(stderr);
    va_end(args);
}

static hwloc_topology_t topology = NULL;
long pagesize = 0x0;

char * mk_alt (size_t size) {
    // 1010...1010
    unsigned long int pattern = 0x0;
    for (size_t x = 0; x < (0x8 * sizeof(pattern)); x++) {
        pattern = (pattern << 1) | (x % 2);
    }

    char *ptr = (char *) malloc(size);
    assert(ptr != NULL);
    for (size_t i = 0; i < size; i += sizeof(pattern)) {
        memset(ptr + i, pattern, sizeof(pattern));
    }
    return ptr;
}

void get_topology () {
    assert(hwloc_topology_init(&topology) == 0);
    assert(hwloc_topology_load(topology) == 0);
}

void del_topology () {
    hwloc_topology_destroy(topology);
}

unsigned int get_nodes () {
    return hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NODE);
}

void test_node (const unsigned int node, const unsigned int iteration_size) {
    // Get the NUMA node
    hwloc_obj_t obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NODE, node);
    if (obj == NULL) {
        logmsg("No NUMA nodes on this machine. Doing whole system.\n");
        obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_MACHINE, 0x0);
        if (obj == NULL) {
            logmsg("Couldn't locate system information\n");
            exit(EXIT_FAILURE);
        }
    }
    // Bind to the NUMA node CPUs
    hwloc_set_cpubind(topology, obj->cpuset, HWLOC_CPUBIND_STRICT);

    // Find how much RAM the node has
    size_t ram = obj->memory.local_memory;
    logmsg("NUMA node %u has %zi bytes of RAM\n", node, ram);

    logmsg("Allocating memory...\n");
    char *m = (char *) hwloc_alloc_membind_nodeset(topology, ram, obj->nodeset, HWLOC_MEMBIND_DEFAULT, 0);
    assert(m != NULL);
    logmsg("Done\n");

    logmsg("Locking memory...\n");
    if(mlock(m, ram) == -1) {
        perror("mlock() failed");
        exit(EXIT_FAILURE);
    }
    logmsg("Done\n");

    char *alt = mk_alt(pagesize);
    // Do tests
    logmsg("Writing pattern...\n");

    {
        unsigned int j = 0;
#pragma omp parallel for private(j)
        for (size_t i = 0; i < ram; i += pagesize) {
            memcpy(m + i, alt, pagesize);
            if (j % iteration_size == 0) {
#ifdef _OPENMP
#pragma omp critical
                logmsg("Iteration %u complete (%u) in thread %i\n", j, i, omp_get_thread_num());
#else
                logmsg("Iteration %u complete (%u)\n", j, i);
#endif
            }
            j++;
        }
    }
    logmsg("Done\n");

    logmsg("Checking pattern...\n");
    {
#pragma omp parallel for
        for (size_t i = 0; i < ram; i += pagesize) {
            assert(memcmp(m + i, alt, pagesize) == 0);
        }
    }
    logmsg("Done\n");

    logmsg("Freeing memory\n");
    hwloc_free(topology, m, ram);

    // Unbind cores
    if (1) {
        obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_MACHINE, 0x0);
        assert(obj != NULL);
        hwloc_set_cpubind(topology, obj->cpuset, HWLOC_CPUBIND_STRICT);
    }

    logmsg("Done\n");
}

void test_nodes () {
    unsigned int iteration = 0x0;
    unsigned int nodes = get_nodes();
    unsigned int mem_iteration = 1024 * 1024;
    logmsg("There are %u NUMA nodes present\n", nodes);

    do {
        if (config.continuously)
            logmsg("Continuous iteration %u\n", iteration++);

        if (config.node_set) {
            unsigned int i = config.node;
            if (i > nodes) {
                logmsg("NUMA node %u isn't present (only have %u NUMA nodes)", i, nodes);
                return;
            }
            logmsg("Testing NUMA node %u\n", i);
            test_node(i, mem_iteration);
            logmsg("Done\n");
        }
        else {
            if (nodes == 0x0) {
                logmsg("Testing system\n");
                test_node(0x0, mem_iteration);
                logmsg("Done\n");
            }
            else {
                for (unsigned int i = 0; i < nodes; i++) {
                    logmsg("Testing node %u\n", i);
                    test_node(i, mem_iteration);
                    logmsg("Done\n");
                }
            }
        }
    } while (config.continuously);
}

void parse_args(int argc, char **argv) {
    int opt;

    config.node = 0x0;
    config.node_set = 0x0;
    config.continuously = 0x0;

    while ((opt = getopt(argc, argv, "cn:?h")) != -1) {
        switch (opt) {
            case 'n':
                config.node = strtol(optarg, (char **) NULL, 10);
                config.node_set = 0x1;
                break;
            case 'c':
                config.continuously = 0x1;
                break;
            case '?':
            case 'h':
            default:
                fprintf(stderr, "Usage: %s [-n node] [-c]\n", argv[0]);
                exit(EXIT_FAILURE);
                break;
        }
    }
}

void dump_config () {
    logmsg("config.node: %u\n", config.node);
    logmsg("config.node_set: %u\n", config.node_set);
    logmsg("config.continuously: %u\n", config.continuously);
}

int main(int argc, char **argv) {
    pagesize = sysconf(_SC_PAGESIZE);

    parse_args(argc, argv);

    get_topology();
    test_nodes();
    del_topology();
}
