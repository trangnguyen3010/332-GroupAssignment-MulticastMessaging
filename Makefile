run:
	gcc server.c -o server
	gcc client.c -o client
	gcc sender.c -o sender
clean:
	ls | grep -v "\.\|Makefile" | xargs rm