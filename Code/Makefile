COMPILER = gcc
OPTION = -Wall -g
FILE = main.o command_executor.o command_finder.o command_splitter.o error_printer.o prompt_printer.o

myshell: $(FILE)
	$(COMPILER) $(OPTION) -o myshell $(FILE)

clear:
	rm -f *.o myshell