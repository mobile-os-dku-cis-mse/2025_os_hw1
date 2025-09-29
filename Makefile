MAKEFLAGS += --silent

SRC	=	main/main.c	\
		src/shell.c	\
		utils/strncat.c


BUILD_DIR = build/

$(BUILD_DIR)%.o: %.c
	@mkdir -p $(@D)
	#@echo "  CC       $<      $@"
	@$(CC) $(CFLAGS) -c $< -o $@

OBJ	= 	$(SRC:%.c=$(BUILD_DIR)%.o)

NAME	=	SiSH

CFLAGS 	= -I include/ -Wall -Wextra -g

all:	$(NAME)

$(NAME):	$(OBJ)
		gcc -o $(NAME) $(SRC) $(CFLAGS) -g
	@ echo "SiSH compiled"

clean:
		rm -f $(OBJ)
	@ echo "clean done"

fclean: clean
		rm -f $(NAME)
	@ echo "fclean done"

re: 	fclean all

.PHONY: all $(NAME) clean fclean re .SILENT