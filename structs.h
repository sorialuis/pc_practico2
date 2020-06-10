#ifndef PRACTICO2_STRUCTS_H
#define PRACTICO2_STRUCTS_H

typedef struct {
    char name[50];
    int prepTime;
    int value;
}Food;

typedef struct{
    int id;
    int *tolerance;
    Food *order;
}Client;

typedef struct{
    int id;
    int *libre;
}Chef;

typedef struct{

}Manager;

typedef struct{
    Food *menu;
    Client *clientes;
    Food *currentOrder;
    Chef *chefs;
    int open;
}FoodPlace;

#endif
