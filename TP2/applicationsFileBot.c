#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>


#define NUMBER_WORKING_CHILDREN 5
#define INPUT_DIRECTORY "Email-Bot-Output-Example"
#define OUTPUT_DIRECTORY "File-Bot-Output"
#define UPDATE_TIMEOUT 5
#define MAX_FILES 100
#define MAX_FILENAME_LENGTH 256

int pid_monitor_child;

struct shm_data {
    int count_files;
    char file_names[MAX_FILES][MAX_FILENAME_LENGTH];
    int processed[MAX_FILES];
    int number_candidates;
};

void die(const char* message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void monitor_input_dir(const char* input_dir, sem_t* semaphore) {
    sem_wait(semaphore);
    int fd, wd;
    char buf[1024];

    fd = inotify_init1(IN_NONBLOCK);
    if (fd == -1) {
        die("inotify_init1");
    }

    wd = inotify_add_watch(fd, input_dir, IN_CREATE | IN_MODIFY);
    if (wd == -1) {
        die("inotify_add_watch");
    }

    while (1) {
        ssize_t len = read(fd, buf, sizeof(buf));
        if (len == -1 && errno != EAGAIN) {
            die("read");
        }

        if (len > 0) {
            struct inotify_event *event;
            for (char *ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
                event = (struct inotify_event *) ptr;
                if (event->mask & (IN_CREATE | IN_MODIFY)) {
                    sem_post(semaphore);
                    kill(pid_monitor_child, SIGUSR1);
                }
            }
        }
        sleep(UPDATE_TIMEOUT);
    }

    close(wd);
    close(fd);

}

int copyFile(const char *input_path, const char *output_path, struct shm_data *shared_data, int count) {

    int input_fd, output_fd;
    char buffer[4096];
    ssize_t read_bytes, writen_bytes;

    input_fd = open(input_path, O_RDONLY);
    if (input_fd == -1) {
        return -1;
    }

    output_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output_fd == -1) {
        close(input_fd);
        return -1;
    }

    while ((read_bytes = read(input_fd, buffer, sizeof(buffer))) > 0) {
        writen_bytes = write(output_fd, buffer, read_bytes);
        if (writen_bytes != read_bytes) {
            close(input_fd);
            close(output_fd);
            return -1;
        }
        shared_data->processed[count] = 1;
    }

    close(input_fd);
    close(output_fd);
    return 0;
}

void copyFilesInDirectory(const char *input_dir, const char *output_dir, const char *candidate, struct shm_data *shared_data) {
    DIR *dir;
    char input_path[1024];
    char output_path[1024];

    if ((dir = opendir(input_dir)) == NULL) {
        die("opendir");
    }

    if (mkdir(output_dir, 0777) == -1 && errno != EEXIST) {
        closedir(dir);
        die("mkdir");
    }
	
    for (int i = 0; i < shared_data->count_files; i++) {
        if (strncmp(shared_data->file_names[i], candidate, strlen(candidate)) == 0 && shared_data->processed[i] == 0) {
            snprintf(input_path, sizeof(input_path), "%s/%s", input_dir, shared_data->file_names[i]);
            snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, shared_data->file_names[i]);
			printf("input: %s, output: %s\n", input_path, output_path);
            if (copyFile(input_path, output_path, shared_data, i) == -1) {
                die("copyFile");
            }

        }
    }

    closedir(dir);
}

void fillSharedMemory(const char *directory, struct shm_data *shared_data) {
    DIR *dir;
    struct dirent *ent;
    shared_data->number_candidates = -1;

    if ((dir = opendir(directory)) == NULL) {
        die("opendir");
    
    }

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG) {
            if (shared_data->count_files >= MAX_FILES) {
                die("Too many files in the directory");
            }

            char *filename = strdup(ent->d_name);
            if (filename == NULL) {
                die("strdup");
            }

            shared_data->processed[shared_data->count_files] = 0;
            strcpy(shared_data->file_names[shared_data->count_files], filename);

            char *token = strtok(filename, "-");
            if (token && isdigit(token[0])) {
                int currentNumber = atoi(token);
                if (currentNumber > shared_data->number_candidates) {
                    shared_data->number_candidates = currentNumber;
                }
            }
            shared_data->count_files++;
            free(filename);
        }
    }

    closedir(dir);
}

void handle_candidate_process(int candidate, sem_t* semaphore, struct shm_data *shared_data) {
    sem_wait(semaphore);

    char numberString[20];
    char output_dir[1024];

    sprintf(numberString, "%d", candidate);
    //printf("Number String: %s\n", numberString);
    snprintf(output_dir, sizeof(output_dir), "%s/%s", OUTPUT_DIRECTORY, numberString);

    copyFilesInDirectory(INPUT_DIRECTORY, output_dir, numberString, shared_data);
    
    sem_post(semaphore);
}

void generateReport(struct shm_data *shared_data){

    char report_path[MAX_FILENAME_LENGTH];
    snprintf(report_path, MAX_FILENAME_LENGTH, "%s/report.txt", OUTPUT_DIRECTORY);

    FILE *report_file = fopen(report_path, "w");
    if (!report_file) {
        perror("Failed to create report file");
        return;
    }

    fprintf(report_file, "Files Copied:\n");
    for (int i = 0; i < shared_data->count_files; i++) {
        if (shared_data->processed[i]) {
            fprintf(report_file, "%s\n", shared_data->file_names[i]);
        }
    }

    fclose(report_file);
}

int main(){

    //criar memoria partilhada e inicializar
    int fd, data_size = sizeof(struct shm_data);
    struct shm_data *shared_data;
    sem_t *semaphore;
    

    fd = shm_open("/shared_memo", O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        die("shm_open");
    }

    if (ftruncate(fd, data_size) == -1) {
        die("ftruncate");
    }

    shared_data = (struct shm_data*) mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_data == MAP_FAILED) {
        die("mmap");
    }

    shared_data->count_files = 0;
    shared_data->number_candidates = 0;
    
    //cria semaforo
    semaphore = sem_open("/sema", O_CREAT | O_EXCL, 0644, 1);
    if (semaphore == SEM_FAILED){
        munmap(shared_data, data_size);
        close(fd);
        shm_unlink("/shared_memo");
        die("sem_open");
    }

    //cria processo filho para monitorizar alterações na pasta de input
    sem_wait(semaphore);
    pid_monitor_child = fork();
    if (pid_monitor_child == -1) {
        die("Failed to fork");
    } else if (pid_monitor_child == 0) {
        monitor_input_dir(INPUT_DIRECTORY, semaphore);
        exit(0);
    }
    sem_post(semaphore);

    //vai buscar o numero de candidatos
    sem_wait(semaphore);
    fillSharedMemory(INPUT_DIRECTORY, shared_data); //em vez disto vai buscar o nome de todos os ficheiros 
                                             //e guarda na memoria partilhada
	//printf("Number candidates: %d\n", shared_data->number_candidates);
	//printf("Number files: %d\n", shared_data->count_files);
    if (shared_data->number_candidates == -1) {                                        
        die("Failed to get the number of candidates");
    }

    //Criação dos processos filhos para processar os arquivos
    pid_t children[NUMBER_WORKING_CHILDREN];
    int candidate[shared_data->number_candidates];
    int current_candidate = 1;

    for (int i = 0; i < NUMBER_WORKING_CHILDREN; i++) {
        if (current_candidate <= shared_data->number_candidates) {
            candidate[i] = current_candidate++;
        } else {
            candidate[i] = 0;
        }
        
        children[i] = fork();
        if (children[i] == -1) {
            die("fork");
        } else if (children[i] == 0) { //usar barriers???
            handle_candidate_process(candidate[i], semaphore, shared_data);
            exit(EXIT_SUCCESS);
        }
    }
    sem_post(semaphore);
    
    //fechar semaforo
    sem_unlink("/sema");
    
    //gera relatorio
	generateReport(shared_data);

    munmap(shared_data, data_size);
    close(fd);
    shm_unlink("/shared_memo");

    return 0;

}
