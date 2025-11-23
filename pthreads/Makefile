CFLAGS = -Wall -Wextra -g #-O2
CC = gcc
LIBS = -pthread


POL_TAR = polynomial_mult
POL_SRC = polynomial_mult.c
POL_OBJS = $(POL_SRC:.c=.o)
POL_LIBS = $(LIBS) -lm

SHVAR_TAR = update_shared_var
SHVAR_SRC = update_shared_var.c 
SHVAR_OBJS = $(SHVAR_SRC:.c=.o)

STATS_TAR = stats 
STATS_SRC = stats.c
STATS_OBJS = $(STATS_SRC:.c=.o)

BANK_TAR = bank 
BANK_SRC = bank.c wrappers.c
BANK_OBJS = $(BANK_SRC:.c=.o)
BANK_LIBS = $(LIBS) -lbsd

BARRIER_TAR = barrier 
BARRIER_SRC = barrier.c sense_barrier.c
BARRIER_OBJS = $(BARRIER_SRC:.c=.o)

ALL = $(POL_TAR) $(SHVAR_TAR) $(STATS_TAR) $(BANK_TAR) $(BARRIER_TAR)

all: $(ALL)


$(POL_TAR): $(POL_OBJS)
	$(CC) $(POL_OBJS) -o $@ $(POL_LIBS)

$(SHVAR_TAR): $(SHVAR_OBJS)
	$(CC) $(SHVAR_OBJS) -o $@ $(LIBS)

$(STATS_TAR): $(STATS_OBJS)
	$(CC) $(STATS_OBJS) -o $@ $(LIBS) 

$(BANK_TAR): $(BANK_OBJS)
	$(CC) $(BANK_OBJS) -o $@ $(BANK_LIBS)

$(BARRIER_TAR): $(BARRIER_OBJS)
	$(CC) $(BARRIER_OBJS) -o $@ $(LIBS)

# Generic compile rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(ALL)
