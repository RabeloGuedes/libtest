NAME		= libtest.a
VERSION		= 0.9.0
CC			= cc
CFLAGS		= -Wall -Wextra -Werror -std=c99 -pedantic
CPPFLAGS	= -I $(INC_DIR) -MMD -MP

SRC_DIR		= src
OBJ_DIR		= obj
INC_DIR		= inc

SRCS		= $(SRC_DIR)/runner.c $(SRC_DIR)/assert.c \
			  $(SRC_DIR)/isolate.c $(SRC_DIR)/signal_name.c \
			  $(SRC_DIR)/options.c $(SRC_DIR)/select.c \
			  $(SRC_DIR)/tags.c $(SRC_DIR)/capture.c \
			  $(SRC_DIR)/exec.c
OBJS		= $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS		= $(OBJS:.o=.d)

EXAMPLE		= example/run_tests
TEST_BIN	= tests/run_tests

# Where install puts things. DESTDIR is prepended, for packaging.
PREFIX		= /usr/local
INCLUDEDIR	= $(PREFIX)/include
LIBDIR		= $(PREFIX)/lib

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

# Only the public header: lt_internal.h stays out of an install.
install: $(NAME)
	mkdir -p $(DESTDIR)$(INCLUDEDIR) $(DESTDIR)$(LIBDIR)
	cp $(INC_DIR)/libtest.h $(DESTDIR)$(INCLUDEDIR)/
	cp $(NAME) $(DESTDIR)$(LIBDIR)/

uninstall:
	rm -f $(DESTDIR)$(INCLUDEDIR)/libtest.h
	rm -f $(DESTDIR)$(LIBDIR)/$(NAME)

version:
	@echo $(VERSION)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME) $(EXAMPLE) $(TEST_BIN)

re: fclean all

print-%:
	@echo '$* = [$($*)]'

-include $(DEPS)

.PHONY: all example test install uninstall version clean fclean re
