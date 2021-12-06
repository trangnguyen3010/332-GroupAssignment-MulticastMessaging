run:
	gcc -Wall -Wextra -o server server.c -lpthread
	gcc -Wall -Wextra -o client client.c -lpthread
	gcc -Wall -Wextra -o sender sender.c -lpthread
clean:
	ls | grep -v "\.\|Makefile" | xargs rm