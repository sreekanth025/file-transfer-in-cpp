## Give permission to run shell scripts

```bash
chmod +x server.sh
chmod +x client.sh
```

# Usage

Open a terminal in the current working directory 
and execute the following command to start the server.

```bash
./server.sh
```


To start a client open a new terminal 
and execute the following commands

```bash
./client.sh
```

    - we can connect to any number of clients
    - Commands format will be displayed to the client once
      the connection is established and user logged in.
 
## Valid usernames and passwords:
user_1 pass_1
user_2 pass_2

    - We can add more users by adding username and passwords in 
      the "server/authentication.h" file and create folders 
      in the server and client for the respective user.


## Commands / Rules format:
```
    upload   <filename>     -->  upload a file to the server
    download <filename>     -->  download a file from the server
    delete   <filename>     -->  delete a file from the server
    mode     <char/binary>  -->  change the mode of transfer
    get_file_list           -->  get the list of files of the current user
                                 (which are present in the server)
    get_rule_format         -->  get the rule/commands format    
    exit                    -->  close the connection with the server
```


If a file is already present in the target location (Receiving side)
with the same file name, then it will prompt the client for replacing 
the existing file. User can enter an "yes" or "no".

The percentage of completion (for both uploading and downloading)
is displayed on the client side.

Code also handles the following:
```
  - Privacy and protection of files from other users
  - Invalid username or password
  - Detection and handling of invalid commands
  - Cases in which file is not present with the given filename
```