#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Hashmap temporal para construir el índice */
typedef struct {
    BuildNode **buckets;
    size_t size; /* cantidad de llaves distintas */
} BuildHashMap;

/* Inicializa vector dinámico de offsets */
static void offsetVecInit(OffsetVec *v){
    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
}

/* Agrega un offset al vector, creciendo con realloc si es necesario */
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

/* Libera el hashmap temporal completo */
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

/* Inserta (key, offset) en el hashmap temporal.
   Si la key ya existe, solo agrega el offset. */
static void insertarBuild(BuildHashMap *hm, const char *key, uint32_t offset){
    unsigned long idx = hashString(key, NUM_BUCKETS);
    BuildNode *cur = hm->buckets[idx];

    while(cur){
        if(strcmp(cur->key, key) == 0){ //Comparar strings
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

    //Insertar al inicio
    nuevo->next = hm->buckets[idx];
    hm->buckets[idx] = nuevo;
    hm->size++;
}

/* Recorre el CSV y construye el índice temporal por name */
static void construirIndiceName(BuildHashMap *hm, const char *rutaCSV){
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

        char name[MAX_STRING_LEN];
        if(extraerCampoCSV(linea, COL_NAME, name, sizeof(name))){
            normalizarCadena(name);

            if(name[0] != '\0'){
                insertarBuild(hm, name, (uint32_t)pos);
            }
        }
    }

    fclose(f);
}

/* Guarda el índice en dos archivos binarios:
   1) rutaDir: directorio de buckets
   2) rutaIdx: entradas reales del índice */
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

    /* Inicialmente todos los buckets están vacíos */
    for(size_t i = 0; i < NUM_BUCKETS; i++){
        dir[i] = INVALID_OFFSET;
    }

    /* Recorremos bucket por bucket */
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

            /* Si es la primera entrada del bucket, guardamos su posición */
            if(primeroBucket == INVALID_OFFSET){
                primeroBucket = entryPos;
            }

            /* Si ya había una entrada previa en el bucket, parchamos su nextEntry
               para que apunte a esta nueva entrada */
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

            /* Escribimos la cabecera */
            fwrite(&h, sizeof(DiskEntryHeader), 1, fidx);

            /* Escribimos la llave */
            fwrite(cur->key, sizeof(char), h.keyLen, fidx);

            /* Escribimos offsets */
            fwrite(cur->offsets.data, sizeof(uint32_t), h.countOffsets, fidx);

            prevEntryOffset = entryPos;
            cur = cur->next;
        }

        dir[i] = primeroBucket;
    }

    /* Guardamos el directorio completo */
    fwrite(dir, sizeof(uint32_t), NUM_BUCKETS, fdir);

    free(dir);
    fclose(fdir);
    fclose(fidx);
}

int main(){
    BuildHashMap *hm = crearBuildMap();
    construirIndiceName(hm, CSV_FILE);
    guardarIndiceBinario(hm, DIR_NAME_FILE, IDX_NAME_FILE);
    liberarBuildMap(hm);

    printf("Indice por name construido correctamente.\n");
    return 0;
}