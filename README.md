To run this program first make it with make or make clean. make clean removes the binary files.

Next type ./loadbalancer LISTENPORT
where LISTENPORT is the port number you want to listen to requests on 

After this the user can input the server ports that 
the loadbalancer will use to handle request.

In addition the user can enable two optional arguments -R and -N followed by numbers.
R is the number of requests to recheck the servers
and N is the number of threads to run

if R request haven't been recieved for 5 seconds then the servers will be probed. 