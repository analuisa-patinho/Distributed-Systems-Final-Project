## Ana Luísa Patinho


FUNCIONAMENTO:
1) When doing make the .o files will go in the object folder, the libraries will go in the lib folder, and the executable files will be in the binary folder
2) The .o and executable files will not be deleted from your directory unless you are told to, i.e. by running the "make clean" command.
	2.1) the "make clean" command will remove all files created by the "make" command
3) Temporary files will be deleted from all directories after the "make" call
4) The names of the executables will be "tree-client" "tree-server".
	4.1) To run the server use the command "./binary/tree-server <Port> <IP ADDRESS>:2181", for example: ./binary/tree-server 1234 127.0.0.1:2181
	4.1.1) Or use the command "make run-server1".
	4.2) To run the client use the command "./binary/tree-client <IP ADDRESS>:2181", for example: ./binary/tree-client 127.0.0.1:2181 
		4.2.1) Or with the use of the command "make run-client".
5) The ProtoBuffer is generated with the "proto" command.

IMPLEMENTATION LIMITATIONS:
 1) Some memory loss!