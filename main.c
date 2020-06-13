#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/stat.h>
#include <mqueue.h>
#include "structs.h"


//Esto va a estar aspero!

//gcc -pthread  main.c -lrt

//Encargado es un procceso que administra los hilos cocineros
//Calle es un proceso que lanza hilos del tipo cliente

//Process Cocineros que larga los hilos cocinero
//Cliente es un hilo que paga con a travez de FIFO
//Cocinero es un hilo informa por cola de mensajes al encargado


#define TAMMSG 8192
/* Variables globales para la alarma */
int placeOpen, finished;


int welcomeMenu();

/* Thread functions */
void *clientThread(void *);
void *chefThread(void *);
void *menuThread(void *);

/* Process functions */
void streetProcess(FoodPlace *);
void chefsProcess(FoodPlace *);
void managerProcess(FoodPlace *);


/* FoodMenu Functions */
Food *menuSetup();
Food *pickFood(Food * menu);
int getMaxWaitTime(Food *menu);

/* Shared */
int initShared(FoodPlace *);
void destroyShared(FoodPlace *);

void serveClient(Compartido *);

/* Close */
void closeFoodPlace();
void closeDoor(int);

void clearScreen();
int chefDesocupado(Compartido *);


int main() {

    int error=0, pid=0, childError=0, childPid=0;

    //Func crear structs
    //Func crear compartido

    FoodPlace *mercadoChino = (FoodPlace *)calloc(1,sizeof(FoodPlace));
    mercadoChino->chefs = (Chef*)calloc(COCINEROS,sizeof(Chef));
    mercadoChino->clientes = (Client*)calloc(CLIENTES,sizeof(Client));
    mercadoChino->menu = menuSetup();
    destroyShared(mercadoChino);
    initShared(mercadoChino);

    int opt;
    opt = welcomeMenu();
    if(opt == 1){
        pid = fork();
        if (pid == 0) {
//            printf(" \n");
            managerProcess(mercadoChino);
        }
        else if (pid > 0) {
            childPid = fork();
            if(childPid == 0){
                chefsProcess(mercadoChino);
            }
            else if (childPid > 0){
                streetProcess(mercadoChino);
            }
            else{
                perror("fork2()");
            }
        }
        else {
            perror("fork()");
        }
    }

    sleep(2);
//    printf("Hola\n");
//    sleep(10);
    return 0;
}

void streetProcess(FoodPlace *mercadoChino){
    /* Setear alarma de fin */
    signal(SIGALRM, closeDoor);
    alarm(60);

    //Datos compartidos
    int error = 0;
    Compartido *datos;
    datos = mmap(NULL, sizeof(Compartido), PROT_READ | PROT_WRITE, MAP_SHARED, mercadoChino->memoriaCompartida, 0);
    if (datos == MAP_FAILED) {
        perror("mmap()");
        error = -1;
    }

    placeOpen = 1;
    int tolerance = getMaxWaitTime(mercadoChino->menu);
    int aux = 0;
    while(placeOpen && aux < CLIENTES){
        pthread_t *cThread = (pthread_t *) calloc(1,sizeof(pthread_t));
        Client *client = (Client*)calloc(1,sizeof(Client));
//        client->id = aux++;
        datos->clientes[aux].id = aux++;
//        datos->clientes[aux].tolerance = (int*)calloc(1,sizeof(int));
//        client->split = (SplitSemaphore_t*)calloc(1,sizeof(SplitSemaphore_t));

        datos->clientes[aux].tolerance = tolerance;
        datos->clientes[aux].order = pickFood(mercadoChino->menu);
//        datos->clientes[aux].mtx = &datos->mtx;
        datos->clientes[aux].mtxClientQueue = &datos->mtxClientQueue;
        datos->clientes[aux].mtxEsperarPedido = &datos->mtxEsperaPedido;
//        client->semQueue = mercadoChino->semQueue;
//        client->m = mercadoChino->m;
//        client->memoria = mercadoChino->memoria;
//        client->split->pagar = mercadoChino->split->pagar;
//        client->split->cobrar = mercadoChino->split->cobrar;
//
//        mercadoChino->clients[client->id] = *client;

        sleep(rand()%5+1);
        datos->clientsTotal = aux;
//        aux++;
        pthread_create(cThread,NULL,clientThread,(void*)&datos->clientes[aux]);
    }
    pthread_exit(NULL);
}

void chefsProcess(FoodPlace *mercadoChino){
    int error = 0;
    Compartido *datos;
    datos = mmap(NULL, sizeof(Compartido), PROT_READ | PROT_WRITE, MAP_SHARED, mercadoChino->memoriaCompartida, 0);
    if (datos == MAP_FAILED) {
        perror("mmap()");
        error = -1;
    }
    Chef chef[COCINEROS];
    pthread_t *chefThreads = (pthread_t *)calloc(COCINEROS,sizeof(pthread_t));
    for(int i = 0; i < COCINEROS; i++){
//        printf("chefsProcess %d\n",i);
//        mercadoChino->chefs[i].id = i;
//       *mercadoChino->chefs[i].libre = 1;
//
//        //ver si tienen mutex
//          printf("Creando el hilo del cheff %d\n", i);
//        mercadoChino->chefs[i].mtx = mercadoChino->mtx;
//        pthread_create(&chefThreads[i],NULL,chefThread,&mercadoChino->chefs[i]);
        chef[i].id = i;
        chef[i].mtx = &datos->mtx[i];
//        pthread_mutex_unlock(&datos->mtx[i]);
        chef[i].mtxEsperarPedido = &datos->mtxEsperaPedido;
        chef[i].libre = &datos->libre[i];
        *chef[i].libre = 1;
        chef[i].pedido = &datos->asignado[i];
        pthread_create(&chefThreads[i],NULL,chefThread,&chef[i]);
    }
    for(int i = 0; i < COCINEROS; i++) {
        pthread_join(chefThreads[i], NULL);
    }
}

void managerProcess(FoodPlace *mercadoChino){
    int error = 0;
    int opt = 0;
    pthread_t menu;
    int ganancias = 0;
    int pedidosTerminados = 0;

    MenuParams params;
    params.compartido = (Compartido *)calloc(1,sizeof(Compartido));
    params.ganancia = (int *)calloc(1,sizeof(int));
    params.pedidosTerminados = (int *)calloc(1,sizeof(int));



    //Memoria compartida
    Compartido *datos;
    datos = mmap(NULL, sizeof(Compartido), PROT_READ | PROT_WRITE, MAP_SHARED, mercadoChino->memoriaCompartida, 0);
    if (datos == MAP_FAILED) {
        perror("mmap()");
        error = -1;
    }

    //Fifo de lectura
    int fifoOut = 0;
    fifoOut = open("./fifo", O_RDONLY);
    if(fifoOut < 0) {
        perror("fifo open");
    }
    char ack[4];
    int leidos;
//    leidos = read(fifoOut,ack,4);

//    if(leidos < 0) {
//        perror("fifo read");
//    }

    //Cola de mensajes
    int enviado;
    char numero[TAMMSG];
//    printf("lawea 1");
//    enviado=mq_receive(datos->colaMensajes,numero,TAMMSG,NULL);
//    printf("lawea 2");
//    if (enviado==-1) {
//        perror("mq_receive");
//        error=enviado;
//    }
    params.compartido = datos;
    params.ganancia = &ganancias;
    params.pedidosTerminados = &pedidosTerminados;
    pthread_create(&menu,NULL,menuThread,(void *)&params);

    //Menu del Juego
    while (!finished){

        opt = 0;
        scanf("%d",&opt);
        /*El switch es feo... lo cambiamos ?*/
        switch (opt) {
            case 1:
                serveClient(datos);
                printf("Opcion1\n");
                break;
            case 2:
//                deliverOrder(params->mercadoChino);
                printf("Opcion2\n");
                break;
            case 3:
//                closeFoodPlace(params->mercadoChino->m);
                closeFoodPlace();
                printf("Opcion3\n");
                break;
            default:
                printf("OPCION INCORRECTA");
        }
        fflush(stdin);
    }
    close(fifoOut);
    pthread_exit(NULL);
}

void *menuThread(void *arg){
    struct mq_attr parametros;
    MenuParams *datos = (MenuParams *) arg;
    int i= 0;
    char *status;

    int ganancia = 0;
    int terminados = 0;

    while (!finished){
        mq_getattr(datos->compartido->colaMensajes,&parametros);
//        clearScreen();
        printf("Puerta: %s\n\n", placeOpen ? "ABIERTA" : "CERRADA");
        printf("Estados de Cocineros:\n");
        for(i = 0; i < COCINEROS; i++ ){
            if(datos->compartido->libre[i]){
                status = "Desocupado";
            }else status = "Ocupado";
            printf("\tCocinero %d: %s\n",i+1,status);
        }
        //Esto lo tendria que ver con la FIFO
        printf("\nClientes en cola:\n");
//        printf("%d\n",datos->clientsTotal);
        for(i = 0; i < datos->compartido->clientsTotal; i++ ){
//            printf("La toleranciaa %d\n",datos->clientes[i].tolerance);
            if(datos->compartido->clientes[i].tolerance){
                printf("\tCliente %d pedido.\n",i);
//                printf("\tCliente %d pedido %s.\n",i, params->mercadoChino->clients[i].order->name);
            }
        }
        //Ver de contar fifo
        printf("\nPedidos Terminados esperando cobrar: %ld\n",
               parametros.mq_curmsgs);



//        printf("\nGanancias: $%d\n",*datos);
        printf("\nAcciones:\n");
        printf("\t1 - Atender Cliente\n");
        printf("\t2 - Entregar Pedido\n");
        printf("\t3 - Cerrar Local\n");
        printf("\nIngrese una opcion: \n");

        sleep(2);
    }
}

void *clientThread(void *arg){
    Client *client = (Client *)arg;
    char strnum[20];
    int guardado;

    printf("Nuevo cliente en cola");

    //Canal de envio de FIFO
    int fifoIn = 0;
    fifoIn = open("./fifo", O_WRONLY);
    if(fifoIn < 0) {
        perror("fifo open");
    }
    struct timespec wait;
    clock_gettime(CLOCK_REALTIME, &wait);
    wait.tv_sec += client->tolerance;
    int errCode = pthread_mutex_timedlock(client->mtxClientQueue, &wait);

    if(!errCode){
        client->tolerance = 0;
        pthread_mutex_lock(client->mtxEsperarPedido);
        //Envio el valor de la comida al encargado
        sprintf(strnum,"%d", client->order->value);
        guardado = write(fifoIn,strnum,6);
        if(guardado < 0){
            perror("Fifo Escritura\n");
        }
    } else{
        printf("El Cliente %d se canso de esperar\r\n",client->id);
        client->tolerance = 0;
    }




    close(fifoIn);
    //free(client);
    pthread_exit(NULL);

}

void *chefThread(void *arg){
    Chef *chef = (Chef *)arg;
    char idCliente[5];
    *chef->libre = 1;
    int enviado = 0;
    //abrir la cola de mensajes
    mqd_t cola;
    int error = 0;
    cola=mq_open("/colaMensajes",O_WRONLY|O_CREAT,0660,NULL);
    if (cola==-1) {
        error=cola;
        perror("mq_open");
    }

//    printf("Soy el chef %d bloqueando\n",chef->id);
//    pthread_mutex_unlock(chef->mtx);
//    printf("Soy el chef %d desbloqueando\n",chef->id);
//    pthread_mutex_lock(chef->mtx);
//

    while(!finished){
        pthread_mutex_lock(chef->mtx);
        *chef->libre = 0;
        printf("Prep time %d\n",chef->pedido->order.prepTime);
        sleep(chef->pedido->order.prepTime);
        enviado = 0;
//        Envio por cola de mensajes
        snprintf(idCliente,4,"%d",chef->pedido->idCliente);
        enviado = mq_send(cola,idCliente,4,0);
        if(enviado == -1) {
            perror("Cola envio error");
        }
        pthread_mutex_unlock(chef->mtxEsperarPedido);
        *chef->libre = 1;
    }

//    free(chef);
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

int initShared(FoodPlace *mercadoChino){
    mercadoChino->memoriaCompartida = 0;
    int error = 0;

    mercadoChino->memoriaCompartida = shm_open("/memCompartida", O_CREAT | O_RDWR, 0660);
    if (mercadoChino->memoriaCompartida < 0) {
        perror("shm_open()");
        error = -1;
    }
    if (!error) {
        printf("Descriptor de memoria creado!\n");
        error = ftruncate(mercadoChino->memoriaCompartida, sizeof(Compartido));
        if (error)
            perror("ftruncate()");
    }
    Compartido *datos;
    datos = mmap(NULL, sizeof(Compartido), PROT_READ | PROT_WRITE, MAP_SHARED, mercadoChino->memoriaCompartida, 0);
    if (datos == MAP_FAILED) {
        perror("mmap()");
        error = -1;
    }
    //Inicia el mutex compartido
    pthread_mutexattr_t mtxa[COCINEROS];
    for (int j = 0; j < COCINEROS; ++j) {
        pthread_mutexattr_init(&mtxa[j]);
        pthread_mutexattr_setpshared(&mtxa[j], PTHREAD_PROCESS_SHARED);
    }

    pthread_mutexattr_t mtxa2;
    pthread_mutexattr_init(&mtxa2);
    pthread_mutexattr_setpshared(&mtxa2, PTHREAD_PROCESS_SHARED);

    pthread_mutexattr_t mtxa3;
    pthread_mutexattr_init(&mtxa3);
    pthread_mutexattr_setpshared(&mtxa3, PTHREAD_PROCESS_SHARED);


    //Inicio mutex compartidos
    for (int i = 0; i < COCINEROS; ++i) {
        pthread_mutex_init(&datos->mtx[i], &mtxa[i]);
        pthread_mutex_lock(&datos->mtx[i]);
    }

    pthread_mutex_init(&datos->mtxEsperaPedido, &mtxa2);
    pthread_mutex_lock(&datos->mtxEsperaPedido);


    pthread_mutex_init(&datos->mtxClientQueue, &mtxa3);
    pthread_mutex_unlock(&datos->mtxClientQueue);

    //Crear la fifo!
    error = mkfifo("./fifo", 0777);
    if((error < 0) && (errno != EEXIST)) {
        perror("mkfifo");
    }
    else {
        error = 0;
    }

    //Crear Colar de mensajes
    mercadoChino->colaMensajes = mq_open("/colaMensajes", O_RDWR | O_CREAT, 0777, NULL);
    if (mercadoChino->colaMensajes == -1) {
        perror("mq_open");
        error = mercadoChino->colaMensajes;
    }

    datos->colaMensajes = mercadoChino->colaMensajes;

    return error;
}

void destroyShared(FoodPlace *mercadoChino){
    int error = 0;
    int fifo;

    //Cerrar memoria compartida
    error = shm_unlink("/memCompartida");
    if (error) {
        perror("unlink()");
    }
    else {
        printf("Descriptor de memoria borrado!\n");
    }

    //Cerrar fifo
    fifo = unlink("./fifo");
    if (fifo) {
        perror("unlinkFIFO!");
    }

    //Cerrra cola de mensajes
    if(!access("/colaMensajes", F_OK)) {
        mercadoChino->colaMensajes = mq_close(mercadoChino->colaMensajes);
        if(mercadoChino->colaMensajes)
            perror("mq_close");
        mercadoChino->colaMensajes = mq_unlink("/colaMensajes");
        if(mercadoChino->colaMensajes)
            perror("mq_close");
    }


}

void closeFoodPlace(){
    finished = 1;
}

void clearScreen(){
//    system("@cls||clear");
    system("clear");
}

int welcomeMenu(){
    int opt = 0;
    do {
//        clearScreen();
        printf("Juego Programacion Concurrente\n");
        printf("1 - Iniciar juego\n");
        printf("2 -  Salir\n");
        printf("Ingrese una opcion: ");
        scanf("%d", &opt);
    } while(opt < 1 || opt > 2);

    return opt;
}

void closeDoor(int sigCode){
    placeOpen = 0;
}

void serveClient(Compartido *datos){
    int chef = -1;\
    chef = chefDesocupado(datos);
    if(chef >= 0){
        for(int i = 0; i < datos->clientsTotal; i++ ){
            if(datos->clientes[i].tolerance){
                //bloquear por tiempo libre de chef
//            sem_post(mercadoChino->semQueue);
                pthread_mutex_unlock(&datos->mtxClientQueue);
                datos->asignado[chef].order = *datos->clientes[i].order;
                datos->asignado[chef].idCliente = i;
                pthread_mutex_unlock(&datos->mtx[chef]);
                i = datos->clientsTotal;
            }
        }

    }
    return;
}

int chefDesocupado(Compartido *datos){
    int libre = -1;
    for (int i = 0; i < COCINEROS; ++i) {
        if(datos->libre[i] == 1){
            return i;
        }
    }
    return libre;

}