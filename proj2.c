#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>

// Defining functions for creating and destroying shared variabiles.
#define MMAP(pointer){(pointer) = mmap(NULL, sizeof(*(pointer)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);} 
#define UNMAP(pointer) {munmap((pointer), sizeof((pointer)));}

FILE *file;

// Semaphores.

sem_t *s_line[3];

sem_t *logged = NULL;

sem_t *main_process = NULL;

sem_t *call = NULL;

sem_t *s_customer = NULL;

// Shared variabiles.

int *customer_count = NULL;

int *officer_count = NULL;

int *actual_cus_num = NULL;

int *activity_line = NULL;

bool *closed = NULL;

int *line = NULL;

// Declaration of those function so it can be used in fuction create_semaphores or create_shared_variabiles.
void destroy_semaphores();
void destroy_shared_variabiles();

// Function for customer to wait some time before entering the office.
void function_customer_wait(int wait)
{
    usleep((rand()%(wait + 1))* 1000);
}

// Function for officer break.
void function_officer_sleep(int obreak)
{
    usleep((rand()%(obreak + 1)) * 1000);
}

// Waiting time till the office will be closed.
void function_closing_time(int timing)
{
    sem_wait(main_process);
    srand(time(NULL));
    usleep((rand() % ((timing/2)+1) + (timing/2)) * 1000);
    sem_wait(logged);
    fprintf(file,"%i: closing\n",++(*line)); // Print to output that the office is closing. 
    fflush(file);
    sem_post(logged);
    *closed = true;           // Set shared variabile to true, signaling all the processes that office is closed now.
    sem_post(main_process);
}

// Creating all semaphores as well as checking if allocation of some of those semaphores failed.
void create_semaphores()
{
    s_line[0] = sem_open("/xjanek05_line_1",O_CREAT, 0666, 0);
    s_line[1] = sem_open("/xjanek05_line_2",O_CREAT, 0666, 0);
    s_line[2] = sem_open("/xjanek05_line_3",O_CREAT, 0666, 0);

    logged = sem_open("/xjanek05_log",O_CREAT, 0666, 1);
    main_process = sem_open("/xjanek05_main_process",O_CREAT, 0666, 1);
    s_customer = sem_open("/xjanek05_s_customer",O_CREAT, 0666, 0);
    call = sem_open("/xjanek05_call",O_CREAT, 0666, 0);

    if( main_process == SEM_FAILED || s_line[0]== SEM_FAILED || s_line[1] == SEM_FAILED ||  s_line[2] == SEM_FAILED || s_customer == SEM_FAILED
    || logged == SEM_FAILED || call == SEM_FAILED )
    {
        fprintf(stderr,"Error while creating semaphores");
        destroy_semaphores();
        destroy_shared_variabiles();
        exit(1);
    }
}

// This function deallocates all created semaphores.
void destroy_semaphores()
{

    sem_close(s_line[0] );
    sem_unlink("/xjanek05_line_1");

    sem_close(s_line[1]);
    sem_unlink("/xjanek05_line_2");

    sem_close(s_line[2]);
    sem_unlink("/xjanek05_line_3");

    sem_close(logged);
    sem_unlink("/xjanek05_log");

    sem_close(main_process);
    sem_unlink("/xjanek05_main_process");

    sem_close(call);
    sem_unlink("/xjanek05_call");

    sem_close(s_customer);
    sem_unlink("/xjanek05_s_customer");
}

// Function to create all shared variabiles, also checks if allocation was sucesfull.
void create_shared_variabiles()
{
    MMAP(customer_count);
    MMAP(officer_count);
    MMAP(closed);
    MMAP(actual_cus_num);
    activity_line = mmap(NULL, 3 * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    MMAP(line);
    

    if (customer_count == MAP_FAILED || officer_count == MAP_FAILED || closed == MAP_FAILED || actual_cus_num == MAP_FAILED || line == MAP_FAILED || activity_line == MAP_FAILED)
    {
        perror("Error: Problem with allocating shared variabiles");
        destroy_semaphores();
        destroy_shared_variabiles();
        exit(1);
    }
}

// This function deallocated all shared variabiles.
void destroy_shared_variabiles()
{
    UNMAP(customer_count);
    UNMAP(officer_count);
    UNMAP(closed);
    UNMAP(actual_cus_num);
    UNMAP(line);
    munmap(activity_line, 3 * sizeof(*activity_line));
}

void officer(int oc,int off_break)
{   
    sem_wait(logged);
    fprintf(file,"%i: U %i: started\n",++(*line),oc);
    fflush(file);
    sem_post(logged);

    while( 1 )
    {

        if ( (*closed) == true && (*actual_cus_num) == 0)
        {

            sem_wait(logged);
            fprintf(file,"%i: U %i: going home \n",++(*line),oc);
            fflush(file);
            sem_post(logged);
            exit(0);

        }
        if( (*actual_cus_num) == 0 )
        {
            sem_wait(logged);
            fprintf(file,"%i: U %i: taking break \n",++(*line),oc);
            fflush(file);
            sem_post(logged);

            function_officer_sleep(off_break);

            sem_wait(logged);
            fprintf(file,"%i: U %i: break finished \n",++(*line),oc);
            fflush(file);
            sem_post(logged);

        }
        else
        {
            for(int m = 0; m < 3;m++)
            {
                if ((activity_line[m]) > 0)
                {
                    activity_line[m]--;
                    (*actual_cus_num)--;
                    sem_post(s_line[m]);

                    sem_wait(s_customer);          

                    sem_wait(logged);
                    fprintf(file,"%i: U %i: serving a service of type %i \n",++(*line),oc,m+1);
                    fflush(file);
                    sem_post(logged);

                    sem_post(call);                
                    sem_wait(s_line[m]);

                    usleep((rand() % 10 + 1)*1000);

                    sem_wait(logged);
                    fprintf(file,"%i: U %i: service finished \n",++(*line),oc);
                    fflush(file);
                    sem_post(logged);
                    break;
                }
            }
        }
    }
    exit(0);
}



void customer(int cc,int wait)
{
    sem_wait(logged);
    fprintf(file,"%i: Z %i: started\n",++(*line), cc);
    fflush(file);
    sem_post(logged);

    function_customer_wait(wait);

    if( (*closed) == true )
    {
        sem_wait(logged);
        fprintf(file,"%i: Z %i: going home\n",++(*line),cc);
        fflush(file);
        sem_post(logged);
        exit(0);
    }

    srand(getpid() * time(NULL));
    int service = rand() % 3 + 1;

    (*actual_cus_num)++;
    activity_line[service-1]++;
    sem_wait(logged);
    fprintf(file,"%i: Z %i: entering office for a service %i\n",++(*line), cc, service);
    fflush(file);
    sem_post(logged);

    sem_wait(s_line[service-1]);
    sem_post(s_customer);
    sem_post(s_line[service-1]);

    sem_wait(call);

    sem_wait(logged);
    fprintf(file,"%i: Z %i: called by office worker\n",++(*line), cc);
    fflush(file);
    sem_post(logged);

    usleep((rand() % 10 + 1)*1000);
 
    sem_wait(logged);
    fprintf(file,"%d: Z %d: going home\n",++(*line), cc);
    fflush(file);
    sem_post(logged);

    exit(0);
}   

int main(int argc, char *argv[]) 
{
    destroy_semaphores();
    destroy_shared_variabiles();
    // Checking all the arguments if there were written correctly as well as storing them in variabile arg1....arg5.
    if(argc < 6 || argc > 6){
        fprintf(stderr, "Error: Count of arguments has to be equal 6\n");
        exit(1);
    }

    char *argument_1;
    int arg1 = strtol(argv[1], &argument_1, 10);
    if (*argument_1 != '\0' || argument_1 == argv[1]) {
        fprintf(stderr, "Error: Count of customers has to be an intiger.\n");
        exit(1);
    }
    
    char *argument_2;
    int arg2 = strtol(argv[2], &argument_2, 10);
    if (*argument_2 != '\0' || argument_2 == argv[2]) {
        fprintf(stderr, "Error: Count of officers has to be an intiger.\n");
        exit(1);
    }
    if (arg2 <= 0)
    {
        fprintf(stderr,"Error: Officer count has to be greater than 0\n");
        exit(1);
    }

    char *argument_3;
    int arg3 = strtol(argv[3], &argument_3, 10);
    if (*argument_3 != '\0' || argument_3 == argv[3]) {
        fprintf(stderr, "Error: Waiting time of customer must be an integer.\n");
        exit(1);
    }
    if (arg3 < 0 || arg3 > 10000)
    {
        fprintf(stderr,"Error: Waiting time of customer has to be from range 0<=X<=10000\n");
        exit(1);
    }

    char *argument_4;
    int arg4 = strtol(argv[4], &argument_4, 10);
    if (*argument_4 != '\0' || argument_4 == argv[4]) {
        fprintf(stderr, "Error: Break time of officer must be an integer.\n");
        exit(1);
    }
    if (arg4 < 0 || arg4 > 100)
    {
        fprintf(stderr,"Error: Break of officer has to be from range 0<=X<=100\n");
        exit(1);
    }

    char *argument_5;
    int arg5 = strtol(argv[5], &argument_5, 10);
    if (*argument_5 != '\0' || argument_5 == argv[5]) {
        fprintf(stderr, "Error: Time for office to close must be an integer.\n");
        exit(1);
    }
    if (arg5 < 0 || arg5 > 10000) {
        fprintf(stderr, "Error: Time for office to close has to be from range 0<=X<=10000\n");
        exit(1);
    }

    // Opening file for output.

    file = fopen("proj2.out","a");
    setbuf(file,NULL);

    // Creating semaphores and shared variabiles.
    create_semaphores();
    create_shared_variabiles();

    // Adding arguments into shared variabile.

    *customer_count = arg1;    // Count of customers.
    *officer_count = arg2;     // Count of officers.
    int customer_sleep = arg3; // Waiting time before entering office.
    int officer_sleep = arg4;  // Officer break time.
    int closing_time = arg5;   // Time for closing the office.

    // Creating my own shared variabile

    *closed = false;         // If office is closed or open.
    activity_line[0] = 0;    // Count of ppl in letter line.
    activity_line[1] = 0;    // Count of ppl in package line.
    activity_line[2] = 0;    // Count of ppl in money line.
    *actual_cus_num = 0;     // Actual count of people that came before closing.
    *line = 0;               // Variabile which is used to increase printing.

    // Forking processes of customers.
    for (int i = 1; i <= (*customer_count) ;i++)
    {
        pid_t bla =fork();
        if(bla < 0)
        {
            fprintf(stderr,"Error: Problem while forking customer");
            destroy_semaphores();
            destroy_shared_variabiles();
            fclose(file);
            exit(1);
        }
        if(bla == 0)
        {
            customer(i,customer_sleep);
        }
    }

    // Forking processes of officers.
    for (int j = 1; j <= (*officer_count) ;j++)
    {
        pid_t blak =fork();
        if (blak < 0)
        {
            destroy_semaphores();
            destroy_shared_variabiles();
            fprintf(stderr,"Error: Problem while forking officer");
            fclose(file);
            exit(1);
        }
        if(blak == 0)
        {
            officer(j,officer_sleep);
        }
    }

    // Function which waits till office is closed.
    function_closing_time(closing_time);

    // Waiting for kids.
    while(wait(NULL)>0);

    // Destroying all shared variabiles and semaphores.
    destroy_semaphores();
    destroy_shared_variabiles();
    fclose(file);
    
    exit(0);
}
