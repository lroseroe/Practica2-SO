#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Hashmap temporal para construir el índice */
typedef struct {
    BuildNode **buckets;
    size_t size;
} BuildHashMap;

/* Inicializa vector dinámico de offsets */
static void offsetVecInit(OffsetVec *v){
    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
}

/* Agrega un offset al vector */
static void offsetVecPush(OffsetVec *v, uint32_t offset){
    if(v->size == v->capacity){
        uint32_t nuevaCap = (v->capacity == 0) ? 1 : v->capacity * 2;
        uint32_t *nuevo = (uint32_t*)realloc(v->data, nuevaCap * sizeof(uint32_t));
        if(!nuevo){
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        v->data = nuevo;
        v->capacity = nuevaCap;
    }
    v->data[v->size++] = offset;
}

/* Crea el hashmap temporal */
static BuildHashMap *crearBuildMap(void){
    BuildHashMap *hm = (BuildHashMap*)malloc(sizeof(BuildHashMap));
    if(!hm){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    hm->buckets = (BuildNode**)calloc(NUM_BUCKETS, sizeof(BuildNode*));
    if(!hm->buckets){
        perror("calloc");
        free(hm);
        exit(EXIT_FAILURE);
    }

    hm->size = 0;
    return hm;
}

/* Libera el hashmap temporal */
static void liberarBuildMap(BuildHashMap *hm){
    if(!hm) return;

    for(size_t i = 0; i < NUM_BUCKETS; i++){
        BuildNode *cur = hm->buckets[i];
        while(cur){
            BuildNode *sig = cur->next;
            free(cur->key);
            free(cur->offsets.data);
            free(cur);
            cur = sig;
        }
    }

    free(hm->buckets);
    free(hm);
}

/* Inserta (publisher, offset) */
static void insertarBuild(BuildHashMap *hm, const char *key, uint32_t offset){
    unsigned long idx = hashString(key, NUM_BUCKETS);
    BuildNode *cur = hm->buckets[idx];

    while(cur){
        if(strcmp(cur->key, key) == 0){
            offsetVecPush(&cur->offsets, offset);
            return;
        }
        cur = cur->next;
    }

    BuildNode *nuevo = (BuildNode*)malloc(sizeof(BuildNode));
    if(!nuevo){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    nuevo->key = (char*)malloc(strlen(key) + 1);
    if(!nuevo->key){
        perror("malloc");
        free(nuevo);
        exit(EXIT_FAILURE);
    }

    strcpy(nuevo->key, key);
    offsetVecInit(&nuevo->offsets);
    offsetVecPush(&nuevo->offsets, offset);

    nuevo->next = hm->buckets[idx];
    hm->buckets[idx] = nuevo;
    hm->size++;
}

/* Construye índice por publishers */
static void construirIndicePublisher(BuildHashMap *hm, const char *rutaCSV){
    FILE *f = fopen(rutaCSV, "r");
    if(!f){
        perror("fopen csv");
        exit(EXIT_FAILURE);
    }

    char linea[MAX_LINE_LEN];

    /* saltar header */
    if(!fgets(linea, sizeof(linea), f)){
        fclose(f);
        return;
    }

    while(1){
        long pos = ftell(f);
        if(pos < 0) break;

        if(!fgets(linea, sizeof(linea), f)) break;

        char campoPublishers[MAX_STRING_LEN * 8];

        if(extraerCampoCSV(linea, COL_PUBLISHERS, campoPublishers, sizeof(campoPublishers))){
            normalizarCadena(campoPublishers);

            if(campoPublishers[0] != '\0'){
                char publishers[MAX_CANT][MAX_STRING_LEN];
                int n = dividirCampo(campoPublishers, publishers, MAX_CANT, true);

                for(int i = 0; i < n; i++){
                    if(publishers[i][0] != '\0'){
                        insertarBuild(hm, publishers[i], (uint32_t)pos);
                    }
                }
            }
        }
    }

    fclose(f);
}

/* Guarda el índice binario */
static void guardarIndiceBinario(BuildHashMap *hm, const char *rutaDir, const char *rutaIdx){
    FILE *fdir = fopen(rutaDir, "wb");
    FILE *fidx = fopen(rutaIdx, "wb");

    if(!fdir || !fidx){
        perror("fopen index");
        if(fdir) fclose(fdir);
        if(fidx) fclose(fidx);
        exit(EXIT_FAILURE);
    }

    uint32_t *dir = (uint32_t*)malloc(NUM_BUCKETS * sizeof(uint32_t));
    if(!dir){
        perror("malloc dir");
        fclose(fdir);
        fclose(fidx);
        exit(EXIT_FAILURE);
    }

    for(size_t i = 0; i < NUM_BUCKETS; i++){
        dir[i] = INVALID_OFFSET;
    }

    for(size_t i = 0; i < NUM_BUCKETS; i++){
        BuildNode *cur = hm->buckets[i];
        uint32_t primeroBucket = INVALID_OFFSET;
        uint32_t prevEntryOffset = INVALID_OFFSET;

        while(cur){
            long entryPosLong = ftell(fidx);
            if(entryPosLong < 0){
                perror("ftell");
                free(dir);
                fclose(fdir);
                fclose(fidx);
                exit(EXIT_FAILURE);
            }

            uint32_t entryPos = (uint32_t)entryPosLong;

            if(primeroBucket == INVALID_OFFSET){
                primeroBucket = entryPos;
            }

            if(prevEntryOffset != INVALID_OFFSET){
                if(fseek(fidx, prevEntryOffset, SEEK_SET) != 0){
                    perror("fseek patch");
                    free(dir);
                    fclose(fdir);
                    fclose(fidx);
                    exit(EXIT_FAILURE);
                }

                uint32_t next = entryPos;
                fwrite(&next, sizeof(uint32_t), 1, fidx);

                if(fseek(fidx, 0, SEEK_END) != 0){
                    perror("fseek end");
                    free(dir);
                    fclose(fdir);
                    fclose(fidx);
                    exit(EXIT_FAILURE);
                }
            }

            DiskEntryHeader h;
            h.nextEntry = INVALID_OFFSET;
            h.keyLen = (uint32_t)strlen(cur->key);
            h.countOffsets = cur->offsets.size;

            fwrite(&h, sizeof(DiskEntryHeader), 1, fidx);
            fwrite(cur->key, sizeof(char), h.keyLen, fidx);
            fwrite(cur->offsets.data, sizeof(uint32_t), h.countOffsets, fidx);

            prevEntryOffset = entryPos;
            cur = cur->next;
        }

        dir[i] = primeroBucket;
    }

    fwrite(dir, sizeof(uint32_t), NUM_BUCKETS, fdir);

    free(dir);
    fclose(fdir);
    fclose(fidx);
}

int main(){
    BuildHashMap *hm = crearBuildMap();
    construirIndicePublisher(hm, CSV_FILE);
    guardarIndiceBinario(hm, DIR_PUBL_FILE, IDX_PUBL_FILE);
    liberarBuildMap(hm);

    printf("Indice por publisher construido correctamente.\n");
    return 0;
}