Dedupe Engine:
	The problem statement is to implement a de-duplication engine, which maintains
	information about what data is already stored in the storage device and optimize
	the storage usage if duplicate data is being stored. 

	This project is for beginners who wants to learn storage term Dedupe.
	This project has client server mechanism, where server process is a daemon
	process (serverd_dedupe) that serve the requests sent from the client process.
	requests from client process will be like, write a file using the dedupe algo
	and read/restore the original file back. delete some file from the dedupe fingerprint.
	dedupe fingerprint is nothing but a chunk of data that will be used for reference while
	writing/reading the data.
	
How to build the code:
	1. clone the Repository
	2. compile the code using make command

How to run the project:
	1. start the server daemon process as below.
		#./serverd_dedupe
		this process will read on-disk dedupe chunks which will be used for reference
		while writing/reading, and it creates a in memory hash table of the read chunks.
	2. run the client utility to request write/read the data to/from dedupe chunks.
		write a file using dedupe -w option is used for write and -r is used for read/restore.

		#./util_dedupe -w -i filename
		#./util_dedupe -o -i filename -o filename_restore
