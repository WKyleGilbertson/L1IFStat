IDIR = ./inc
CC = CL
#CFLAGS = -I
CURRENT_HASH := '"$(shell git rev-parse HEAD)"'
CURRENT_DATE := '"$(shell date /t)"'
CURRENT_NAME := '"L1IFStat"'


L1IFStat: L1IFstat.c
	$(CC) L1IFStat.c /EHsc /DCURRENT_HASH=$(CURRENT_HASH) \
	/DCURRENT_DATE=$(CURRENT_DATE) /DCURRENT_NAME=$(CURRENT_NAME)
	rm *.obj
clean:
	rm *.exe
