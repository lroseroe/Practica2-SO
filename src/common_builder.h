#ifndef COMMONBUILDER_H
#define COMMONBUILDER_H

#include "common.h"
#include "common_builder.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

/* Hashmap temporal para construir el índice */
typedef struct {
    BuildNode **buckets;
    size_t size; /* cantidad de llaves distintas */
} BuildHashMap;

void offsetVecInit(OffsetVec *v);/* Inicializa vector dinámico de offsets */

void offsetVecPush(OffsetVec *v, uint32_t offset);/* Agrega un offset al vector, creciendo con realloc si es necesario */

BuildHashMap *crearBuildMap(void);

void liberarBuildMap(BuildHashMap *hm);

void insertarBuild(BuildHashMap *hm, const char *key, uint32_t offset);

void guardarIndiceBinario(BuildHashMap *hm, const char *rutaDir, const char *rutaIdx);



#endif