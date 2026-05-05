#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h> //Para read/write
#include <fcntl.h> //Para open
#include <sys/types.h> //Para mkfifo
#include <sys/stat.h> //Para mkfifo
#include "common.h"
#include "search_name.h"
#include "search_publisher.h"

int fd_query, fd_response;

int main(){
    int r, count;
    Query query;
    OffsetList offsets;
    Videojuego *results;
    results = malloc(sizeof(Videojuego) * MAX_RESULTS);
    if(!results){
        perror("Error alocando memoria");
        exit(-1);
    }

    //Inicializar offsets
    offsets.positions = NULL;
    offsets.size = 0;

    r = mkfifo("/tmp/fifo_query", 0666); //Fifo para solicitar busqueda
    if(r < 0 && errno != EEXIST){
        perror("Error creando fifo_query");
        exit(1);
    }

    r = mkfifo("/tmp/fifo_response", 0666); //Fifo para enviar resultados encontrados
    if(r < 0 && errno != EEXIST){
        perror("Error creando fifo_response");
        exit(1);
    }

    fd_query = open("/tmp/fifo_query", O_RDONLY);
    if(fd_query < 0){
        perror("Error abriendo fifo_query");
        exit(-1);
    }

    fd_response = open("/tmp/fifo_response", O_WRONLY);
    if(fd_response < 0){
        perror("Error abriendo fifo_response");
        exit(-1);
    }

    //Abriendo archivos para las búsquedas
    FILE *fcsv = fopen(CSV_FILE, "r");
    //Buscar con nombre
    uint32_t *dir_name = cargarDirectorioName(DIR_NAME_FILE);
    FILE *fidx_name = fopen(IDX_NAME_FILE, "rb");

    //Buscar con distribuidora
    uint32_t *dir_publ = cargarDirectorioPublisher(DIR_PUBL_FILE);
    FILE *fidx_publ = fopen(IDX_PUBL_FILE, "rb");
    

    if(!fidx_name || !fcsv){
        perror("fopen");
        free(dir_name);
        if(fidx_name) fclose(fidx_name);
        if(fcsv) fclose(fcsv);
        return 1;
    }

    while(1){
        r = read(fd_query, &query, sizeof(query));

        if(r == 0){ //GUI cerrada
            printf("Terminando proceso de búsqueda...\n");
            break;
        }

        if(r < 0){
            perror("Error leyendo fifo_query");
            exit(-1);
        }

        normalizarCadena(query.criteria);
        if(query.type == 0){
            printf("Buscando nombre... %s\n", query.criteria);
        
            offsets = buscarEnIndiceName(query.criteria, dir_name, fidx_name);  
            count = offsets.size;

        } else if(query.type == 1){
            printf("Buscando distribuidora... %s\n", query.criteria);

            offsets = buscarEnIndicePublisher(query.criteria, dir_publ, fidx_publ);  
            count = offsets.size;

        }
        writeFull(fd_response, &count, sizeof(int));

        printf("DEBUG searcher count = %d\n", count);
        fflush(stdout);

        if(count > MAX_RESULTS) count = MAX_RESULTS;

        for(int i = 0; i < count; i++){
            results[i] = getRegisterFromCSV(offsets.positions[i], fcsv);
        }
        /*NOTA. El buffer de la tubería es de solo 65536 bytes,
        así que debemos enviar los resultados por partes
        */
        if(count > 0){
            writeFull(fd_response, results, count * sizeof(Videojuego));
        }

    }

    if(offsets.positions) free(offsets.positions);
    free(results);
    free(dir_name);
    fclose(fidx_name);
    fclose(fcsv);
    
    //close(fd_query);

    return 0;
}