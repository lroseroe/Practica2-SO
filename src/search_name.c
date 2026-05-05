#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "search_name.h"
#include "common.h"

/* Carga el directorio de buckets en memoria */
uint32_t *cargarDirectorioName(const char *rutaDir){
    FILE *f = fopen(rutaDir, "rb");
    if(!f){
        perror("fopen dir");
        exit(EXIT_FAILURE);
    }

    uint32_t *dir = (uint32_t*)malloc(NUM_BUCKETS * sizeof(uint32_t));
    if(!dir){
        perror("malloc dir");
        fclose(f);
        exit(EXIT_FAILURE);
    }

    size_t leidos = fread(dir, sizeof(uint32_t), NUM_BUCKETS, f);
    fclose(f);

    if(leidos != NUM_BUCKETS){
        fprintf(stderr, "Error leyendo directorio.\n");
        free(dir);
        exit(EXIT_FAILURE);
    }

    return dir;
}

/* Busca una consulta dentro del índice binario por name */
OffsetList buscarEnIndiceName(const char *query, const uint32_t *dir, FILE *fidx){
    unsigned long bucket = hashString(query, NUM_BUCKETS);
    uint32_t pos = dir[bucket];

    while(pos != INVALID_OFFSET){
        if(fseek(fidx, pos, SEEK_SET) != 0){
            perror("fseek index");
            exit(-1);
        }

        DiskEntryHeader h;
        if(fread(&h, sizeof(DiskEntryHeader), 1, fidx) != 1){
            fprintf(stderr, "Error leyendo cabecera del índice.\n");
            exit(-1);
        }

        char *key = (char*)malloc(h.keyLen + 1);
        if(!key){
            perror("malloc key");
            exit(-1);
        }

        if(fread(key, sizeof(char), h.keyLen, fidx) != h.keyLen){
            free(key);
            fprintf(stderr, "Error leyendo key.\n");
            exit(-1);
        }
        key[h.keyLen] = '\0';

        if(strcmp(key, query) == 0){
            uint32_t *offsets = (uint32_t*)malloc(h.countOffsets * sizeof(uint32_t));
            if(!offsets){
                perror("malloc offsets");
                free(key);
                exit(-1);
            }

            if(fread(offsets, sizeof(uint32_t), h.countOffsets, fidx) != h.countOffsets){
                fprintf(stderr, "Error leyendo offsets.\n");
                free(offsets);
                free(key);
                exit(-1);
            }

            OffsetList list;
            list.positions = offsets;
            list.size = h.countOffsets;

            free(key);
            return list;
        }

        free(key);
        pos = h.nextEntry;
    }

    OffsetList empty = {NULL, 0};
    return empty;
}

