#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>//tipos exactos como uint32_t
#include <stddef.h>//size_t
#include <stdio.h>//archivos File
#include <stdbool.h>
#include <unistd.h>

/* Cantidad de buckets del hash.
   65536 = 2^16, buen balance entre memoria y colisiones */
#define NUM_BUCKETS 65536UL

/* Valor especial para indicar que un bucket no tiene entradas */
#define INVALID_OFFSET 0xFFFFFFFFU

/* Columna del CSV donde está "name" */
#define COL_NAME 2

/* Columna del CSV donde está "publishers" */
#define COL_PUBLISHERS 21

#define MAX_STRING_LEN 256 //Nombre, desarrollador, etc
#define MAX_LINK_LEN 1024 //URL de imagen, sitio web
#define MAX_TEXT_LEN 4096 //Para descripciones
#define MAX_LINE_LEN 8192 //Linea del CSV

#define MAX_RESULTS 2048 //Máxima cantidad de resultados que podemos obtener en una busqueda 
#define MAX_CANT 16 //Maximo numero de elementos en arreglos como plataformas

typedef struct {
    char name[MAX_STRING_LEN]; // 2
    char release_date[15]; //Formato YYYY-MM-DD 3
    char background_image[MAX_LINK_LEN]; // 4
    float rating; // 5 
    int ratings_count; // 7
    int added; // 9 
    int playtime; // 10
    int reviews_count; //13
    char platforms[MAX_CANT][MAX_STRING_LEN]; //16
    char stores[MAX_CANT][MAX_STRING_LEN]; //17
    char developers[MAX_CANT][MAX_STRING_LEN]; //18
    char genres[MAX_CANT][MAX_STRING_LEN]; //19
    char publishers[MAX_CANT][MAX_STRING_LEN]; //21
    char website[MAX_LINK_LEN]; //32
    char description[MAX_TEXT_LEN]; //41
} Videojuego;

/*
Es necesario especificar si queremos buscar por nombre (tipo 0) o por
distribuidora (tipo 1) al hacer una solicitud
*/
typedef struct{
    int type; //0 para name, 1 para publisher
    char criteria[MAX_STRING_LEN];//llave
} Query;

#define CSV_FILE "dataset/rawg-games-dataset.csv"
#define DIR_NAME_FILE "index/name_dir.bin"
#define IDX_NAME_FILE "index/name_index.bin"
#define DIR_PUBL_FILE "index/publisher_dir.bin"
#define IDX_PUBL_FILE "index/publisher_index.bin"

/* Vector dinámico de offsets.
   Reemplaza la lista enlazada de offsets para ahorrar memoria
   y facilitar escritura binaria. */
typedef struct OffsetVec {
    uint32_t *data;//puntero al arreglo de offsets
    uint32_t size;//tamano real
    uint32_t capacity;//capacidad del arreglo
} OffsetVec;
//en la construccion del indices, una key puede aparecer varias veces: un mismo publisher puede salir en muchos registros


/* Nodo temporal del hashmap usado SOLO durante la construcción
   del índice. */
typedef struct BuildNode {
    char *key;                 /* name normalizado */
    OffsetVec offsets;         /* offsets del CSV para esta llave, todas las posiciones del CSV asociadas a esta llave*/
    struct BuildNode *next;    /* chaining para colisiones */
} BuildNode;

/* Cabecera de cada entrada guardada en el archivo binario.
   Después de esta cabecera, se escriben:
   - la llave (key)
   - el arreglo de offsets */
typedef struct {
    uint32_t nextEntry;      /* posición de la siguiente entrada del bucket en el archivo */
    uint32_t keyLen;         /* longitud de la llave */
    uint32_t countOffsets;   /* cantidad de offsets asociados */
} DiskEntryHeader;

/*Struct que retornarán las funciones de búsqueda 
   positions : Resultados encontrados (posiciones en CSV)
   size : Cantidad de resultados encontrados */
typedef struct{
    uint32_t *positions;
    uint32_t size;
} OffsetList;

/* Función hash para strings */
unsigned long hashString(const char *str, unsigned long B);

/* Normaliza una cadena:
   - quita \r y \n
   - quita espacios al inicio y al final
   - pasa a minúsculas
   - quita comillas externas */
void normalizarCadena(char *s);

/* Extrae un campo del CSV respetando comillas.
   Devuelve 1 si pudo extraer el campo, 0 si no. */
int extraerCampoCSV(const char *linea, int indiceCampo, char *salida, size_t maxSalida);

int dividirCampo(const char *campo, char arr[][MAX_STRING_LEN], int max, bool normal);

/*Construye un struct Videojuego a partir de una posición en el CSV con los campos de interés*/
Videojuego getRegisterFromCSV(uint32_t position, FILE *file);

/*Realiza la escritura del contenido de un buffer por bloques en la tubería*/
ssize_t writeFull(int fd, void *buf, size_t size);

/*Realiza la lectura de la tubería y lo guarda dentro de un buffer por bloques*/
ssize_t readFull(int fd, void *buf, size_t size);

#endif