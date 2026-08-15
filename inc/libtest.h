#ifndef LIBTEST_H
# define LIBTEST_H

# define GREEN "\x1b[32m"
# define RED "\x1b[91m"
# define YELLOW "\x1b[33m"
# define RESET "\x1b[0m"

typedef void (*t_test_func)(void);

typedef struct s_assertion
{
			
}	t_assertion;

typedef struct s_test
{
	char		*name;
	t_test_func	*func;
	t_assertion	*assertions;
	size_t		assertions_count;
}	t_test;

typedef struct s_context {
	t_test_func	current_test;
}	t_context;

t_context	*Context(void);
t_test		*Tester(void);

# define ASSERTION_TRUE(condition) 

#endif
