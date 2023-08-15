IDIR = /git/GPS/L1IFStream/Code/inc
CC = CL
#CFLAGS = -I

L1IFStat.exe: L1IFstat.c
	$(CC) L1IFStat.c
	rm *.obj
clean:
	rm *.exe
