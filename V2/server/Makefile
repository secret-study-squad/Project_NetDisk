OUT:= main
SRCS:=$(wildcard *.c)
OBJS:=$(patsubst %.c,%.o,$(SRCS))
CC := gcc
$(OUT) : $(OBJS)
	$(CC) $^ -o $@
%.o:%.c
	$(CC) -c $^ -o $@ -g
clean:
	$(RM) $(OUT) $(OBJS)
