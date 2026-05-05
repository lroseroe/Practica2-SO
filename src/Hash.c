#include <stdio.h>
#include <stdlib.h>
#include <errno.h> //Para perror


typedef struct HashNode
{
    long llave; //valor clave
    long offset; //posicion de byte en el csv
    struct HashNode* siguiente; //siguiente nodo para chaining
}HashNode;

typedef struct HashMap
{
    long tamanoBuckets; //cantidad de espacio
    long tamanoActual; //cantidad de elementos
    HashNode** buckets; //espacio fisico en memoria buckets

    //todo se realiza para saber cantidad de elementos 
    //del hashmap y ver su optimizacion real con busquedas en O(1) 
}HashMap;

///////////////////////////////////////////////////////
//crear y borrar en memoria HM
HashMap* crearHM(long tamanoBuckets){
    HashMap* myHashMap=(HashMap*)malloc(sizeof(HashMap));//se reserva espacio en memoria para HashMap como struct
    if(myHashMap==NULL){
        perror("Error al crear HashMap");
        exit(-1);
    }
    myHashMap->tamanoBuckets=tamanoBuckets;//se asigna cuantos buckets se desea en la tabla
    myHashMap->tamanoActual=0;//se inicializa la cantidad de elementos en 0
    myHashMap->buckets=(HashNode**)calloc(myHashMap->tamanoBuckets, sizeof(HashNode*));
    //se reserva la cantidad de memoria necesaria para los buckets
    //se emplea calloc con el # de buckets necesario y el tamaño de cada uno de los HashNodes
    
    if(myHashMap->buckets==NULL){
        perror("Error al asignar memoria para Buckets");
        free(myHashMap);
        exit(-1);
    }
    printf("HashMap creado correctamente");
    return myHashMap;//se retorna el hashmap creado
}

void borrarHM(HashMap* myHashMap){
    if(myHashMap==NULL){
        return;
    }
    for(long i=0;i<myHashMap->tamanoBuckets;i++){//se recorren todos los buckets disponibles
        HashNode* actual=myHashMap->buckets[i];
        while(actual!=NULL){//se recorre y se libera uno a uno el chaining
            HashNode* sigui=actual->siguiente;
            free(actual);
            actual=sigui;
        }
    }
    free(myHashMap->buckets);//se libera el espacio de buckets
    free(myHashMap);//se libera el espacio del struct
    printf("Hashmap eliminado correctamente");
}
/////////////////////////////////////////////////////////////////////
//Hash de la llave en los buckets, se debe mejorar para distribuir equitativamente
long hash(long llave, long tamanoBuckets){
    long miLlave=abs(llave);//se convierte en positivo
    return (long)(miLlave%tamanoBuckets);//se hace modulo cantidad de buckets
}


//Insertar un elemento al hashmap
void insertarHM(HashMap* myHashMap, long llave, long offset){
    long index=hash(llave, myHashMap->tamanoBuckets);//se hashea segun llave

    HashNode* nuevo=(HashNode*)malloc(sizeof(HashNode));//se aparta espacio en memoria para la nueva entrada
    if(!nuevo){ perror("Error al asignar memoria"); exit(EXIT_FAILURE); }
    nuevo->llave=llave;//se asignan valores
    nuevo->offset=offset;

    nuevo->siguiente=myHashMap->buckets[index];//se realiza el chaining al siguiente elemento
    myHashMap->buckets[index]=nuevo;//se asigna como nueva cabeza en el bucket
    
    myHashMap->tamanoActual+=1;//se aumenta el tamano actual de hashmap
}

long buscarOffset(HashMap* myHashMap, long llave){
    long index=hash(llave, myHashMap->tamanoBuckets);//se hashea segun llave para encontrar el index ya asignado

    HashNode* actual=myHashMap->buckets[index];//se encuentra el nodo cabeza en el bucket segun index
    while(actual!=NULL){
        if(actual->llave==llave){//se recorre la cadena hasta encontrar el valor deseado
            return actual->offset;
        }
        actual=actual->siguiente;
    }
    return -1;//si no se encuentra se devuelve -1

}

void datoCSV(long offset){
    if(offset == -1){
        printf("NA\n");
        return;
    }

    FILE* f = fopen("dataset.csv","r");
    if(!f){
        perror("fopen");
        return;
    }

    if(fseek(f, offset, SEEK_SET) != 0){
        perror("fseek");
        fclose(f);
        return;
    }

    char linea[1024];
    if(!fgets(linea, sizeof(linea), f)){
        printf("No se pudo leer la linea\n");
        fclose(f);
        return;
    }

    printf("Registro: %s", linea); // ya trae \n generalmente
    fclose(f);
}

int main(){
    
    HashMap* myHashMap=crearHM(10);
    
    insertarHM(myHashMap, 123, 32);
    insertarHM(myHashMap, 223, 30);
    insertarHM(myHashMap, 165, 40);

    printf("%li",buscarOffset(myHashMap, 123));

    borrarHM(myHashMap);
    return 0;
}