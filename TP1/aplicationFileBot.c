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

int time_to_go = 1;
int pid_monitor_child;
int number_candidates;


#define NUMBER_OF_WORKING_CHILDS 5
#define INPUT_DIRECTORY "Email-Bot-Output-Example"
#define OUTPUT_DIRECTORY "File-Bot-Output"
#define UPDATE_TIMEOUT 1000
#define WORKER_CHILD_CHECK_TIMEOUT 50


void die(char* message){
  fprintf(stderr, message);
 	exit(EXIT_FAILURE);
}

void handle_signal_monitor (int sig) {
    printf("Child Handled SIGINT\n");
    time_to_go = 0;
    exit(sig);
}

void handle_signal (int sig) {
    printf("Parent Handled SIGINT\n");
    kill(pid_monitor_child, SIGINT);
    exit(sig);
}

void handle_signal_newFiles (int sig) {
  write(STDOUT_FILENO, "New file detected in directory \n", 33);
}


void monitor_input_dir(const char* input_dir, int is_monitor) {
	/* to verify this is the monitor child */
	if (!is_monitor) {
		return;
	}

	/* file/watch descriptors */
	int fd, wd;
	char buf[1024];

	/* inotify */
	fd = inotify_init();
	if (fd == -1) {
		die("inotify_init:");
	}

	wd = inotify_add_watch(fd, input_dir, IN_CREATE);
	if (wd == -1) {
		die("inotify_add_watch:");
	}

	while(time_to_go != 0) {
		printf("Monitoring directory %s for new files...\n", input_dir);

		/* monitor the directory, send signal to parent if detects a change */
		ssize_t len = read(fd, buf, sizeof(buf));
		if (len == -1) {
			die("read:");
		}

		struct inotify_event *event;
		for (char *ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
			event = (struct inotify_event *) ptr;
			if (event->mask & IN_CREATE) {
				/* send signal */
        kill(getppid(), SIGUSR1);
			}
		}

		sleep(UPDATE_TIMEOUT);
	}

	close(fd);
}

int copyFile(const char *srcPath, const char *destPath, const char *prefix) {
    FILE *sourceFile, *destFile;
    char buffer[BUFSIZ];
    size_t bytes;

    // Open source file for reading
    sourceFile = fopen(srcPath, "rb");
    if (sourceFile == NULL) {
        perror("Error opening source file");
        return 1;
    }

    // Extract filename from srcPath
    char *filename = strrchr(srcPath, '/');
    if (filename == NULL) {
        filename = (char *)srcPath;
    } else {
        filename++;
    }

    // Check if the filename starts with the specified prefix
    if (strncmp(filename, prefix, strlen(prefix)) != 0) {
    
        fclose(sourceFile);
        return 0; 
    }

    // Construct full destination path
    char destFilepath[strlen(destPath) + strlen(filename) + 2]; 
    sprintf(destFilepath, "%s/%s", destPath, filename);

    // Open destination file for writing
    destFile = fopen(destFilepath, "wb");
    if (destFile == NULL) {
        perror("Error creating destination file");
        fclose(sourceFile);
        return 1;
    }

    // Copy contents from source to destination
    while ((bytes = fread(buffer, 1, BUFSIZ, sourceFile)) > 0) {
        fwrite(buffer, 1, bytes, destFile);
    }

    // Close files
    fclose(sourceFile);
    fclose(destFile);

    return 0;
}

int copyFilesInDirectory(const char *sourceDir, const char *destDir, const char *prefix) {
    DIR *dir;
    struct dirent *ent;
    char srcPath[1024];

    // Open source directory
    if ((dir = opendir(sourceDir)) != NULL) {

              // Ensure the destination directory exists
        if (mkdir(destDir, 0777) == -1) {
            if (errno != EEXIST) {
                perror("Error creating destination directory");
                closedir(dir);
                return 1;
            }
        }

        // Iterate through each entry in the directory
        while ((ent = readdir(dir)) != NULL) {
          
          if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
              continue;
          }

          // Construct full path for source file
          sprintf(srcPath, "%s/%s", sourceDir, ent->d_name);

          // Copy the file if it meets the criteria
          copyFile(srcPath, destDir, prefix);

        }
        closedir(dir);
    } else {
        // Could not open directory
        perror("Error opening source directory");
        return 1;
    }

    return 0;
}

int getNumberLastCandidate(const char *directory) {
    DIR *dir;
    struct dirent *ent;
    int lastNumber = -1; // Default value indicating failure

    // Open the directory
    if ((dir = opendir(directory)) != NULL) {
        // Iterate through each entry in the directory
        while ((ent = readdir(dir)) != NULL) {
        
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }

            // Check if the first character is a numeric digit
            if (isdigit(ent->d_name[0])) {
                // Convert the first character to an integer
                int currentNumber = ent->d_name[0] - '0';
                // Update the last number if it's greater than the current one
                if (currentNumber > lastNumber) {
                    lastNumber = currentNumber;
               }
            }
        }
        closedir(dir);
    } else {
        // Could not open directory
        perror("Error opening directory");
    }

    return lastNumber;
}

int main() {
    
  int pfd[2]; 
  int pfd2[2]; 
  pid_t pid_monitor_child;     // Process ID
  struct sigaction act;
  struct sigaction act2;
  
    // Create the pipes
  if (pipe(pfd) == -1) {
      perror("pipe");
      exit(EXIT_FAILURE);
  }
  if (pipe(pfd2) == -1) {
      perror("pipe2");
      exit(EXIT_FAILURE);
  }
  
  memset(&act, 0, sizeof(struct sigaction)); 
  act.sa_flags = SA_RESTART;

  memset(&act2, 0, sizeof(struct sigaction)); 
  act2.sa_flags = SA_RESTART;

  pid_monitor_child = fork();
  
  if (pid_monitor_child == -1) {
      perror("fork monitor");
      exit(EXIT_FAILURE);
  }
  
  if (pid_monitor_child == 0) { // Child process
    act.sa_handler = handle_signal_monitor; 
    sigaction(SIGINT, &act, NULL); 
    
    close(pfd[0]); // Close the read end of the pipe
    close(pfd[1]); // Close the write end of the pipe
    close(pfd2[0]); // Close the read end of the pipe
    close(pipefd2[1]); // Close the write end of the pipe

    monitor_input_dir(INPUT_DIRECTORY, 1);
  
    exit(EXIT_SUCCESS);
  } else if (pid_monitor_child > 0) { // Parent process

    act.sa_handler = handle_signal_newFiles; 
    sigaction(SIGUSR1, &act, NULL); 

    memset(&act2, 0, sizeof(struct sigaction)); 
    act2.sa_flags = SA_RESTART;
    act2.sa_handler = handle_signal; 
    sigaction(SIGINT, &act2, NULL); 


    number_candidates = getNumberLastCandidate(INPUT_DIRECTORY);

    if (number_candidates != -1) {
      printf("First letter (as integer) of the last file: %d\n", number_candidates);
    } else {
      printf("Error occurred while getting the first letter of the last file.\n");
    }

    int candidate[NUMBER_OF_WORKING_CHILDS]; 
    int contador_candidato = 1; 

    for (int p = 0; p < number_candidates && p < NUMBER_OF_WORKING_CHILDS; p++)
    {
      candidate[p] = contador_candidato;
      contador_candidato++;
    }


           // Fork child processes
    for (int i = 0; i < NUMBER_OF_WORKING_CHILDS; i++) {

      int pid = fork();

      if (pid == -1) {
          perror("fork worker childs");
          exit(EXIT_FAILURE);
      }

      if (pid == 0) { // Child process
        close(pipefd[1]); // Close the write end of the pipe 1
        close(pipefd2[0]); // Close the read end of the pipe 2

        char numberString[20];
        char output_dir[1024] = OUTPUT_DIRECTORY;
        int candidato[NUMBER_OF_WORKING_CHILDS];
        int lastvalue = 0;

        while (lastvalue == 0) {

          if (read(pipefd[0], &candidato, sizeof(candidato)) == -1) {
            perror("read");
            exit(EXIT_FAILURE);
          }

          printf("\nArray Child: %d\n", getpid());
              for (int wir = 0; wir < NUMBER_OF_WORKING_CHILDS; wir++) {
                printf("%d : %d\n", i, candidate[wir]);
              }



              // Convert integer to string
            sprintf(numberString, "%d", candidato[i]);
            strcat(output_dir, "/");
            strcat(output_dir, numberString);

            copyFilesInDirectory(INPUT_DIRECTORY, output_dir, numberString);

            candidato[i] = 0;

            if (write(pipefd2[1], &candidato, sizeof(candidato)) == -1) {
              perror("write");
              exit(EXIT_FAILURE);
            }

          
          sleep(WORKER_CHILD_CHECK_TIMEOUT);

        }
        

        close(pfd[0]); // Close the read end of the pipe 1
        close(pfd2[1]); // Close the write end of the pipe 2
        exit(EXIT_SUCCESS);

      } else if (pid > 0) {

        if (write(pipefd[1], &candidate, sizeof(candidate)) == -1) {
          perror("write");
          exit(EXIT_FAILURE);
        }

        if (i == number_candidates-1 && number_candidates < NUMBER_OF_WORKING_CHILDS) {
          pause();

          number_candidates = getNumberLastCandidate(INPUT_DIRECTORY);

          for (int p = contador_candidato-1; p < number_candidates && p < NUMBER_OF_WORKING_CHILDS; p++) {
            candidate[p] = contador_candidato;
            contador_candidato++;
          }

        } else if (i == NUMBER_OF_WORKING_CHILDS -1) {

          while (0<1) {
          

          if (read(pfd2[0], &candidate, sizeof(candidate)) == -1) {
            perror("read");
            exit(EXIT_FAILURE);
          }

          for (int c = 0; c < NUMBER_OF_WORKING_CHILDS; c++) {

            if (candidate[c] == 0){
              
              while (contador_candidato > number_candidates) {
                pause();
                number_candidates = getNumberLastCandidate(INPUT_DIRECTORY);
              }

              candidate[c] = contador_candidato;
              contador_candidato++;
              
              if (write(pipefd[1], &candidate, sizeof(candidate)) == -1) {
                perror("write");
                exit(EXIT_FAILURE);
              }

              printf("\nArray Parent: %d\n", getpid());
              for (int wir = 0; wir < NUMBER_OF_WORKING_CHILDS; wir++) {
                printf("%d\n", candidate[wir]);
              }

            }
          }
            
          }
        }
      }

    }
    close(pfd[0]); // Close the read end of the pipe 1 
    close(pfd2[1]); // Close the write end of the pipe 2
    

    pause();

    close(pfd[1]); // Close the write end of the pipe 1
    close(pfd2[0]); // Close the read end of the pipe 2
    exit(EXIT_SUCCESS);
  }
}
