#include "common.h"
#include <ctype.h>//tolower
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //Para read/write
#include <stdbool.h>
#include <errno.h>

/* Hash djb2 para strings */
unsigned long hashString(const char *str, unsigned long B){
    unsigned long hash = 5381;//numero de hash para djb2
    int c;

    while((c = (unsigned char)*str++)){
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    // x<<n = x*2**n
    //33 buena distribucion de hashes
    return hash % B;
}

/* Normalización de cadenas para que las búsquedas sean consistentes */
void normalizarCadena(char *s){
    if(s == NULL) return;

    /* quitar fin de línea */
    s[strcspn(s, "\r\n")] = '\0';
    //elimina espacio final con strcspn, se usan los escapes como caracteres prohibidos

    /* quitar espacios iniciales */
    char *inicio = s;
    while(*inicio && isspace((unsigned char)*inicio)) inicio++;

    if(inicio != s){
        memmove(s, inicio, strlen(inicio) + 1);
    }
    //se usa memmove para eliminar el primer caracter, memomove reasigna la memoria

    /* quitar espacios finales */
    int n = (int)strlen(s);
    while(n > 0 && isspace((unsigned char)s[n - 1])){
        s[n - 1] = '\0';
        n--;
    }

    /* pasar a minúsculas */
    for(int i = 0; s[i]; i++){
        s[i] = (char)tolower((unsigned char)s[i]);//ctype.h
    }

    /* quitar comillas exteriores */
    n = (int)strlen(s);
    if(n >= 2 && s[0] == '"' && s[n - 1] == '"'){
        memmove(s, s + 1, n - 2);
        s[n - 2] = '\0';
    }
}

/* Extrae la columna indicada de una línea CSV.
   Respeta comillas dobles para no cortar mal si hay comas dentro del campo. */
int extraerCampoCSV(const char *linea, int indiceCampo, char *salida, size_t maxSalida){
    int campoActual = 0;
    int dentroComillas = 0;
    size_t j = 0;

    for(size_t i = 0; linea[i] != '\0'; i++){//se recorre toda la linea del csv
        char c = linea[i];

        if(c == '"'){
            dentroComillas = !dentroComillas;//se alterna entre el estado estoy dentro de comillas o no
        }
        else if(c == ',' && !dentroComillas){//si se encuentra una coma dentro de comillas, se entiende que se estan separando columnas
            if(campoActual == indiceCampo){
                salida[j] = '\0';
                return 1;//si se lee la columna deseada, se termina el campo y se cierra el string
            }//sino se avanza al siguiente campo
            campoActual++;
            j = 0;
        }
        else{
            if(campoActual == indiceCampo){//solo copia los caracteres cuando esta en la columna que es de interes
                if(j + 1 < maxSalida){
                    salida[j++] = c;
                }
            }
        }
    }

    if(campoActual == indiceCampo){
        salida[j] = '\0';
        return 1;//se retorna con la salida correcta asi no se termine en coma(se toma el )
    }

    return 0;
}

/* Divide un campo usando '|' como separador.
   Guarda cada elemento en arr[i].
   Devuelve cuántos elementos encontró. */
int dividirCampo(const char *campo, char arr[][MAX_STRING_LEN], int max, bool normal){
    if(campo == NULL || campo[0] == '\0') return 0;//si esta vacio devuelve 0

    int count = 0;//n de elementos guardados
    int j = 0;//posicion dentro del string

    for(size_t i = 0; ; i++){//bucle "infinito" hasta stop manual, se hace para procesar hasta \0
        char c = campo[i];

        if(c == '|' || c == '\0'){//si encuentra un caracter separador o el final
            if(j > 0 && count < max){
                arr[count][j] = '\0';
                if(normal) normalizarCadena(arr[count]);
                
                if(arr[count][0] != '\0'){
                    count++;
                }
            }
            //se cierra el string, se incrementa el contador, se reinicia j 
            j = 0;

            if(c == '\0'){//si es el fin de la cadena, se termina el ciclo
                break;
            }
        }
        else{
            if(count < max && j < MAX_STRING_LEN - 1){//va construyendo la subcadena actual mientras no supere el valor maximo
                arr[count][j++] = c;
            }
        }
    }

    return count;//se devuelve el numero de elementos
}

/* Retorna una estructura Videojuego creada con campos de interés 
a partir de una posición en el CSV */
Videojuego getRegisterFromCSV(uint32_t position, FILE *file){
    int r, len;
    bool in_quotes = false; //Para evitar errores parseando el CSV
    char line[MAX_LINE_LEN], value[MAX_STRING_LEN];//line= se carga linea del CSV, value buffer auxiliar valor de una columna
    Videojuego vjuego;

    //SEEK_SET para que cuente los byts indicados por position desde el inicio del csv
    r = fseek(file, position, SEEK_SET); 
    if(r != 0){
        perror("Error en fseek()");
        exit(-1);
    }
    
    if(fgets(line, MAX_LINE_LEN, file) == NULL){//se lee linea del registro
        perror("Error leyendo linea");
        exit(-1);
    }

    int col = 0;
    int start = 0;
    int end = 0;

    for(int i = 0; line[i] != '\0'; i++){
        if(line[i] == '"'){ 
            in_quotes = !in_quotes; //se revisa que se este entre comillas
            continue;
        } 
        if(line[i] == ',' && !in_quotes){
            end = i; 

            if(line[start] == '"'){
                start++;
            }

            if(line[end - 1] == '"'){
                end--;
            }

            len = end - start;//longitud del campo
            strncpy(value, &line[start], len);//se copia en el buffer auxiliar
            value[len] = '\0';

            switch(col){//se mira que se quiere cargar del csv
                case 2: //name
                    strcpy(vjuego.name, value);
                    break;
                case 3: //release_date
                    strcpy(vjuego.release_date, value);
                    break;
                case 4: //background_image
                    strcpy(vjuego.background_image, value);
                    break;
                case 5: //rating
                    vjuego.rating = atof(value);
                    break;
                case 7: //ratings_count
                    vjuego.ratings_count = atoi(value);
                    break;
                case 9: //added
                    vjuego.added = atoi(value);
                    break;
                case 10: //playtime
                    vjuego.playtime = atoi(value);
                    break;
                case 13: //reviews_count
                    vjuego.reviews_count = atoi(value);
                    break;
                case 16: //platforms
                    dividirCampo(value, vjuego.platforms, MAX_CANT, false);
                    break;
                case 17: //stores
                    dividirCampo(value, vjuego.stores, MAX_CANT, false);
                    break;
                case 18: //developers
                    dividirCampo(value, vjuego.developers, MAX_CANT, false);
                    break;
                case 19: //genres
                    dividirCampo(value, vjuego.genres, MAX_CANT, false);
                    break;
                case 21: //publishers
                    dividirCampo(value, vjuego.publishers, MAX_CANT, false);
                    break;
                case 32: //website
                    strcpy(vjuego.website, value);
                    break;
                case 41: //description
                    strcpy(vjuego.description, value);
                    break;
            }

            col ++;//avanzamos de columnas
            start = i + 1;//mueve el inicio al caracter posterior a la coma
        }

    }

    return vjuego;
}

/*Función que escribe la información de un buffer por partes 
(Para garantizar que se envíe la información completa)*/
ssize_t writeFull(int fd, void *buf, size_t size){
    size_t total = 0;
    ssize_t r;

    while(total < size){
        // ((char*)buf) + total -> Para indicar que se debe escribir desde la cantidad de bytes deseada
        r = write(fd, ((char*)buf) + total, size - total); 
        if(r < 0){
            perror("Error en write");
            exit(-1);
        }

        total += r;
    }

    return r;
}

/*Función que lee la información y la guarda en un buffer por partes
(Para garantizar que se reciba la información completa)*/
ssize_t readFull(int fd, void *buf, size_t size){
    size_t total = 0;
    ssize_t r;

    while(total < size){
        r = read(fd, ((char*)buf) + total, size - total);
        if(r == 0) return 0;
            
        if(r < 0){
            perror("Error en read");
            exit(-1);
        }

        total += r;
    }

    return r;
}