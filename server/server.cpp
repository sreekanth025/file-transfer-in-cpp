#include <bits/stdc++.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <dirent.h>
#include "authentication.h"

using namespace std;

#define PORT 3000
#define BACKLOG 5
#define MAX_CLIENTS 10000
#define MAX_LEN 4096
#define PACKET_SIZE 4096
#define int long long

void *ftp_server_instance(void *connection);
void server_send_file(int conn_fd, string username, string filename);
void server_receive_file(int conn_fd, string username, string filename, string mode);
bool check_file_exists(string filepath);
void send_user_files(int conn_fd, string username);
void server_delete_file(int conn_fd, string username, string filename);


typedef struct thread_arguments {
    int connection;
    int client_no;
} thread_arguments;

map<string, string> passwords;
pthread_t clients[MAX_CLIENTS];
int counter = 0;

int32_t main(int32_t argc, char *argv[]) {

    int socket_fd, conn_fd;
    struct sockaddr_in server_address, client_address;
    int len = sizeof(client_address);
    char buffer[MAX_LEN];
    
    // create a socket for IPv4(PF_INET) and TCP-Protocol(SOCK_STREAM)
    if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Some error occurred during the creation of socket\n");
        exit(1);
    } else {
        printf("Socket created successfully\n");
    }


    // clear the memory for sever_address.
    memset(&server_address, 0, sizeof(server_address));
    

    // Set IP family, IP address and port number
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);


    // Bind the socket with IP address and port
    if (bind(socket_fd, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        printf("Some error occurred during binding\n");
        exit(1);
    } else {
        printf("Socket binded with network\n");
    }


    // listen for incoming connction requests
    if (listen(socket_fd, BACKLOG) != 0) {
        printf("Failed to listen\n");
        exit(1);
    } else {
        printf("Server started listening\n");
    }

    
    // Accept the client connection request
    while(1) {

        if(counter >= MAX_CLIENTS) {
            printf("Cannot handle clients anymore!\n");
            break;
        }

        conn_fd = accept(socket_fd, (struct sockaddr*) &client_address, (socklen_t *) &len);
        if(conn_fd < 0) {
            printf("Failed to accept the conection request\n");
            exit(1);
        } else {
            printf("Accepted the client connection request\n");

            thread_arguments t;
            t.client_no = counter;
            t.connection = conn_fd;

            pthread_create(&clients[counter], NULL, ftp_server_instance, (void *)&t);
            counter++;
        }
    }

    for(int i=0; i<counter; i++) {
        pthread_join(clients[i], NULL);
    }

    close(socket_fd);
    return 0;
}


void *ftp_server_instance(void *arguments) {

    thread_arguments t = *((thread_arguments *)arguments);

    // default mode
    string mode = "binary";

    // get passwords
    passwords = get_passwords();

    cout << "\nCurrent users:";
    for(auto x: passwords) {
        cout << x.first << " " << x.second << "\n";
    }

    char buffer[MAX_LEN];
    int conn_fd = t.connection;

    string hello = "Provide <username> <password>";
    hello += "\n\t\t (or) new_user <username> <password>";
    send(conn_fd, hello.c_str(), MAX_LEN, 0);

    // Receive username and password
    memset(buffer, 0, sizeof(buffer));
    recv(conn_fd, buffer, sizeof(buffer), 0);
    cout << "\nReceived: " << buffer << "";

    char user[MAX_LEN], pass[MAX_LEN];
    char tmp[MAX_LEN];

    if(strncmp("new_user", buffer, 8) == 0) {
        
        if(sscanf(buffer, "%s %s %s", tmp, user, pass) != -1) {

            if(add_user(string(user), string(pass)) == -1) {
                string msg = "ERROR: cannot create user";
                send(conn_fd, msg.c_str(), MAX_LEN, 0);
                cout << msg << "\n";
                return NULL;
            } else {
                string msg = "user created, you can login now";
                send(conn_fd, msg.c_str(), MAX_LEN, 0);
                cout << msg << "\n";
                return NULL;
            }
        } 
        else {
            string msg = "ERROR: invalid format";
            send(conn_fd, msg.c_str(), MAX_LEN, 0);
            printf("Error-user-creation: closing a client (id = %lld) instance\n", t.client_no);
            return NULL;
        }
    }

    if(sscanf(buffer, "%s %s", user, pass) == -1) {
        // authentication failure
        string msg = "ERROR: invalid format";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        printf("Error-0: closing a client (id = %lld) instance\n", t.client_no);
        return NULL;
    }

    string username(user);
    string password(pass);
    
    if(username == "" || password == "") {
        // authentication failure
        string msg = "ERROR: invalid format";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        printf("Error-1: closing a client (id = %lld) instance\n", t.client_no);
        return NULL;
    }

    // cout << "username: " << username << " || password: " << password << "\n";

    if(passwords[username] != password) {
        // authentication failure
        string msg = "ERROR: invalid username or password";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        printf("Error-2: closing a client (id = %lld) instance\n", t.client_no);
        return NULL;
    }

    // authentication success
    string login_message = "SUCCESS: login success\n";
    send(conn_fd, login_message.c_str(), MAX_LEN, 0);

    // Send rule format
    string rules = "rules       : \n\t\t upload   <filename>";
    rules += "\n\t\t download <filename> ";
    rules += "\n\t\t delete   <filename> ";
    rules += "\n\t\t mode     <char/binary> ";
    rules += "\n\t\t get_file_list ";
    rules += "\n\t\t get_rule_format ";
    rules += "\n\t\t exit";

    send(conn_fd, rules.c_str(), MAX_LEN, 0);

    while(1) {

        // Receive operation and filename
        memset(buffer, 0, sizeof(buffer));
        recv(conn_fd, buffer, sizeof(buffer), 0);
        cout << "\nReceived: " << buffer << "";

        char operation[MAX_LEN], filename[MAX_LEN];
        if(sscanf(buffer,"%s %s", operation, filename) == -1) {
            string msg = "ERROR: invalid rule format";
            send(conn_fd, msg.c_str(), MAX_LEN, 0);
            printf("Error-3: invalid format, aborting operation\n");
            continue;
        }
        
        string op(operation), file_name(filename);
        
        if(op == "upload") {
            server_receive_file(conn_fd, username, file_name, mode);
        }
        else if (op == "download") {
            server_send_file(conn_fd, username, file_name);
        }
        else if (op == "get_file_list") {
            send_user_files(conn_fd, username);
        }
        else if (op == "delete") {
            server_delete_file(conn_fd, username, file_name);
        }
        else if (op == "mode") {

            if(file_name == "char") {
                mode = "char";
                string msg = "Changed to char mode";
                send(conn_fd, msg.c_str(), MAX_LEN, 0);
            }
            else if (file_name == "binary") {
                mode = "binary";
                string msg = "Changed to binary mode";
                send(conn_fd, msg.c_str(), MAX_LEN, 0);
            }
            else {
                string msg = "ERROR: Invalid format";
                send(conn_fd, msg.c_str(), MAX_LEN, 0);
            }

        }
        else if(op == "get_rule_format") {
            send(conn_fd, rules.c_str(), MAX_LEN, 0);
        }
        else if(op == "exit") {
            break;
        }
        else {
            string msg = "ERROR: invalid rule format";
            cout << msg << "\n";
            send(conn_fd, msg.c_str(), MAX_LEN, 0);
        }
    }

    printf("closing a client instance (id = %lld)\n", t.client_no);
    return NULL;
}


void server_send_file(int conn_fd, string username, string filename) {

    char buffer[MAX_LEN];
    char cwd[MAX_LEN];

    if(getcwd(cwd, sizeof(cwd)) == NULL) {
        string msg = "ERROR: Some error occurred during getcwd()\n";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        cout << msg << "\n";
        return;
    }

    string directory(cwd);
    string user_file = "clients_storage/" + username + "/" + filename;
    string file_path = directory + "/" + user_file;

    cout << "File path: " << file_path << "\n";
    bool file_exists = check_file_exists(file_path);
    if(!file_exists) {
        // Send operation failure message
        string msg = "ERROR: file not exists\n";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        cout << msg << "\n";
        return;
    }


    // // old method
    // FILE *f;
    // if((f = fopen(file_path.c_str(), "rb")) == NULL) {
    //     // Send operation failure message
    //     string msg = "ERROR: file not exists\n";
    //     send(conn_fd, msg.c_str(), MAX_LEN, 0);
    //     cout << msg << "\n";
    //     return;
    // }


    // Send operation acceptance status
    string m = "SUCCESS: file opened successfully";
    send(conn_fd, m.c_str(), MAX_LEN, 0);

    struct stat file_stat;
    if(stat(file_path.c_str(), &file_stat) == -1) {
        cout << "ERROR: some error occurred during extracting file stats\n";
        return;
    }

    string file_size = to_string(file_stat.st_size);
    cout << "File-size: " << file_size << " bytes\n";
    // send file size
    send(conn_fd, file_size.c_str(), MAX_LEN, 0);



    /* ************************************* */
    // Handling file with same name in the target location.

    memset(buffer, 0, sizeof(buffer));
    recv(conn_fd, buffer, sizeof(buffer), 0);
    cout  << "Client   : " << buffer << "\n";

    if(strncmp(buffer, "ERROR", 5)==0) {
        cout << "Aborting operation..\n";
        return;
    }

    /* ************************************* */



    int fd = open(file_path.c_str(), O_RDONLY);
    int sent_bytes, offset = 0;
    int remain_data = file_stat.st_size;

    float progress;
    int barWidth = 70;

    int finished_bytes = 0;
    cout << "uploading: \n";

    while (((sent_bytes = sendfile(conn_fd, fd, NULL, PACKET_SIZE)) > 0) && (remain_data > 0)) {
        remain_data -= sent_bytes;
        // cout << "remaining bytes: " << remain_data << "\n";

        finished_bytes += sent_bytes;
        progress = ((float) finished_bytes)/stoll(file_size);

        // Progress bar
        std::cout << "[";
        int pos = barWidth * progress;
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << (int)(progress * 100.0) << " %\r";
        std::cout.flush();
    }
    std::cout << std::endl;

    cout << "File sent successfully\n";
    return;
}

void server_receive_file(int conn_fd, string username, string filename, string mode) {
    
    char buffer[MAX_LEN];
    char cwd[MAX_LEN];

    // receive opeartion acceptance status
    memset(buffer, 0, sizeof(buffer));
    recv(conn_fd, buffer, sizeof(buffer), 0);
    cout  << "Client      : " << buffer << "\n";

    if(strncmp(buffer, "ERROR", 5)==0) {
        return;
    }

    /* ************************************* */
    // Handling file with same name in the target location.

    if(getcwd(cwd, sizeof(cwd)) == NULL) {
        // Send operation failure message
        string msg = "ERROR: Some error occurred during getcwd()\n";
        cout << msg << "\n";
        return;
    }

    string directory(cwd);
    string user_file = "clients_storage/" + username + "/" + filename;
    string filepath = directory + "/" + user_file;

    bool file_already_exists = check_file_exists(filepath);

    if(file_already_exists) {

        string msg = "ISSUE: File with same name exists.";
        msg += "\nDo you want to replace ? (yes/no): ";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);

        memset(buffer, 0, sizeof(buffer));
        recv(conn_fd, buffer, sizeof(buffer), 0);
        // cout << buffer << "\n";

        if(string(buffer) != "yes") {
            cout << "Aborting operation..\n";
            return;
        }
    }
    else {
        string msg = "No issue";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);

        memset(buffer, 0, sizeof(buffer));
        recv(conn_fd, buffer, sizeof(buffer), 0);
        // Receiving message just to follow the protocol
        // nothing to do with received message
    }

    /* ************************************* */



    // Receive file size
    memset(buffer, 0, sizeof(buffer));
    recv(conn_fd, buffer, sizeof(buffer), 0);
    // cout  << "Client      : " << buffer << "\n";
    if(strncmp(buffer, "ERROR", 5)==0) {
        cout << buffer << "\n";
        return;
    }

    int file_size;
    sscanf(buffer, "%lld", &file_size);
    cout << "File size   : " << file_size << " bytes\n";


    // char cwd[MAX_LEN];
    // if(getcwd(cwd, sizeof(cwd)) == NULL) {
    //     cout << "Some error occurred during getcwd()\n";
    // }
    // string filepath = string(cwd) + "/clients_storage/" + string(username) + "/" + filename;
    // // cout << filepath << "\n";


    FILE *received_file;

    string write_mode = "wb";
    if(mode == "char") {
        write_mode = "w";
    }

    if((received_file = fopen(filepath.c_str(), write_mode.c_str())) == NULL) {
        cout << "some error occured during file creation\n";
    }

    int remain_data = file_size;
    int len, rec_bytes=0;


    float progress;
    int barWidth = 70;



    cout << "downloading : \n";
    while((remain_data > 0) && ((len = recv(conn_fd, buffer, PACKET_SIZE, 0)) > 0)) {
        fwrite(buffer, sizeof(char), len, received_file);
        rec_bytes += len;
        remain_data -= len;
        // cout << (100*rec_bytes/file_size) << "% ";

        progress = ((float) rec_bytes)/file_size;

        // Progress bar
        std::cout << "[";
        int pos = barWidth * progress;
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << (int)(progress * 100.0) << " %\r";
        std::cout.flush();
        
    }
    std::cout << std::endl;

    // cout << "\n";
    fclose(received_file);
    cout << "File Received successfully\n";
    return;
}

bool check_file_exists(string filepath) {
    if( access(filepath.c_str(), F_OK ) == 0 ) {
        return true;
    } else
        return false;
}


void send_user_files(int conn_fd, string username) {

    char cwd[MAX_LEN];

    if(getcwd(cwd, sizeof(cwd)) == NULL) {
        // Send operation failure message
        string msg = "ERROR: Some error occurred during getcwd()\n";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        cout << msg << "\n";
        return;
    }

    string directory(cwd);
    string user_directory = directory + "/clients_storage/" + username + "/";

    // cout << "user_directory: " << user_directory << "\n";

    DIR *d;
    struct dirent *dir;

    if((d = opendir(user_directory.c_str())) == NULL) {
        // Send operation failure message
        string msg = "ERROR: cannot access the files\n";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        cout << msg << "\n";
        return;
    }

    string file_list = "\n\nYour files are: ";
    int count = 0;

    while ((dir = readdir(d)) != NULL) {
        // printf("%s\n", dir->d_name);
        string file_name(dir->d_name);

        if(file_name == "." || file_name == "..")
            continue;
            
        file_list += "\n" + file_name;
        count++;
    }

    if(count == 0) {
        file_list = "No files are present in the server for the current user";
    }

    send(conn_fd, file_list.c_str(), MAX_LEN, 0);
    return;
}


void server_delete_file(int conn_fd, string username, string filename) {
    
    char buffer[MAX_LEN];
    char cwd[MAX_LEN];

    if(getcwd(cwd, sizeof(cwd)) == NULL) {
        // Send operation failure message
        string msg = "ERROR: Some error occurred during getcwd()\n";
        cout << msg << "\n";
        return;
    }

    string directory(cwd);
    string user_file = "clients_storage/" + username + "/" + filename;
    string filepath = directory + "/" + user_file;

    bool file_exists = check_file_exists(filepath);

    if(!file_exists) {
        // Send failure status
        string msg = "ERROR: file not exists";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        cout << msg << "\n";
        return;
    }

    if(remove(filepath.c_str()) == 0) {
        // Send success status
        string msg = "file " + filename + " deleted";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        cout << msg << "\n";
        return;
    } else {
        // Send failure status
        string msg = "ERROR: file not deleted";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        cout << msg << "\n";
        return;
    }

}