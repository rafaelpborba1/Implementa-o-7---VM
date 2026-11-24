#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 256
#define NUM_PAGES 256
#define NUM_FRAMES 128
#define TLB_SIZE 16
#define OFFSET_MASK 0xFF
#define PAGE_MASK 0xFF
#define BACKING_STORE_FILE "BACKING_STORE.bin"
#define OUTPUT_FILE "correct.txt"

typedef enum { ALG_FIFO, ALG_LRU } ReplacementAlg;

typedef struct {
    int frame_number;
    int valid;
    unsigned long last_used;
} PageTableEntry;

static PageTableEntry page_table[NUM_PAGES];
static signed char physical_memory[NUM_FRAMES][PAGE_SIZE];
static int frame_to_page[NUM_FRAMES];

static int next_free_frame = 0;
static int fifo_victim_frame = 0;
static unsigned long access_clock = 0;
static int page_faults = 0;

static int parse_algorithm(const char *arg, ReplacementAlg *out) {
    if (strcmp(arg, "fifo") == 0) {
        *out = ALG_FIFO;
        return 1;
    }
    if (strcmp(arg, "lru") == 0) {
        *out = ALG_LRU;
        return 1;
    }
    return 0;
}

static int parse_address_line(const char *line, int *address_out) {
    const char *p = line;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p == '\0') {
        return 0;
    }

    errno = 0;
    char *endptr = NULL;
    long val = strtol(p, &endptr, 10);

    if (errno != 0 || endptr == p) {
        return -1;
    }

    while (*endptr != '\0' && isspace((unsigned char)*endptr)) {
        endptr++;
    }
    if (*endptr != '\0') {
        return -1;
    }

    if (val < 0 || val > INT_MAX) {
        return -1;
    }

    *address_out = (int)val;
    return 1;
}

static int select_victim_frame(ReplacementAlg page_alg) {
    if (next_free_frame < NUM_FRAMES) {
        return next_free_frame++;
    }

    if (page_alg == ALG_FIFO) {
        int victim = fifo_victim_frame;
        fifo_victim_frame = (fifo_victim_frame + 1) % NUM_FRAMES;
        return victim;
    }

    int victim = 0;
    unsigned long oldest_time = ULONG_MAX;
    for (int f = 0; f < NUM_FRAMES; f++) {
        int p = frame_to_page[f];
        if (p != -1 && page_table[p].last_used < oldest_time) {
            oldest_time = page_table[p].last_used;
            victim = f;
        }
    }
    return victim;
}

static int handle_page_fault(int page_number, FILE *backing_store, ReplacementAlg page_alg) {
    page_faults++;

    int target_frame = select_victim_frame(page_alg);

    int evicted_page = frame_to_page[target_frame];
    if (evicted_page != -1) {
        page_table[evicted_page].valid = 0;
        page_table[evicted_page].frame_number = -1;
    }

    long disk_offset = (long)page_number * PAGE_SIZE;
    if (fseek(backing_store, disk_offset, SEEK_SET) != 0) {
        return 0;
    }

    size_t bytes_read = fread(physical_memory[target_frame], 1, PAGE_SIZE, backing_store);
    if (bytes_read != PAGE_SIZE) {
        return 0;
    }

    page_table[page_number].frame_number = target_frame;
    page_table[page_number].valid = 1;
    frame_to_page[target_frame] = page_number;

    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <arquivo_enderecos> <fifo|lru> <fifo|lru>\n", argv[0]);
        return EXIT_FAILURE;
    }

    ReplacementAlg page_alg;
    ReplacementAlg tlb_alg;

    if (!parse_algorithm(argv[2], &page_alg)) {
        fprintf(stderr, "Erro: algoritmo de substituicao de pagina invalido '%s'. Use 'fifo' ou 'lru'.\n", argv[2]);
        return EXIT_FAILURE;
    }

    if (!parse_algorithm(argv[3], &tlb_alg)) {
        fprintf(stderr, "Erro: algoritmo de substituicao da TLB invalido '%s'. Use 'fifo' ou 'lru'.\n", argv[3]);
        return EXIT_FAILURE;
    }

    (void)tlb_alg;

    FILE *addr_file = fopen(argv[1], "r");
    if (addr_file == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo de enderecos '%s': %s\n",
                argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    FILE *backing_store = fopen(BACKING_STORE_FILE, "rb");
    if (backing_store == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir '%s': %s\n",
                BACKING_STORE_FILE, strerror(errno));
        fclose(addr_file);
        return EXIT_FAILURE;
    }

    FILE *out_file = fopen(OUTPUT_FILE, "w");
    if (out_file == NULL) {
        fprintf(stderr, "Erro: nao foi possivel criar o arquivo de saida '%s': %s\n",
                OUTPUT_FILE, strerror(errno));
        fclose(backing_store);
        fclose(addr_file);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < NUM_PAGES; i++) {
        page_table[i].valid = 0;
        page_table[i].frame_number = -1;
        page_table[i].last_used = 0;
    }

    for (int i = 0; i < NUM_FRAMES; i++) {
        frame_to_page[i] = -1;
    }

    char line[256];
    int total_addresses = 0;

    while (fgets(line, sizeof(line), addr_file) != NULL) {
        int logical_address = 0;
        int parse_status = parse_address_line(line, &logical_address);

        if (parse_status == 0) {
            continue;
        }
        if (parse_status < 0) {
            fprintf(stderr, "Erro: formatacao invalida no arquivo '%s': %s", argv[1], line);
            fclose(out_file);
            fclose(backing_store);
            fclose(addr_file);
            remove(OUTPUT_FILE);
            return EXIT_FAILURE;
        }

        total_addresses++;
        access_clock++;

        int page_number = (logical_address >> 8) & PAGE_MASK;
        int offset = logical_address & OFFSET_MASK;

        if (!page_table[page_number].valid) {
            if (!handle_page_fault(page_number, backing_store, page_alg)) {
                fprintf(stderr, "Erro: falha ao ler pagina %d de '%s'.\n", page_number, BACKING_STORE_FILE);
                fclose(out_file);
                fclose(backing_store);
                fclose(addr_file);
                remove(OUTPUT_FILE);
                return EXIT_FAILURE;
            }
        }

        page_table[page_number].last_used = access_clock;
        int frame_number = page_table[page_number].frame_number;
        int physical_address = (frame_number * PAGE_SIZE) + offset;
        signed char value = physical_memory[frame_number][offset];

        fprintf(out_file, "Virtual address: %d TLB: -1 Physical address: %d Value: %d\n",
                logical_address, physical_address, (int)value);
    }

    if (total_addresses == 0) {
        fprintf(stderr, "Erro: o arquivo '%s' nao contem enderecos validos.\n", argv[1]);
        fclose(out_file);
        fclose(backing_store);
        fclose(addr_file);
        remove(OUTPUT_FILE);
        return EXIT_FAILURE;
    }

    fclose(out_file);
    fclose(backing_store);
    fclose(addr_file);

    return EXIT_SUCCESS;
}