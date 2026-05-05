#include "common.h"
#include "common_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


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
    BuildHashMap *hm = crearBuildMap();
    construirIndiceName(hm, CSV_FILE);
    guardarIndiceBinario(hm, DIR_NAME_FILE, IDX_NAME_FILE);
    liberarBuildMap(hm);

    printf("Indice por name construido correctamente.\n");
    return 0;
}