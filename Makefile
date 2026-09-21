NAME		= libtest.a
CC			= cc
CFLAGS		= -Wall -Wextra -Werror -std=c99 -pedantic
CPPFLAGS	= -I $(INC_DIR) -MMD -MP

SRC_DIR		= src
OBJ_DIR		= obj
INC_DIR		= inc

SRCS		= $(SRC_DIR)/runner.c $(SRC_DIR)/assert.c \
			  $(SRC_DIR)/isolate.c $(SRC_DIR)/signal_name.c \
			  $(SRC_DIR)/options.c $(SRC_DIR)/select.c \
			  $(SRC_DIR)/tags.c $(SRC_DIR)/capture.c
OBJS		= $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS		= $(OBJS:.o=.d)

EXAMPLE		= example/run_tests
TEST_BIN	= tests/run_tests

all: $(NAME)

$(NAME): $(OBJS)
	ar rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(EXAMPLE): example/test_example.c $(NAME)
	$(CC) $(CFLAGS) -I $(INC_DIR) $< -L. -ltest -o $@

example: $(EXAMPLE)
	./$(EXAMPLE) $(ARGS)

$(TEST_BIN): tests/test_libtest.c $(NAME)
	$(CC) $(CFLAGS) -I $(INC_DIR) $< -L. -ltest -o $@

test: $(TEST_BIN)
	./$(TEST_BIN) $(ARGS)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME) $(EXAMPLE) $(TEST_BIN)

re: fclean all

print-%:
	@echo '$* = [$($*)]'

-include $(DEPS)

.PHONY: all example test clean fclean re
