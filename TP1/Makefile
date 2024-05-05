aplication_file_bot_runnable: aplicationFileBot.o
		gcc aplicationFileBot.o -z noexecstack -o aplication_file_bot_runnable
		
aplicationFileBot.o: aplicationFileBot.c
		gcc -Wall -Wextra -fanalyzer -c aplicationFileBot.c -o aplicationFileBot.o
			
run: aplication_file_bot_runnable
		./aplication_file_bot_runnable
		
clean: 
		rm *.o aplication_file_bot_runnable
