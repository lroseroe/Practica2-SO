#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h> //Para read/write
#include <sys/types.h> 
#include <strings.h>
#include <time.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"
#include "search_name.h"
#include "search_publisher.h"

#define BACKLOG 8

int main(){
    int r, count, sockfd, sockfdc1;
    struct sockaddr_in server, cliente;
    socklen_t addrlen, addrlen_c;

    Query query;
    OffsetList offsets;
    Videojuego temp;   // <-- un solo buffer temporal
    struct timespec start, end;
    double totalTime;

    //Inicializar offsets
    offsets.positions = NULL;
    offsets.size = 0;

    // crear socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        perror("Error en socket");
        exit(-1);
    }

    server.sin_family = AF_INET;    //ipv4
    server.sin_port = htons(PORT);  //endianismo
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server.sin_zero), 8);
    addrlen = sizeof(struct sockaddr);
    //nombrar socket
    r = bind(sockfd, (struct sockaddr *)&server, addrlen);
    if(r == -1){
        perror("Error en bind");
        exit(-1);
    }

    r = listen(sockfd, BACKLOG);
    if(r == -1){
        perror("Error en listen");
        exit(-1);
    }
    addrlen_c = sizeof(struct sockaddr_in);
    sockfdc1 = accept(sockfd, (struct sockaddr *)&cliente, &addrlen_c);
    if(sockfdc1 == -1){
        perror("Error en accept");
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
        r = recvFull(sockfdc1, &query, sizeof(query));

        if(r == 0){ //GUI cerrada
            printf("Terminando proceso de búsqueda...\n");
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        normalizarCadena(query.criteria);
        //si query=0, se busca por nombre
        //si query=1, se busca por publisher
        if(query.type == 0){
            printf("Buscando nombre... %s\n", query.criteria);
            offsets = buscarEnIndiceName(query.criteria, dir_name, fidx_name);
        } else if(query.type == 1){
            printf("Buscando distribuidora... %s\n", query.criteria);
            offsets = buscarEnIndicePublisher(query.criteria, dir_publ, fidx_publ);
        } else {
            offsets.positions = NULL;
            offsets.size = 0;
        }

        count = offsets.size;
        sendFull(sockfdc1, &count, sizeof(int));

        //Enviamos los resultados encontrados uno por uno para evitar sobrecargar la memoria 
        for(int i = 0; i < count; i++){
            temp = getRegisterFromCSV(offsets.positions[i], fcsv);
            sendFull(sockfdc1, &temp, sizeof(Videojuego));
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        totalTime = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        printf("Tiempo de búsqueda: %.6f segundos\n", totalTime);
        printf("Resultados encontrados: %i\n", count);
        fflush(stdout);

        printf("DEBUG searcher count = %d\n", count);
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
    close(sockfdc1);
    close(sockfd);

    return 0;
}