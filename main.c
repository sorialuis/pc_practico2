#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "structs.h"
//Esto va a estar aspero!
//Cristian Puto

//Encargado es un procceso que administra los hilos cocineros

//Calle es un proceso que lanza hilos del tipo calle

//Calle es un hilo

//Cocinero es un hilo

#define COCINEROS 3
#define CLIENTES 50

/* Variables globales para la alarma */
int placeOpen, finished;

/* Thread functions */

void *clientThread(void *);
void *chefThread(void *);

/* Process functions */
void *streetProcess(FoodPlace *);
void *chefsProcess(void *);

/* FoodMenu Functions */
Food *menuSetup();
Food *pickFood(Food * menu);
int getMaxWaitTime(Food *menu);

int main() {
    printf("Hello, World!\n");
    int error=0, pid=0, childError=0, childPid=0;

    pid = fork();
    if (pid == 0) {
        Encargado();
    }
    else if (pid > 0) {
        childPid = fork();
        if(childPid == 0){
            Cocineros();
        }
        else if (childPid > 0){
            Calle();
        }
        else{
            perror("fork2()");
        }
    }
    else {
        perror("fork()");
    }
    return 0;
}

void *streetProcess(FoodPlace *mercadoChino){
    placeOpen = 1;
    int tolerance = getMaxWaitTime(mercadoChino->menu);
    int aux = 0;

    while(placeOpen && aux < CLIENTES){
        pthread_t *cThread = (pthread_t *) calloc(1,sizeof(pthread_t));

        Client *client = (Client*)calloc(1,sizeof(Client));
        client->id = aux++;
        client->tolerance = (int*)calloc(1,sizeof(int));
        client->split = (SplitSemaphore_t*)calloc(1,sizeof(SplitSemaphore_t));
        *client->tolerance = tolerance;
        client->order = pickFood(mercadoChino->menu);
        client->mtx = mercadoChino->mtx;
        client->semQueue = mercadoChino->semQueue;
        client->m = mercadoChino->m;
        client->memoria = mercadoChino->memoria;
        client->split->pagar = mercadoChino->split->pagar;
        client->split->cobrar = mercadoChino->split->cobrar;

        mercadoChino->clients[client->id] = *client;

        sleep(rand()%5+1);
        pthread_create(cThread,NULL,clientThread,(void*)client);
        mercadoChino->clientsTotal = aux;
    }

    printf("Cerrando hilo calle");

    pthread_exit(NULL);
}

void *clientThread(void *arg){
    Client *client = (Client *)arg;

    int *datos=NULL;
    int mem = *client->memoria;
    datos = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, mem, 0);

    printf("Nuevo cliente en cola");


    struct timespec wait;
    clock_gettime(CLOCK_REALTIME, &wait);
    wait.tv_sec += *client->tolerance;

    int errCode = sem_timedwait(client->semQueue, &wait);

    if(!errCode){
        *client->tolerance = 0;
        IngresarPedido(client->m, *client->order);
        pthread_mutex_lock(client->mtx);
        SacarComida(client->m, client->order);

        sem_wait(client->split->pagar);
        //Guardando en memoria compartida
        *datos = *datos + client->order->value;
        sem_post(client->split->cobrar);


    } else{
        printf("El Cliente %d se canso de esperar\r\n",client->id);
        *client->tolerance = 0;
    }

    free(client);
    pthread_exit(NULL);

}

void *chefThread(void *arg){
    Chef *chef = (Chef *)arg;
    Food comida;

    while(!finished){
        SacarPedido(chef->m,&comida);
        *chef->libre = 0;
        sleep(comida.prepTime);
        IngresarComida(chef->m,comida);
        *chef->m->lista_terminados->cant = *chef->m->lista_terminados->cant + 1;
        pthread_mutex_unlock(chef->mtx);
        *chef->libre = 1;
    }
    free(chef);
    pthread_exit(NULL);

}

Food *menuSetup(){
    Food *menu = calloc(10, sizeof(Food));

    sprintf(menu[0].name,"Pizza");
    menu[0].prepTime = 2;
    menu[0].value = 100;

    sprintf(menu[1].name,"Lomito");
    menu[1].prepTime = 2;
    menu[1].value = 300;

    sprintf(menu[2].name,"Empanadas");
    menu[2].prepTime = 5;
    menu[2].value = 150;

    sprintf(menu[3].name,"Ensalada");
    menu[3].prepTime = 4;
    menu[3].value = 100;

    sprintf(menu[4].name,"Milanesa");
    menu[4].prepTime = 3;
    menu[4].value = 150;

    sprintf(menu[5].name,"Sushi");
    menu[5].prepTime = 6;
    menu[5].value = 200;

    sprintf(menu[6].name,"Chop Suey");
    menu[6].prepTime = 3;
    menu[6].value = 200;

    sprintf(menu[7].name,"Pollo");
    menu[7].prepTime = 4;
    menu[7].value = 150;

    sprintf(menu[8].name,"Matambre");
    menu[8].prepTime = 3;
    menu[8].value = 350;

    sprintf(menu[9].name,"Choripan");
    menu[9].prepTime = 2;
    menu[9].value = 100;

    return menu;
}

Food *pickFood(Food *menu){
    return &menu[rand()%10];
}

int getMaxWaitTime(Food *menu){
    int max = menu[0].prepTime;

    for(int i = 0; i < 10; i++)
        if(menu[i].prepTime > max)
            max = menu[i].prepTime;
    return max * 4;
}
