CFLAGS = -Wall -Wextra -g
CC = mpicc 
LIBS = -fopenmp


POL_TAR = polynomial_mult
POL_SRC = polynomial_mult.c
POL_OBJS = $(POL_SRC:.c=.o)

SPARSE_TAR = sparse 
SPARSE_SRC = sparse_mat_vec.c 
SPARSE_OBJS = $(SPARSE_SRC:.c=.o)


HSPARSE_TAR = hybrid_sparse 
HSPARSE_OBJS = $(patsubst %.c,hyb_%.o,$(SPARSE_SRC))

ALL = $(POL_TAR) $(SPARSE_TAR) $(HSPARSE_TAR)

all: $(ALL)

$(POL_TAR): $(POL_OBJS)
	$(CC) $(POL_OBJS) -o $@

$(SPARSE_TAR): $(SPARSE_OBJS)
	$(CC) $(SPARSE_OBJS) -o $@

$(HSPARSE_TAR): $(HSPARSE_OBJS)
	$(CC) $(HSPARSE_OBJS) -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

hyb_%.o: %.c
	$(CC) $(CFLAGS) -DHYB $(LIBS) -c $< -o $@

clean:
	rm -f *.o $(ALL)
