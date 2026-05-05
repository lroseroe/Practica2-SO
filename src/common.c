#include "common.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //Para read/write
#include <stdbool.h>
#include <errno.h>

/* Hash djb2 para strings */
unsigned long hashString(const char *str, unsigned long B){
    unsigned long hash = 5381;
    int c;

    while((c = (unsigned char)*str++)){
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash % B;
}

/* Normalización de cadenas para que las búsquedas sean consistentes */
void normalizarCadena(char *s){
    if(s == NULL) return;

    /* quitar fin de línea */
    s[strcspn(s, "\r\n")] = '\0';

    /* quitar espacios iniciales */
    char *inicio = s;
    while(*inicio && isspace((unsigned char)*inicio)) inicio++;

    if(inicio != s){
        memmove(s, inicio, strlen(inicio) + 1);
    }

    /* quitar espacios finales */
    int n = (int)strlen(s);
    while(n > 0 && isspace((unsigned char)s[n - 1])){
        s[n - 1] = '\0';
        n--;
    }

    /* pasar a minúsculas */
    for(int i = 0; s[i]; i++){
        s[i] = (char)tolower((unsigned char)s[i]);
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

    for(size_t i = 0; linea[i] != '\0'; i++){
        char c = linea[i];

        if(c == '"'){
            dentroComillas = !dentroComillas;
        }
        else if(c == ',' && !dentroComillas){
            if(campoActual == indiceCampo){
                salida[j] = '\0';
                return 1;
            }
            campoActual++;
            j = 0;
        }
        else{
            if(campoActual == indiceCampo){
                if(j + 1 < maxSalida){
                    salida[j++] = c;
                }
            }
        }
    }

    if(campoActual == indiceCampo){
        salida[j] = '\0';
        return 1;
    }

    return 0;
}

/* Divide un campo usando '|' como separador.
   Guarda cada elemento en arr[i].
   Devuelve cuántos elementos encontró. */
int dividirCampo(const char *campo, char arr[][MAX_STRING_LEN], int max, bool normal){
    if(campo == NULL || campo[0] == '\0') return 0;

    int count = 0;
    int j = 0;

    for(size_t i = 0; ; i++){
        char c = campo[i];

        if(c == '|' || c == '\0'){
            if(j > 0 && count < max){
                arr[count][j] = '\0';
                if(normal) normalizarCadena(arr[count]);
                
                if(arr[count][0] != '\0'){
                    count++;
                }
            }

            j = 0;

            if(c == '\0'){
                break;
            }
        }
        else{
            if(count < max && j < MAX_STRING_LEN - 1){
                arr[count][j++] = c;
            }
        }
    }

    return count;
}

/* Retorna una estructura Videojuego creada con campos de interés 
a partir de una posición en el CSV */
Videojuego getRegisterFromCSV(uint32_t position, FILE *file){
    int r, len;
    bool in_quotes = false; //Para evitar errores parseando el CSV
    char line[MAX_LINE_LEN], value[MAX_STRING_LEN];
    Videojuego vjuego;

    //SEEK_SET para que cuente los byts indicados por position desde el inicio del csv
    r = fseek(file, position, SEEK_SET); 
    if(r != 0){
        perror("Error en fseek()");
        exit(-1);
    }
    
    if(fgets(line, MAX_LINE_LEN, file) == NULL){
        perror("Error leyendo linea");
        exit(-1);
    }

    int col = 0;
    int start = 0;
    int end = 0;

    for(int i = 0; line[i] != '\0'; i++){
        if(line[i] == '"'){ 
            in_quotes = !in_quotes; 
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

            len = end - start;
            strncpy(value, &line[start], len);
            value[len] = '\0';

            switch(col){
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

            col ++;
            start = i + 1;
        }

    }

    return vjuego;
}

void writeFull(int fd, void *buf, size_t size){
    size_t total = 0;
    ssize_t r;

    while(total < size){
        r = write(fd, ((char*)buf) + total, size - total);
        if(r < 0){
            perror("Error en write");
            exit(-1);
        }

        total += r;
    }
}

void readFull(int fd, void *buf, size_t size){
    size_t total = 0;
    ssize_t r;

    while(total < size){
        r = read(fd, ((char*)buf) + total, size - total);
        if(r < 0){
            perror("Error en read");
            exit(-1);
        }

        total += r;
    }
}