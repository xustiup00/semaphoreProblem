/**************************************************************
 * 
 * IOS 2 project (synchronizace, samafory)
 * @author Polina Ustiuzhantseva (xustiup00)
 * 
 * ************************************************************/

#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <ctype.h>
#include <time.h>
#include <sys/wait.h>

#define MAP_ANONYMOUS 0x20

FILE *file; // soubor s vystupem

int L, Z, K, TL, TB; // parametry funkce
int smProcFailed = 0;

typedef struct
{
    int actionCount;          // poradove cislo provadene akce
    int actualBusStop;        // aktualni cislo zastavky
    int skiers;               // pocet procesu lyzaru, ktere jeste neskoncili
    int actualBusCapacity;    // aktualni kapacita autobusu
    int *countOfSkiersAtStop; // pole, ve kterem je ulozen aktualni pocet lyzaru pro kazdou zastavku

    sem_t printSem;        // pro vlastni funkci print
    sem_t *allowedToBoard; // pole samaforu pro povoleni nastupu pro kazdou zastavku
    sem_t boarding;        // pro skibus cekani na dokonceni nastupu lyzaru
    sem_t unboardAll;      // pro cekani lyzare na povoleni vystupovat na vystupni zastavce
    sem_t unboarded;       // pro cekani na to, kdy lyzar vystoupi

} sharedMemory;

sharedMemory *sharedMem;
int shmId;
/**
 * @brief funkce zpracuje vstupni argumenty
 *
 * @param argc pocet argumentu
 * @param argv pole argumentu
 */
void argsCheck(int argc, char *argv[])
{
    if (argc != 6)
    {
        fprintf(stderr, "The count of arguments is invalid. \n");
        exit(1);
    }

    for (int i = 1; i < argc; i++)
    {
        int j = 0;
        while (argv[i][j] != '\0')
        {
            if (isdigit(argv[i][j]) == 0)
            {
                fprintf(stderr, "Invalid argument(s) value. \n");
                exit(1);
            }
            j++;
        }
    }

    L = atoi(argv[1]);
    Z = atoi(argv[2]);
    K = atoi(argv[3]);
    TL = atoi(argv[4]);
    TB = atoi(argv[5]);

    if (L >= 20000 || L < 0 || Z <= 0 || Z > 10 || K < 10 || K > 100 || TL < 0 || TL > 10000 || TB < 0 || TB > 1000)
    {
        fprintf(stderr, "Invalid argument(s) value. \n");
        exit(1);
    }
}

/**
 * @brief funkce inicializuje sdilenou pamet a nastavi pocatecni hodnoty pro procesy a samafory
 */
void init()
{
    shmId = shmget(IPC_PRIVATE, sizeof(sharedMemory), IPC_CREAT | 0666);
    if (shmId < 0)
    {
        fprintf(stderr, "Shared memory inicialization failed.\n");
        exit(1);
    }
    sharedMem = (sharedMemory *)shmat(shmId, NULL, 0);
    if (sharedMem == (sharedMemory *)-1)
    {
        fprintf(stderr, "Shared memory inicialization failed.\n");
        exit(1);
    }

    // inicializace promennych
    sharedMem->actionCount = 1;
    sharedMem->actualBusStop = 0;
    sharedMem->skiers = L;
    sharedMem->actualBusCapacity = K;
    sharedMem->countOfSkiersAtStop = mmap(NULL, sizeof(int) * (Z - 1), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    for (int i = 0; i < Z - 1; i++)
    {
        sharedMem->countOfSkiersAtStop[i] = 0;
    }

    // inicializace samaforu
    if (sem_init(&sharedMem->printSem, 1, 1) == -1)
    {
        fprintf(stderr, "Semaphore inicialization failed.\n");
        exit(1);
    }
    sharedMem->allowedToBoard = mmap(NULL, sizeof(sem_t) * (Z - 1), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    for (int i = 0; i < Z; i++)
    {
        if (sem_init(&sharedMem->allowedToBoard[i], 1, 0) == -1)
        {
            fprintf(stderr, "Semaphore inicialization failed.\n");
            exit(1);
        }
    }
    if (sem_init(&sharedMem->boarding, 1, 0) == -1)
    {
        fprintf(stderr, "Semaphore inicialization failed.\n");
        exit(1);
    }
    if (sem_init(&sharedMem->unboardAll, 1, 0) == -1)
    {
        fprintf(stderr, "Semaphore inicialization failed.\n");
        exit(1);
    }
    if (sem_init(&sharedMem->unboarded, 1, 0) == -1)
    {
        fprintf(stderr, "Semaphore inicialization failed.\n");
        exit(1);
    }
    return;
}

/**
 * @brief uklidi samafory a sdilenou pamet
 */
void cleanUp()
{
    // uvolni samafory
    if (sem_destroy(&sharedMem->printSem) == -1)
    {
        fprintf(stderr, "Semaphore destroy failed.\n");
        exit(1);
    }

    for (int i = 0; i < Z; i++)
    {
        if (sem_destroy(&sharedMem->allowedToBoard[i]) == -1)
        {
            fprintf(stderr, "Semaphore destroy failed.\n");
            exit(1);
        }
    }

    if (sem_destroy(&sharedMem->boarding) == -1)
    {
        fprintf(stderr, "Semaphore destroy failed.\n");
        exit(1);
    }

    if (sem_destroy(&sharedMem->unboardAll) == -1)
    {
        fprintf(stderr, "Semaphore destroy failed.\n");
        exit(1);
    }

    if (sem_destroy(&sharedMem->unboarded) == -1)
    {
        fprintf(stderr, "Semaphore destroy failed.\n");
        exit(1);
    }

    // uvolni pole(mapy)
    if (munmap(sharedMem->countOfSkiersAtStop, sizeof(int) * (Z - 1)) == -1)
    {
        fprintf(stderr, "Map destroy failed. \n");
        exit(1);
    }

    if (munmap(sharedMem->allowedToBoard, sizeof(sem_t) * (Z - 1)) == -1)
    {
        fprintf(stderr, "Map destroy failed. \n");
        exit(1);
    }

    // odstrani vytvorenou strukturu
    if (shmdt(sharedMem) == -1)
    {
        fprintf(stderr, "Shared memory destroying failed.\n");
        exit(1);
    }
    shmctl(shmId, IPC_RMID, NULL);
}

/**
 * @brief funkce otevre soubor pro zapis
 */
void openFile()
{
    file = fopen("proj2.out", "w+");
    if (file == NULL)
    {
        fprintf(stderr, "Openning file failed. \n");
        exit(1);
    }
    return;
}

/**
 * @brief funkce ukonci praci se souborem pro zapis
 *
 */
void closeFile()
{
    if (fclose(file) == EOF)
    {
        fprintf(stderr, "Closing file failed. \n");
        exit(1);
    }
    return;
}

/**
 * @brief funkce vypise do souboru ocislovany radek s popisem akce
 * @param message string, ktery se vypiskne
 * @param a prvni promenna
 * @param b druha promenna
 */
void print(char *message, int a, int b)
{
    sem_wait(&sharedMem->printSem);
    fprintf(file, "%d: ", sharedMem->actionCount);
    if (a == 0 && b == 0)
    {
        fprintf(file, message, 0);
    }
    else if (b == 0)
    {
        fprintf(file, message, a);
    }
    else
    {
        fprintf(file, message, a, b);
    }
    fflush(file);
    sharedMem->actionCount++;
    sem_post(&sharedMem->printSem);
    return;
}

/**
 * @brief vlastni wait funce
 * @param a maximalni cas, ktery se ceka
 *
 */
void waitFunc(int a)
{
    srand(time(NULL) * getpid());
    usleep(rand() % (a + 1));
    return;
}

int main(int argc, char *argv[])
{
    // kontrola a prevzeti argumentu
    argsCheck(argc, argv);

    // inicializace mapy a samaforu
    init();

    // otevreni souboru pro zapis
    openFile();

    // Skibus proces
    pid_t skibus = fork();

    if (skibus < 0)
    {
        fprintf(stderr, "Starting skibus process failed. \n");
        cleanUp();
        exit(-1);
    }

    if (skibus == 0)
    {
        print("BUS: started\n", 0, 0);

        // pokud vsechny procesy lyzaru neskoncili
        while (sharedMem->skiers > 0 && smProcFailed == 0)
        {
            sharedMem->actualBusStop = 1;

            // pokud neni vystupni zastavka
            while (sharedMem->actualBusStop < Z)
            {
                waitFunc(TB);
                print("BUS: arrived to %d\n", sharedMem->actualBusStop, 0);                                                 // skibus prijede na zastavku
                while (sharedMem->actualBusCapacity > 0 && sharedMem->countOfSkiersAtStop[sharedMem->actualBusStop - 1] > 0) // pokud kapacita skibusu neni naplnena a pokud na zastavce cekaji nejaci lyzari
                {
                    sem_post(&sharedMem->allowedToBoard[sharedMem->actualBusStop - 1]); // povoli se nastup pro jednoho lyzarze
                    sem_wait(&sharedMem->boarding);                                     // ceka se, nez lyzar skonci nastup
                }
                print("BUS: leaving %d\n", sharedMem->actualBusStop, 0); // skibus odjede ze zastavky
                sharedMem->actualBusStop++;
            }
            waitFunc(TB);
            print("BUS: arrived to final\n", 0, 0);  // skibus prijel na vystupni zastavku
            while (sharedMem->actualBusCapacity != K) // pokud skibus neni prazdny
            {
                sem_post(&sharedMem->unboardAll); // povoli se vystup jedoho lyzarze
                sem_wait(&sharedMem->unboarded);  // ceka se, nez lyzar vystoupi
            }
            print("BUS: leaving final\n", 0, 0); // skibus opustil vystupni zastavku i pojede na prvni zastavku
        }
        if (smProcFailed == 1)
        {
            exit(1);
        }
        print("BUS: finish\n", 0, 0);

        exit(0);
    }
    if (skibus != 0)
    {
        // vytvoreni L procesu lyzaru a prideleni kazdemu nastupni zastavky
        for (int i = 1; i <= L; i++)
        {
            pid_t skierP = fork();

            if (skierP < 0)
            {
                fprintf(stderr, "Starting skyer process failed. \n");
                smProcFailed = 1;
                cleanUp();
                exit(-1);
            }

            if (skierP == 0)
            {
                print("L %i: started\n", i, 0);
                waitFunc(TL);                                          // lyzar ji snidane
                int actualBusStop = rand() % (Z - 1);                  // prideleni nahodne zastavky
                print("L %i: arrived to %i\n", i, actualBusStop + 1); // lyzar prisel na zastavku
                sharedMem->countOfSkiersAtStop[actualBusStop]++;       // zvysil se pocet cekajicich lyzaru na zastavce
                sem_wait(&sharedMem->allowedToBoard[actualBusStop]);   // ceka na povoleni nastoupit do skibusu
                print("L %i: boarding\n", i, 0);                      // nastupuje
                sharedMem->actualBusCapacity--;                        // snizi se pocet volnych mist v skibuse
                sharedMem->countOfSkiersAtStop[actualBusStop]--;       // snizi se pocet lyzarzu na zastavce
                sem_post(&sharedMem->boarding);                        // dava signal skibusu, ze nastoupil
                sem_wait(&sharedMem->unboardAll);                      // ceka na povoleni vystupu ze skibusu
                print("L %i: going to ski\n", i, 0);
                sharedMem->skiers--;             // snizi se pocet lyzaru, kterych je potreba obslouzit
                sharedMem->actualBusCapacity++;  // zvysi se kapacita skibusu
                sem_post(&sharedMem->unboarded); // dava signal skibusu, ze skoncil vystup
                exit(0);
            }
        }
    }

    while (wait(NULL) > 0)
    {
        continue;
    };
    cleanUp();

    closeFile();

    exit(0);
    return 0;
}