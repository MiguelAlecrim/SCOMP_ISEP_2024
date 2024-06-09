# Makefile for applicationsFileBot
# variables

SOURCES = applicationsFileBot.c
OBJFILES = applicationsFileBot.o
EXEC = applicationsFileBot

# Suffix rules
.SUFFIXES : .c .o

.c.o:

	gcc -g -Wall -Wextra -fanalyzer -c $<

${EXEC}: ${OBJFILES}
	gcc ${OBJFILES} -o ${EXEC} -lpthread -lrt

${OBJFILES}: ${SOURCES}

run: ${EXEC}
	./${EXEC} ${ARGS}

clean:
	rm -f ${OBJFILES} ${EXEC}	

force_clean: clean
	-killall ${EXEC} 2>/dev/null || true

