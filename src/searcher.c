#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h> 
#include "common.h"
#include "search_name.h"
#include "search_publisher.h"

int fd_query, fd_response;

int main(){
    int r, count;
    Query query;
    OffsetList offsets;
    Videojuego temp;   // <-- un solo buffer temporal
    struct timespec start, end;
    double totalTime;

    offsets.positions = NULL;
    offsets.size = 0;

    //fifo query para consultas
    //fifo response para respuestas

    r = mkfifo("/tmp/fifo_query", 0666);
    if(r < 0 && errno != EEXIST){
        perror("Error creando fifo_query");
        exit(1);
    }

    r = mkfifo("/tmp/fifo_response", 0666);

    if(r < 0 && errno != EEXIST){
        perror("Error creando fifo_response");
        exit(1);
    }

    fd_query = open("/tmp/fifo_query", O_RDONLY);//se abre para lectura
    if(fd_query < 0){
        perror("Error abriendo fifo_query");
        exit(-1);
    }

    fd_response = open("/tmp/fifo_response", O_WRONLY);//se abre para escritura
    if(fd_response < 0){
        perror("Error abriendo fifo_response");
        exit(-1);
    }

    FILE *fcsv = fopen(CSV_FILE, "r");//se abre el csv
    uint32_t *dir_name = cargarDirectorioName(DIR_NAME_FILE);//se abre y carga en ram el dir name
    FILE *fidx_name = fopen(IDX_NAME_FILE, "rb");//se abre el indice name

    uint32_t *dir_publ = cargarDirectorioPublisher(DIR_PUBL_FILE);//se abre y carga dir publisher
    FILE *fidx_publ = fopen(IDX_PUBL_FILE, "rb");//se carga el index de publisher

    if(!fidx_name || !fidx_publ || !fcsv){//se verifica que todo se abra correctamente
        perror("fopen");
        free(dir_name);
        free(dir_publ);
        if(fidx_name) fclose(fidx_name);
        if(fidx_publ) fclose(fidx_publ);
        if(fcsv) fclose(fcsv);
        return 1;
    }
    
    while(1){
        r = readFull(fd_query, &query, sizeof(query));//se lee una query (que venga desde gui)

        if(r == 0){
            printf("Terminando proceso de búsqueda...\n");
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        normalizarCadena(query.criteria);
        
        //si query=0, se busca por nombre
        //si query=1, se busca por publisher
        if(query.type == 0){
            offsets = buscarEnIndiceName(query.criteria, dir_name, fidx_name);
        } else if(query.type == 1){
            offsets = buscarEnIndicePublisher(query.criteria, dir_publ, fidx_publ);
        } else {
            offsets.positions = NULL;
            offsets.size = 0;
        }

        count = offsets.size;
        writeFull(fd_response, &count, sizeof(int)); //Enviar cantidad de resultados encontrados

        //Enviamos los resultados encontrados uno por uno para evitar sobrecargar la memoria 
        for(int i = 0; i < count; i++){
            temp = getRegisterFromCSV(offsets.positions[i], fcsv);
            writeFull(fd_response, &temp, sizeof(Videojuego));
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        totalTime = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        printf("Tiempo de búsqueda: %.6f segundos\n", totalTime);
        printf("Resultados encontrados: %i\n", count);
        fflush(stdout);

        if(offsets.positions){
            free(offsets.positions);
            offsets.positions = NULL;
            offsets.size = 0;
        }
    }

    free(dir_name);
    free(dir_publ);
    fclose(fidx_name);
    fclose(fidx_publ);
    fclose(fcsv);
    close(fd_query);
    close(fd_response);

    return 0;
}