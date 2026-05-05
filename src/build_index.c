#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

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
/*si un nombre aparece varias veces en el csv, los offsets se guardaran en este vector*/
static void offsetVecPush(OffsetVec *v, uint32_t offset){
    if(v->size == v->capacity){//se verifica si esta lleno
        uint32_t nuevaCap = (v->capacity == 0) ? 1 : v->capacity * 2;//se duplica la capacidad, si esta vacio se asigna 1
        uint32_t *nuevo = (uint32_t*)realloc(v->data, nuevaCap * sizeof(uint32_t));//se hace el nuevo arreglo cargando los datos q ya se tenian
        if(!nuevo){
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        v->data = nuevo;
        v->capacity = nuevaCap;//se actualizan los nuevos valores
    }
    v->data[v->size++] = offset;//se inserta el nuevo offset si no esta lleno
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
    unsigned long idx = hashString(key, NUM_BUCKETS);//se mira en q bucket cae
    BuildNode *cur = hm->buckets[idx];

    while(cur){
        if(strcmp(cur->key, key) == 0){ //Comparar strings, si la key ya existe, simplemente se añade el nuevo offset
            offsetVecPush(&cur->offsets, offset);
            return;
        }
        cur = cur->next;
    }
    //si no existe, crea un bucket nuevo y lo añade
    BuildNode *nuevo = (BuildNode*)malloc(sizeof(BuildNode));
    if(!nuevo){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    nuevo->key = (char*)malloc(strlen(key) + 1);//se crea el espacio en memoria para la llave
    if(!nuevo->key){
        perror("malloc");
        free(nuevo);
        exit(EXIT_FAILURE);
    }

    strcpy(nuevo->key, key);//se copia la llave al nuevo nodo
    offsetVecInit(&nuevo->offsets);//se crea el vecotr de offsets
    offsetVecPush(&nuevo->offsets, offset);//se actualiza con el primer offset

    //Insertar al inicio
    nuevo->next = hm->buckets[idx];
    hm->buckets[idx] = nuevo;
    hm->size++;
}


/* Guarda el índice en dos archivos binarios:
   1) rutaDir: directorio de buckets
   2) rutaIdx: entradas reales del índice */
static void guardarIndiceBinario(BuildHashMap *hm, const char *rutaDir, const char *rutaIdx){
    FILE *fdir = fopen(rutaDir, "wb");//archivo directorio
    FILE *fidx = fopen(rutaIdx, "wb");//archivo index

    if(!fdir || !fidx){
        perror("fopen index");
        if(fdir) fclose(fdir);
        if(fidx) fclose(fidx);
        exit(EXIT_FAILURE);
    }

    uint32_t *dir = (uint32_t*)malloc(NUM_BUCKETS * sizeof(uint32_t));//arreglo de direcciones tamano buckets, indica en que posicion
    //esta la primera entrada de ese bucket o invalid offset

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
        uint32_t primeroBucket = INVALID_OFFSET;//se tiene la cabeza del bucket
        uint32_t prevEntryOffset = INVALID_OFFSET;//se toma el bucket anterior

        while(cur){
            long entryPosLong = ftell(fidx);//posicion del puntero dentro del archivo
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
                if(fseek(fidx, prevEntryOffset, SEEK_SET) != 0){//volver a entrada anterior para escribirle offset con nueva entrada
                    perror("fseek patch");
                    free(dir);
                    fclose(fdir);
                    fclose(fidx);
                    exit(EXIT_FAILURE);
                }

                uint32_t next = entryPos;
                fwrite(&next, sizeof(uint32_t), 1, fidx);//se sobreescribe el nextEntry del previo, para "simular" el chaining del hm

                if(fseek(fidx, 0, SEEK_END) != 0){
                    perror("fseek end");
                    free(dir);
                    fclose(fdir);
                    fclose(fidx);
                    exit(EXIT_FAILURE);
                }
            }

            DiskEntryHeader h;
            h.nextEntry = INVALID_OFFSET;//como no sabemos si existe un siguiente bucket, lo pasamos como invalid offset para manejarlo con facilidad
            h.keyLen = (uint32_t)strlen(cur->key);
            h.countOffsets = cur->offsets.size;

            /* Escribimos la cabecera */
            fwrite(&h, sizeof(DiskEntryHeader), 1, fidx);

            /* Escribimos la llave */
            fwrite(cur->key, sizeof(char), h.keyLen, fidx);

            /* Escribimos offsets */
            fwrite(cur->offsets.data, sizeof(uint32_t), h.countOffsets, fidx);
            
            //Para cada llave se escribe en el archivo
            //DiskEntryHeader: -nextEntry -keylen -countOffsets
            //key: Valve
            //arreglo de offsets:[100,200,3]

            //al hacer fwrite, se mueve la cantidad de bytes necesaria, por lo que al hacer ftell, obtenemos la posicion actual donde estamos escribiendo

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
        long pos = ftell(f);//se extrae la direccion byte en que empieza el registro(offset)
        if(pos < 0) break;

        if(!fgets(linea, sizeof(linea), f)) break;

        char campoPublishers[MAX_STRING_LEN * 8];

        if(extraerCampoCSV(linea, COL_PUBLISHERS, campoPublishers, sizeof(campoPublishers))){//se extrae el valor o valores de la columna publishers 
            normalizarCadena(campoPublishers);

            if(campoPublishers[0] != '\0'){
                char publishers[MAX_CANT][MAX_STRING_LEN];
                int n = dividirCampo(campoPublishers, publishers, MAX_CANT, true);//se divide el campo por publishers individuales

                for(int i = 0; i < n; i++){
                    if(publishers[i][0] != '\0'){
                        insertarBuild(hm, publishers[i], (uint32_t)pos);//se inserta cada publisher distinto con el mismo offset
                    }
                }
            }
        }
    }

    fclose(f);
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
        long pos = ftell(f);//se extrae la direccion byte en que empieza el registro(offset)
        if(pos < 0) break;

        if(!fgets(linea, sizeof(linea), f)) break;

        char name[MAX_STRING_LEN];
        if(extraerCampoCSV(linea, COL_NAME, name, sizeof(name))){//se extrae el valor de la columna name 
            normalizarCadena(name);

            if(name[0] != '\0'){
                insertarBuild(hm, name, (uint32_t)pos);//se inserta en el hm temporal si no esta vacio
            }
        }
    }

    fclose(f);
}

int main(){
    printf("Construyendo indice por publisher.\n");
    BuildHashMap *hm = crearBuildMap();
    construirIndicePublisher(hm, CSV_FILE);
    guardarIndiceBinario(hm, DIR_PUBL_FILE, IDX_PUBL_FILE);
    liberarBuildMap(hm);

    printf("Indice por publisher construido correctamente.\n");
    printf("\n");

    
    printf("Construyendo indice por name.\n");
    BuildHashMap *hm2 = crearBuildMap();
    construirIndiceName(hm2, CSV_FILE);
    guardarIndiceBinario(hm2, DIR_NAME_FILE, IDX_NAME_FILE);
    liberarBuildMap(hm2);

    printf("Indice por name construido correctamente.\n");
    return 0;
}