IDIR = ./inc
CC = CL
GCC = gcc
#CFLAGS = -I
CURRENT_HASH := '"$(shell git rev-parse HEAD)"'
CURRENT_DATE := '"$(shell date /t)"'
POSIX_DATE   := '"$(shell date +"%Y%m%dT%H%M%SZ")"'
CURRENT_NAME := '"L1IFStat"'

L1IFStat: L1IFstat.c
	$(CC) L1IFStat.c /EHsc /DCURRENT_HASH=$(CURRENT_HASH) \
	/DCURRENT_DATE=$(CURRENT_DATE) /DCURRENT_NAME=$(CURRENT_NAME)
	rm *.obj

l1ifstat: L1IFStat.c
	$(GCC) L1IFStat.c -DCURRENT_HASH=$(CURRENT_HASH) \
	-DCURRENT_DATE=$(POSIX_DATE) -DCURRENT_NAME=$(CURRENT_NAME) \
	-lftd2xx -o l1ifstat
clean:
	rm l1ifstat
	rm *.exe
