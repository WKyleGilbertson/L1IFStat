IDIR = ./inc
CC = gcc
#CFLAGS = -I
CURRENT_HASH := '"$(shell git rev-parse HEAD)"'
#CURRENT_DATE := '"$(shell date /t)"'
CURRENT_DATE := '"$(shell date +"%Y%m%dT%H%M%SZ")"'
CURRENT_NAME := '"L1IFStat"'


L1IFStat: L1IFStat.c
	$(CC) L1IFStat.c -DCURRENT_HASH=$(CURRENT_HASH) \
	-DCURRENT_DATE=$(CURRENT_DATE) -DCURRENT_NAME=$(CURRENT_NAME) \
	-lftd2xx -o L1IFStat
clean:
#	rm *.exe
	rm L1IFStat
