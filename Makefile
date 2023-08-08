IDIR = /git/GPS/L1IFStream/Code/inc
CC = CL
#CFLAGS = -I
#CURRENT_HASH := '"$(shell git rev-parse HEAD)"'
#CURRENT_TIME := '"$(shell date +"%Y%m%dT%H%M%SZ" -u)"'
CURRENT_NAME = '"L1IFStat"'

L1IFStat.exe: L1IFstat.c
#	$(CC) L1IFStat.c -DCURRENT_NAME=$(CURRENT_NAME)
	$(CC) /DDCURRENT_NAME=\"L1IFStat\" L1IFStat.c
	rm *.obj
clean:
	rm *.exe