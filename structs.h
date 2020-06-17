#ifndef PRACTICO2_STRUCTS_H
#define PRACTICO2_STRUCTS_H

#define COCINEROS 3
#define CLIENTES 20

typedef struct {
    char name[50];
    int prepTime;
    int value;
}Food;

typedef struct{
    int id;
    int tolerance;
    Food *order;
    pthread_mutex_t *mtx;
    pthread_mutex_t *mtxEsperarPedido;
    pthread_mutex_t *mtxClientQueue;
}Client;

typedef struct{
    int idCliente;
    Food order;
}Pedido;

typedef struct{
    int id;
    int *libre;
    int *placeOpen;
    pthread_mutex_t *mtx;
    pthread_mutex_t *mtxEsperarPedido;
    Pedido *pedido;
}Chef;

typedef struct{
    int libre[COCINEROS];
    Pedido asignado[COCINEROS];
    pthread_mutex_t mtx[COCINEROS];
    pthread_mutex_t mtxClientQueue;
    Client clientes[CLIENTES];
    int clientsTotal;
    int placeOpen;
    pthread_mutex_t mtxEsperaPedido;
}Compartido;

typedef struct {
    Compartido *compartido;
    int *ganancia;
    int *pedidosTerminados;
    int *placeOpen;
}MenuParams;


typedef struct{
    int memoriaCompartida;
    Food *menu;
    Client *clientes;
    Food *currentOrder;
    Chef *chefs;
    int fifo;
    mqd_t colaMensajes;
}FoodPlace;

#endif
