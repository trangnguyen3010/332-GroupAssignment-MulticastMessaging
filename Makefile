run:
	gcc server.c -o server
	gcc sender.c -o sender
	gcc rev.c -o rev
	gcc senderDemo.c -o senderDemo

clean:
	ls | grep -v "\.\|Makefile" | xargs rm