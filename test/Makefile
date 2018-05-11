INCLUDE = ../include
CFLAGS = -Wall -Werror -g -O2 -D_M64_ -I$(INCLUDE)

#CFLAGS += -DRT_SCHEDULE
#CFLAGS += -DPROD_BATCH

spsc_test: ../include/*.h spsc_test.c
	gcc ${CFLAGS} spsc_test.c -o  spsc_test -lpthread

clean:
	rm -f spsc_test
