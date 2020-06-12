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

}Manager;

typedef struct{
    int idCliente;
    Food orden;
}Pedido;

typedef struct{
    int id;
    int *libre;
    pthread_mutex_t *mtx;
    Pedido pedido;
}Chef;

typedef struct{
    int libre[COCINEROS];
    Pedido asignado[COCINEROS];
    pthread_mutex_t mtx;
    pthread_mutex_t mtxClientQueue;
    int clientTolerance[CLIENTES];
    int clientsTotal;
    int placeOpen, finished;
}Compartido;


typedef struct{
    int memoriaCompartida;
    Food *menu;
    Client *clientes;
    Food *currentOrder;
    Chef *chefs;
    int open;
    int fifo;
    mqd_t colaMensajes;
//    pthread_mutex_t *mtx;
}FoodPlace;

#endif
