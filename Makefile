##
## OS PROJECT, 2025
## MiniShell
## File description:
## Makefile
##

CFLAGS	=	-W -Wall -Wextra -I./include/

SRC	=	$(shell find src -type f -name '*.c')

OBJ	=	${SRC:.c=.o}

TARGET	=	mysh

all: $(TARGET)

$(TARGET):	$(OBJ)
	$(CC) -o $(TARGET) $(OBJ)

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(TARGET)

re: fclean all
