#ifndef PRACTICO2_STRUCTS_H
#define PRACTICO2_STRUCTS_H

#define COCINEROS 3
#define CLIENTES 50

typedef struct {
    char name[50];
    int prepTime;
    int value;
}Food;

typedef struct{
    int id;
    int *tolerance;
    Food *order;
    pthread_mutex_t *mtx;
    pthread_mutex_t *mtxClientQueue;
}Client;

typedef struct{
    int id;
    int *libre;
    pthread_mutex_t *mtx;
}Chef;

typedef struct{

}Manager;

typedef struct{
    int libre[COCINEROS];
    pthread_mutex_t mtx;
    pthread_mutex_t mtxClientQueue;
    int clientTolerance[CLIENTES];
    int clientsTotal;
}Compartido;

typedef struct{
    int memoriaCompartida;
    Food *menu;
    Client *clientes;
    Food *currentOrder;
    Chef *chefs;
    int open;
//    pthread_mutex_t *mtx;
}FoodPlace;

#endif
