#include "common.h"
#include "common_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

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

int main(){
    BuildHashMap *hm = crearBuildMap();
    construirIndicePublisher(hm, CSV_FILE);
    guardarIndiceBinario(hm, DIR_PUBL_FILE, IDX_PUBL_FILE);
    liberarBuildMap(hm);

    printf("Indice por publisher construido correctamente.\n");
    return 0;
}