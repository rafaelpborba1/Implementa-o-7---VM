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
        return 0; /* Linha em branco */
    }

    errno = 0;
    char *endptr = NULL;
    long val = strtol(p, &endptr, 10);

    if (errno != 0 || endptr == p) {
        return -1; /* Nao e numero valido */
    }

    while (*endptr != '\0' && isspace((unsigned char)*endptr)) {
        endptr++;
    }
    if (*endptr != '\0') {
        return -1; /* Caracteres invalidos apos o numero */
    }

    if (val < 0 || val > INT_MAX) {
        return -1; /* Endereco negativo ou fora do limite */
    }

    *address_out = (int)val;
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