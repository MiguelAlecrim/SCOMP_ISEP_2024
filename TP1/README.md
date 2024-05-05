# US 2001
### Descrição da User Story
Como Product Owner, quero que os sistemas processem continuamente os arquivos produzidos pelo
Applications Email Bot, para que possam ser importados no sistema por iniciativa do Operador

## Funcionalidades e percentagem funcional
| Funcionalidade | Percentagem                                    |
|----------------|------------------------------------------------|
| AC1            | 80% - não utiliza funções exec                 |
| AC2            | 100%                                           |
| AC3            | 50% - não copia os ficheiros                                          |
| AC4            | 100%                                           |
| AC5            | 50% - nao lista ficheiros                                     |
| AC6            | 100%                                           |
| AC7            | 50% - é configurável através do próprio código |

- AC1: The “Applications File Bot” must be developed in C and utilize processes, signals,
  pipes, and exec function primitives
- AC2: A child process should be created to periodically monitor an input directory for new
  files related to the 'Application' phase of the recruitment process. If new files are
  detected, a signal should be sent to the parent process.
- AC3: Upon receiving a signal, the parent process should distribute the new files among a
  fixed number of worker child processes. Each child process will be responsible for
  copying all files related to a specific candidate to its designated subdirectory in the
  output directory.
- AC4: Once a child has finished copying all files for a candidate, it should inform its parent
  that it is ready to perform additional work. Child workers do not terminate unless they
  are specifically terminated by the parent process.
- AC5: Once all files for all candidates have been copied, the parent process should
  generate a report file in the output directory. This report should list, for each
  candidate, the name of the output subdirectory and the names of all files that were
  copied.
- AC6: To terminate the application, the parent process must handle the SIGINT signal.
  Upon reception, it should terminate all children and wait for their termination.
- AC7: The names of the input and output directories, the number of worker children, the
  time interval for periodic checking of new files, etc., should be configurable. This
  configuration can be achieved either through input parameters provided when
  running the application or by reading from a configuration file.

## Diagrama de Componentes
![Diagrama de Componentes](./components.svg)