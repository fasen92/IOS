/**
 * @file proj2.c
 * @author Ivan Mahut, xmahut01
 * @brief Program vytvara molekuly vody z dvoch atomov vodiku a jedneho atomu kysliku
 * @version 0.1
 * @date 2022-04-17
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>


#define MMAP(ptr) {(ptr) = mmap(NULL, sizeof(*(ptr)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1 , 0);}

sem_t *writeSem = NULL;
sem_t *mutex = NULL;
sem_t *mutexMol = NULL;
sem_t *oxygenQ = NULL;
sem_t *hydrogenQ = NULL;
sem_t *barrier1 = NULL;
sem_t *barrier2 = NULL;
sem_t *moleculeCreating = NULL;


static int *processCount;
static int *oxygenCount;
static int *hydrogenCount;
static int *moleculeCount;
static int *moleculeTmp;

FILE *file;

typedef struct{
    int NO;
    int NH;
    int TI;
    int TB;
}params;

/**
 * @brief funkcia zapisuje parametre do struktury param, validuje ich spravnost
 * 
 * @param argc pocet vstupnych argumentov
 * @param argv pole poli argumentov
 * @param param struktura pre ukladanie
 * @return param 
 */
params argCheck(int argc, char **argv, params param){
    char *wrongPtr;
    if(argc != 5){
        fprintf(stderr, "ERROR: not enough arguments");
        exit(1);
    }

    param.NO = strtol(argv[1], &wrongPtr, 10);
    if(param.NO <= 0 || wrongPtr == argv[1] || *wrongPtr !='\0'){
        fprintf(stderr, "ERORR: Arguments are not valid");
        exit(1);
    }

    param.NH = strtol(argv[2], &wrongPtr, 10);
    if(param.NH <= 0 || wrongPtr == argv[2] || *wrongPtr !='\0'){
        fprintf(stderr, "ERORR: Arguments are not valid");
        exit(1);
    }

    param.TI = strtol(argv[3], &wrongPtr, 10);
    if(param.TI < 0 || param.TI > 1000 || wrongPtr == argv[3] || *wrongPtr !='\0'){
        fprintf(stderr, "ERORR: Arguments are not valid");
        exit(1);
    }

    param.TB = strtol(argv[4], &wrongPtr, 10);
    if(param.TB < 0 || param.TB > 1000 || wrongPtr == argv[4] || *wrongPtr !='\0'){
        fprintf(stderr, "ERORR: Arguments are not valid");
        exit(1);
    }

    return param;
}

/**
 * @brief funkcia uvolnuje alokovanu pamat
 * 
 */
void freeMemory(){
    munmap(processCount, sizeof *processCount);
    munmap(oxygenCount, sizeof *oxygenCount);
    munmap(hydrogenCount, sizeof *hydrogenCount);
    munmap(moleculeCount, sizeof *moleculeCount);
    munmap(moleculeTmp, sizeof *moleculeTmp);
}

/**
 * @brief funkcia uvolnuje semafory
 * 
 */
void freeSem(){
    sem_close(writeSem);
    sem_unlink("/xmahut01_ios2_writeSem");

    sem_close(mutex);
    sem_unlink("/xmahut01_ios2_mutex");

    sem_close(mutexMol);
    sem_unlink("/xmahut01_ios2_mutexMol");

    sem_close(oxygenQ);
    sem_unlink("/xmahut01_ios2_oxygenQ");

    sem_close(hydrogenQ);
    sem_unlink("/xmahut01_ios2_hydrogenQ");

    sem_close(barrier1);
    sem_unlink("/xmahut01_ios2_barrier1");

    sem_close(barrier2);
    sem_unlink("/xmahut01_ios2_barrier2");

    sem_close(moleculeCreating);
    sem_unlink("/xmahut01_ios2_moleculeCreating");
}

/**
 * @brief funkcia vytvara zdielanu pamat
 * 
 */
void sharedMemSet(){
    MMAP(processCount)
    if(processCount == MAP_FAILED){
        fprintf(stderr, "ERROR: Shared memory failed to allocate");
        freeMemory();
        fclose(file);
        exit(1);

    }
    
    MMAP(oxygenCount)
    if(oxygenCount == MAP_FAILED){
        fprintf(stderr, "ERROR: Shared memory failed to allocate");
        freeMemory();
        fclose(file);
        exit(1);
    }

    MMAP(hydrogenCount)
    if(hydrogenCount == MAP_FAILED){
        fprintf(stderr, "ERROR: Shared memory failed to allocate");
        freeMemory();
        fclose(file);
        exit(1);
    }

    MMAP(moleculeCount)
    if(moleculeCount == MAP_FAILED){
        fprintf(stderr, "ERROR: Shared memory failed to allocate");
        freeMemory();
        fclose(file);
        exit(1);
    }

    MMAP(moleculeTmp)
    if(moleculeTmp == MAP_FAILED){
        fprintf(stderr, "ERROR: Shared memory failed to allocate");
        freeMemory();
        fclose(file);
        exit(1);
    }

    *processCount = 0;
    *oxygenCount = 0;
    *hydrogenCount = 0;
    *moleculeCount = 0;
    *moleculeTmp = 0;
}

/**
 * @brief funckia vytvara a nastavuje semafory
 * 
 */
void semSet(){
    if((writeSem = sem_open("/xmahut01_ios2_writeSem", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED){
        fprintf(stderr, "ERORR: Inicialization of semaphore has failed");
        freeMemory();
        freeSem();
        fclose(file);
        exit(1);
    }
    
    if((mutex = sem_open("/xmahut01_ios2_mutex", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED){
        fprintf(stderr, "ERORR: Inicialization of semaphore has failed");
        freeMemory();
        freeSem();
        fclose(file);
        exit(1);
    }

    if((mutexMol = sem_open("/xmahut01_ios2_mutexMol", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED){
        fprintf(stderr, "ERORR: Inicialization of semaphore has failed");
        freeMemory();
        freeSem();
        fclose(file);
        exit(1);
    }

    
    if((oxygenQ = sem_open("/xmahut01_ios2_oxygenQ", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED){
        fprintf(stderr, "ERORR: Inicialization of semaphore has failed");
        freeMemory();
        freeSem();
        fclose(file);
        exit(1);
    }

    if((hydrogenQ = sem_open("/xmahut01_ios2_hydrogenQ", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED){
        fprintf(stderr, "ERORR: Inicialization of semaphore has failed");
        freeMemory();
        freeSem();
        fclose(file);
        exit(1);
    }

    if((barrier1 = sem_open("/xmahut01_ios2_barrier1", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED){
        fprintf(stderr, "ERORR: Inicialization of semaphore has failed");
        freeMemory();
        freeSem();
        fclose(file);
        exit(1);
    }

    if((barrier2 = sem_open("/xmahut01_ios2_barrier2", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED){
        fprintf(stderr, "ERORR: Inicialization of semaphore has failed");
        freeMemory();
        freeSem();
        fclose(file);
        exit(1);
    }

    if((moleculeCreating = sem_open("/xmahut01_ios2_moleculeCreating", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED){
        fprintf(stderr, "ERORR: Inicialization of semaphore has failed");
        freeMemory();
        freeSem();
        fclose(file);
        exit(1);
    }
}

/**
 * @brief sluzi na generovanie nahodneho casu pre oxygen a hydrogen
 * 
 * @param maxTime maximalny mozny generovany cas 
 * @return int vygenerovany cas
 */
int genTimeAtom(int maxTime){
    int ranT;
    if(maxTime == 0){
        return 0;
    }
    srand(time(NULL) * getpid()); // nasobenie pid pre zarucenie nahodnosti
    ranT = (rand() % maxTime);
    ranT = ranT * 1000;
    return ranT;
}

/**
 * @brief sluzi na generovanie nahodneho casu pre molekulu
 * 
 * @param maxTime maximalny mozny generovany cas 
 * @return int vygenerovany cas
 */
int genTimeMol(int maxTime){
    int ranT;
    if(maxTime == 0){
        return 0;
    }
    srand(time(NULL));
    ranT = (rand() % maxTime);
    ranT = ranT * 1000;
    return ranT;
}


/**
 * @brief funkcia na vytvaranie molekul
 * 
 * @param atomNum cislo atomu
 * @param atom true = oxygen; false = hydrogen
 * @param TB maximalny cas cakania pre molekulu
 */
void genMolecule(int atomNum, bool atom, int TB){
    int moleculeCurrent = *moleculeCount + 1;

    sem_wait(writeSem);
        (*processCount)++;
        if(atom){
            fprintf(file,"%d: O %d: creating molecule %d\n",*processCount, atomNum, moleculeCurrent);
        }else{
            fprintf(file,"%d: H %d: creating molecule %d\n",*processCount, atomNum, moleculeCurrent);
        }

        fflush(file);
    sem_post(writeSem);

    //cakanie - iba oxygen
    if(atom){
        usleep(genTimeMol(TB));
    }

    //bariera->
    sem_wait(mutexMol);
        (*moleculeTmp)++;
        if(*moleculeTmp == 3){
            (*moleculeCount)++;
            sem_wait(barrier2);
            sem_post(barrier1);
        }
    sem_post(mutexMol);

    sem_wait(barrier1);
    sem_post(barrier1);

    sem_wait(writeSem);
        (*processCount)++;
        if(atom){
            fprintf(file,"%d: O %d: molecule %d created\n",*processCount, atomNum, moleculeCurrent);
        }else{
            fprintf(file,"%d: H %d: molecule %d created\n",*processCount, atomNum, moleculeCurrent);
        }
        fflush(file);
    sem_post(writeSem);

    sem_wait(mutexMol);
        (*moleculeTmp)--;
        if(*moleculeTmp == 0){ 
            sem_wait(barrier1);
            sem_post(barrier2);
        }
    sem_post(mutexMol);

    sem_wait(barrier2);
    sem_post(barrier2);
    //<-bariera
}

/**
 * @brief spracovanie procesov oxygen
 * 
 * @param param vstupne parametre programu
 * @param i poradove cislo oxygenu
 * @param molTotal konecny pocet molekul
 */
void genOxygen(params param, int i, int molTotal){
    int ox_num = i+1;

    sem_wait(writeSem);
        (*processCount)++;
        fprintf(file,"%d: O %d: started\n",*processCount, ox_num);
        fflush(file);
    sem_post(writeSem);

    usleep(genTimeAtom(param.TI));

    sem_wait(writeSem);
        (*processCount)++;
        fprintf(file,"%d: O %d: going to queue\n",*processCount, ox_num);
        fflush(file);
    sem_post(writeSem);

    for(;;){
        sem_wait(mutex); //prvy atom prejde, ostatne cakaju
        (*oxygenCount)++;
        if(*hydrogenCount >= 2){ //kontroluje ci je dostatocny pocet atomov na vytvorenie molekule
            sem_post(hydrogenQ); //ak ano, singalom prepusti 2 cakajuce hydrogeny
            sem_post(hydrogenQ);
            (*hydrogenCount) -=2; 
            sem_post(oxygenQ);
            (*oxygenCount)--;
        }else if(molTotal == *moleculeCount){ //osetrenie nedostatocneho poctu atomov
            sem_wait(writeSem);
                (*processCount)++;
                fprintf(file,"%d: O %d: not enough H\n",*processCount, ox_num);
                fflush(file);
            sem_post(writeSem);
            (*oxygenCount)--;
            if(*oxygenCount != 0){
                sem_post(oxygenQ);
            }
            sem_post(mutex);
            exit(0);
        }else{ //nebol dostatocny pocet, posle mutex dalsiemu
            sem_post(mutex);
        }

        sem_wait(oxygenQ); //cakanie v queue
        if(molTotal == *moleculeCount){ //nedostatocny pocet atomov
            sem_wait(writeSem);
                (*processCount)++;
                fprintf(file,"%d: O %d: not enough H\n",*processCount, ox_num);
                fflush(file);
            sem_post(writeSem);
            (*oxygenCount)--;
            if(*oxygenCount != 0){ // ak nie je posledny, posle signal aby sa ukoncili aj zvysne
                sem_post(oxygenQ);
            }
            exit(0);
        }
        genMolecule(ox_num, true, param.TB); //vytvaranie molekule
        sem_wait(moleculeCreating); //cakanie na ukoncenie hydrogenov po vytvoreni molekule
        sem_wait(moleculeCreating);
        sem_post(mutex); // posle singal mutex aby sa mohla zacat vytvarat nova molekula
        break;
    }

    if(molTotal == *moleculeCount){ //po vytvoreni poslednej molekule
            sem_post(hydrogenQ); //vysle signal na uvolnenie hydrogenu, ktory moze cakat v queue
            sem_post(oxygenQ); // vysle signal na uvolnenie oxygenu
    }
    exit(0);
}

/**
 * @brief spracovanie procesov hydrogen
 * 
 * @param param vstupne parametre programu
 * @param i poradove cislo hydrogenu
 * @param molTotal konecny pocet molekul
 */
void genHydrogen(params param, int i, int molTotal){
    int hy_num = i+1;

    sem_wait(writeSem);
        (*processCount)++;
        fprintf(file,"%d: H %d: started\n",*processCount, hy_num);
        fflush(file);
    sem_post(writeSem);

    usleep(genTimeAtom(param.TI));

    sem_wait(writeSem);
        (*processCount)++;
        fprintf(file,"%d: H %d: going to queue\n",*processCount, hy_num);
        fflush(file);
    sem_post(writeSem);

    for(;;){
        sem_wait(mutex);
        (*hydrogenCount)++;
        if(*hydrogenCount >= 2 && *oxygenCount >= 1){
            sem_post(hydrogenQ);
            sem_post(hydrogenQ);
            (*hydrogenCount)-=2;
            sem_post(oxygenQ);
            (*oxygenCount)--;
        }else if(molTotal == *moleculeCount){
            sem_wait(writeSem);
                (*processCount)++;
                fprintf(file,"%d: H %d: not enough O or H\n",*processCount, hy_num);
                fflush(file);
            sem_post(writeSem);
            (*hydrogenCount)--;
            sem_post(mutex);
            exit(0);
        }else{
            sem_post(mutex);
        }

        sem_wait(hydrogenQ);
        if(molTotal == *moleculeCount){
            sem_wait(writeSem);
                (*processCount)++;
                fprintf(file,"%d: H %d: not enough O or H\n",*processCount, hy_num);
                fflush(file);
            sem_post(writeSem);
            (*hydrogenCount)--;
            if(*hydrogenCount != 0){
                sem_post(hydrogenQ);
            }
            exit(0);
        }
        genMolecule(hy_num, false, param.TB);
        sem_post(moleculeCreating); //po vytvoreni molekule, posle signal oxygenu
        break;
    }
    exit(0);
}

/**
 * @brief vypocet ocakavaneho poctu molekul
 * 
 * @param param vstupne parametre
 * @return int pocet molekul
 */
int countTotalMol(params param){
    int i = 0;
    int no =  param.NO;
    int nh = param.NH;
    while(true){
        if(no >= 1 && nh >=2){
            no--;
            nh -=2;
            i++;
        }else{
            break;
        }
    }
    return i;
}

int main(int argc, char **argv){
    //kontrola a ulozenie parametrov
    params param = argCheck(argc, argv, param);

    //cistenie pamate pre pripad, ze pri predchadzajucom spusteti uvolnenie zlyhalo
    freeSem();
    freeMemory();

    //vystupny subor
    file = fopen("proj2.out", "w+");
    if(file==NULL){
        fprintf(stderr,"Failed to open file");
        exit(1);
    }

    int molTotal = countTotalMol(param);

    //nastavenie zdielanej pamate a semaforov
    sharedMemSet();
    semSet();
    
    //vytvaranie procesov oxygen
    pid_t IDoxygens;
    for (int i = 0; i < param.NO; i++){
            IDoxygens = fork();
        if(IDoxygens == 0){
            genOxygen(param,i,molTotal);
        }else if(IDoxygens < 0){
            fprintf(stderr, "Oxygen fork has failed");
        }
    }

    //vytvaranie procesov hydrogen
    pid_t IDhydrogens;
    for (int i = 0; i < param.NH; i++){
        IDhydrogens = fork();
        if(IDhydrogens == 0){
            genHydrogen(param,i,molTotal);
        }else if(IDhydrogens < 0){
            fprintf(stderr, "Hydrogen fork has failed");
        }
    }

    //cakanie na ukoncenie vytvorenych procesov
    while(wait(NULL) > 0);
    freeMemory(); //uvolnovaie pamate
    freeSem(); //uvolnovanie semaforov
    return 0;
}