CFLAGS = -Wall -Wextra -g
CC = gcc
LIBS = -fopenmp


POL_TAR = polynomial_mult
POL_SRC = polynomial_mult.c
POL_OBJS = $(POL_SRC:.c=.o)

SPARSE_TAR = sparse 
SPARSE_SRC = sparse_matr_v.c 
SPARSE_OBJS = $(SPARSE_SRC:.c=.o)

MERGE_TAR = mergesort 
MERGE_SRC = mergesort.c
MERGE_OBJS = $(MERGE_SRC:.c=.o)


ALL = $(POL_TAR) $(SPARSE_TAR) $(MERGE_TAR) 

all: $(ALL)


$(POL_TAR): $(POL_OBJS)
	$(CC) $(POL_OBJS) -o $@ $(LIBS)

$(SPARSE_TAR): $(SPARSE_OBJS)
	$(CC) $(SPARSE_OBJS) -o $@ $(LIBS) 

$(MERGE_TAR): $(MERGE_OBJS)
	$(CC) $(MERGE_OBJS) -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

clean:
	rm -f *.o $(ALL)
