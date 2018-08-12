PTHREADPOOLPATH=$(shell pwd)/pthreadpool
HANDSOCKETPATH=$(shell pwd)/handsocket
HTTPSERVERPATH=$(shell pwd)
HTTPSERVEROBJ=$(shell ls $(HTTPSERVERPATH)/*.c | sed 's/.c/.o/g'|sed 's/.*\///g')
PTHREADPOOLOBJ=$(shell ls $(PTHREADPOOLPATH)/*.c | sed 's/.c/.o/g'|sed 's/.*\///g')
HANDSOCKETOBJ=$(shell ls $(HANDSOCKETPATH)/*.c | sed 's/\.c/.o/g'|sed 's/.*\///g')
INCLUDE=Ipthreadpool/ -Ihandsocket/
LODLIB=lpthread
CC=gcc
httpserver:$(HTTPSERVEROBJ) $(PTHREADPOOLOBJ) $(HANDSOCKETOBJ)
	$(CC) -g -o $@ $^ -$(INCLUDE) -$(LODLIB)
%.o:$(PTHREADPOOLPATH)/%.c
	$(CC) -g -c $<   
%.o:$(HANDSOCKETPATH)/%.c
	$(CC) -g -c $<   
%.o:$(HTTPSERVERPATH)/%.c
	$(CC) -g -c $<   -$(INCLUDE)
.PHONY:debug
debug:
	@echo $(PTHREADPOOLPATH)
	@echo $(HTTPSERVERPATH)
	@echo $(HANDSOCKETPATH)
	@echo $(PTHREADPOOLOBJ)
	@echo $(HTTPSERVEROBJ)
	@echo $(HANDSOCKETOBJ)
.PHONY:clean
clean:
	rm -r httpserver *.o
