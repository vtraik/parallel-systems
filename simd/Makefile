EXEC = polynomial_mult
EXEC_O1 = polynomial_mult_O1

polynomial_mult:
	gcc -o $(EXEC) polynomial_mult.c -mavx2 -Wall -Wpedantic -Wextra -Warray-bounds -DO0
	gcc -o $(EXEC_O1) polynomial_mult.c -mavx2 -Wall -Wpedantic -Wextra -Warray-bounds -DO1
clean:
	rm -f $(EXEC) $(EXEC_O1)
