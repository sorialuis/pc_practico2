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
int deliverOrder();

/* Close */
void closeFoodPlace();
void closeDoor(int);

void clearScreen();
int chefDesocupado(Compartido *);


int main() {
    int error=0, pid=0, childError=0, childPid=0;
    
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
    destroyShared(mercadoChino);
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

        datos->clientes[aux].id = aux;


        datos->clientes[aux].tolerance = tolerance;
        datos->clientes[aux].order = pickFood(mercadoChino->menu);

        datos->clientes[aux].mtxClientQueue = &datos->mtxClientQueue;
        datos->clientes[aux].mtxEsperarPedido = &datos->mtxEsperaPedido;


        sleep(rand()%5+1);

        pthread_create(cThread,NULL,clientThread,(void*)&datos->clientes[aux]);
        datos->clientsTotal = ++aux;
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

    struct mq_attr attrCola;
//    mqd_t cola = mq_open("/colaMensajes",O_RDONLY);
    mqd_t cola = mq_open("/colaMensajes",O_RDONLY,0664,&attrCola);
    if(cola == -1){
        perror("mq_open managerProcess");
    }

    char buf[6];
    char nro_cliente[TAMMSG];

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
    }

    //Fifo de lectura
    int mififo = open("./fifo",O_RDONLY);


    params.compartido = datos;
    params.ganancia = &ganancias;
    params.pedidosTerminados = &pedidosTerminados;
    pthread_create(&menu,NULL,menuThread,(void *)&params);

    //Menu del Juego
    while (!finished){
        opt = 0;
        scanf("%d",&opt);
        if(opt == 1){
            serveClient(datos);
        }else if(opt == 2){
            //Obtengo el numero del cliente que esta esperando cobrar
            mq_getattr(cola, &attrCola);

            if(attrCola.mq_curmsgs){
                int err = 0;
                err = mq_receive(cola, nro_cliente, TAMMSG, NULL);
                if(err == -1){
                    perror("Error de mq_recive");
                }
                printf("Cobrando al cliente %s\n",nro_cliente);
                //Obtengo de la fifo el pago del cliente
                int numero = 0;
                read(mififo,buf,sizeof(buf));
                numero = atoi(buf);
                ganancias = ganancias + numero;
            }else{
                printf("Sin clientes para cobrar\n");
            }
        }else if(opt == 3){
            //cerrar
        }else {
            printf("OPCION INCORRECTA\n");
            printf("%d\n",opt);
        }


        fflush(stdin);
    }
    close(mififo);
    pthread_exit(NULL);
}

void *menuThread(void *arg){
    struct mq_attr attrCola;
    mqd_t cola = mq_open("/colaMensajes",O_RDONLY,0664,&attrCola);

    MenuParams *datos = (MenuParams *) arg;
    int i= 0;
    char *status;

    while (!finished){
        mq_getattr(cola,&attrCola);
        clearScreen();
        printf("Puerta: %s\n\n", placeOpen ? "CERRADA" : "ABIERTA");
        printf("Estados de Cocineros:\n");
        for(i = 0; i < COCINEROS; i++ ){
            if(datos->compartido->libre[i]){
                status = "Desocupado";
            }else status = "Ocupado";
            printf("\tCocinero %d: %s\n",i+1,status);
        }
        //Esto lo tendria que ver con la FIFO
        printf("\nClientes en cola:\n");
        for(i = 0; i < datos->compartido->clientsTotal; i++ ){
            if(datos->compartido->clientes[i].tolerance){
//                printf("\tCliente %d pedido.\n",i);
                printf("\tCliente %d pedido %s.\n",i, datos->compartido->clientes[i].order->name);
            }
        }

        //Ver de contar fifo
        printf("\nPedidos Terminados esperando cobrar: %ld\n",
               attrCola.mq_curmsgs);

        printf("\nGanancias: $%d\n",*datos->ganancia);
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
//    free(client);
    pthread_exit(NULL);

}

void *chefThread(void *arg){
    Chef *chef = (Chef *)arg;
    char idCliente[10];
    *chef->libre = 1;
    int enviado = 0;
    //abrir la cola de mensajes
    mqd_t cola;
    int error = 0;
    cola = mq_open("/colaMensajes",O_WRONLY | O_CREAT,0664, NULL);
    if (cola==-1) {
        error=cola;
        perror("mq_open");
    }

    while(!finished){
        pthread_mutex_lock(chef->mtx);
        *chef->libre = 0;
        printf("Prep time %d\n",chef->pedido->order.prepTime);
        sleep(chef->pedido->order.prepTime);
        enviado = 0;
//        Envio por cola de mensajes
        snprintf(idCliente,10,"%d",chef->pedido->idCliente);
        enviado = mq_send(cola,idCliente,10,0);
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

    //Crear ColaMensajes
    mqd_t cola;
    cola=mq_open("/colaMensajes",O_CREAT, 0777, NULL);
    if (cola==-1) {
        perror("mq_open Init");
        error = cola;
    }

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
    mercadoChino->colaMensajes = mq_unlink("/colaMensajes");
    if(mercadoChino->colaMensajes)
        perror("mq_close");

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
        clearScreen();
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

int deliverOrder(int mififo){
    char buf[50];
    int numero = 0;
    read(mififo,buf,sizeof(buf));
    numero = atoi(buf);
    close(mififo);
    return numero;
}