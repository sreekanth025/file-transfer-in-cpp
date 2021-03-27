#include <bits/stdc++.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>

using namespace std;

#define PORT 3000
#define MAX_LEN 4096
#define PACKET_SIZE 4096

void *ftp_client_instance(void *connection);
void client_send_file(int conn_fd, string username, string filename);
void client_receive_file(int conn_fd, string username, string filename);
bool check_file_exists(string filepath);
void get_user_files(int conn_fd);
void send_delete_file(int conn_fd, string filename);


int main() {

    int socket_fd;
    struct sockaddr_in server_address, client_address;
    char buffer[MAX_LEN];

    // create a socket with IPv4 (PF_INET) and TCP protocol (SOCK_STREAM)
    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        printf("Some error occurred during the creation of socket\n");
        exit(1);
    } else {
        printf("Socket created successfully\n");
    }

    // clear the memory for sever_address.
    memset(&server_address, 0, sizeof(server_address));

    // Set IP family, IP address and port number
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(PORT);

    if (connect(socket_fd, (struct sockaddr*) &server_address, sizeof(server_address)) != 0) {
        printf("Some error occurred while connecting to server\n");
        exit(1);
    } else {
        printf("Connected to server\n");
    }

    pthread_t client_thread;
    printf("Accepted the client connection request\n");
    pthread_create(&client_thread, NULL, ftp_client_instance, (void *)&socket_fd);
    pthread_join(client_thread, NULL);

    return 0;
}

void *ftp_client_instance(void *connection) {

    int socket_fd = *((int *)connection);
    char buffer[MAX_LEN];

    cout << "\n";

    // clear the buffer and receive the message from server.
    // receive hello message
    memset(buffer, 0, sizeof(buffer));
    recv(socket_fd, buffer, sizeof(buffer), 0);
    printf("Server      : %s\n", buffer);

    // scan and send username and password
    memset(buffer, 0, sizeof(buffer));
    printf("You (client): ");
    fgets(buffer, sizeof(buffer), stdin);
    // strncpy(buffer, "user_1 pass_1", 13);
    send(socket_fd, buffer, sizeof(buffer), 0);

    char uname[MAX_LEN], pass[MAX_LEN];
    sscanf(buffer, "%s %s", uname, pass);
    string username(uname), password(pass);
    // cout << "username: " << username << " || password: " << password << "\n";

    // receive authentication result
    memset(buffer, 0, sizeof(buffer));
    recv(socket_fd, buffer, sizeof(buffer), 0);
    printf("Server      : %s\n", buffer);

    if(strncmp(buffer, "ERROR", 5) == 0) {
        cout << "closing instance\n";
        return NULL;
    }

    // receive rule format
    memset(buffer, 0, sizeof(buffer));
    recv(socket_fd, buffer, sizeof(buffer), 0);

    string rules(buffer);
    cout << rules << "\n";


    while (1) {

        cout << "\nEnter your operation:\n";
        // scan operation and filename
        memset(buffer, 0, sizeof(buffer));
        printf("You (client): ");
        fgets(buffer, sizeof(buffer), stdin);

        while(string(buffer) == "\n") {
            fgets(buffer, sizeof(buffer), stdin);
        }

        send(socket_fd, buffer, sizeof(buffer), 0);

        if(strncmp(buffer, "exit", 4)==0) {
            break;
        }

        char operation[MAX_LEN], filename[MAX_LEN];
        sscanf(buffer, "%s %s", operation, filename);
        string op(operation), file_name(filename);

        if(op == "upload") {
            client_send_file(socket_fd, username, file_name);
        } 
        else if(op == "download") {
            client_receive_file(socket_fd, username, file_name);
        } 
        else if (op == "get_file_list") {
            get_user_files(socket_fd);
        }
        else if (op == "delete") {
            send_delete_file(socket_fd, file_name);
        }
        else if(op == "exit") {
            cout << "Closing the connection..\n";
            break;
        } 
        else {
            // receive failure message
            memset(buffer, 0, sizeof(buffer));
            recv(socket_fd, buffer, sizeof(buffer), 0);
            cout << buffer << "\n";
        }
    }
    

    return NULL;
}


void client_receive_file(int conn_fd, string username, string filename) {
    
    char buffer[MAX_LEN];

    // receive opeartion acceptance status
    memset(buffer, 0, sizeof(buffer));
    recv(conn_fd, buffer, sizeof(buffer), 0);
    cout  << "Server      : " << buffer << "\n";

    if(strncmp(buffer, "ERROR", 5)==0) {
        return;
    }

    // Receive file size
    memset(buffer, 0, sizeof(buffer));
    recv(conn_fd, buffer, sizeof(buffer), 0);
    // cout  << "Server      : " << buffer << "\n";

    int file_size;
    sscanf(buffer, "%d", &file_size);
    cout << "File size   : " << file_size << " bytes\n";


    char cwd[MAX_LEN];
    if(getcwd(cwd, sizeof(cwd)) == NULL) {
        cout << "Some error occurred during getcwd()\n";
        return;
    }

    string filepath = string(cwd) + "/clients_storage/" + string(username) + "/" + filename;
    // cout << filepath << "\n";


    /* ************************************* */
    // Handling file with same name in the target location.

    bool file_already_exists = check_file_exists(filepath);

    if(file_already_exists) {
        cout << "File with same name already exists\n";
        cout << "Do you want to replace ? (yes/no): ";
        
        string m;
        cin >> m;

        if(m == "yes") {
            string msg = "SUCCESS: proceed";
            send(conn_fd, msg.c_str(), MAX_LEN, 0);
        }
        else {
            string msg = "ERROR: stop";
            send(conn_fd, msg.c_str(), MAX_LEN, 0);
            cout << "Aborting operation..\n";
            return;
        }
    }
    else {
        string msg = "SUCCESS: proceed";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
    }

    /* ************************************* */


    FILE *received_file;
    if((received_file = fopen(filepath.c_str(), "w")) == NULL) {
        cout << "some error occured during file creation\n";
    }

    int remain_data = file_size;
    int len, rec_bytes=0;

    cout << "downloading : ";
    while((remain_data > 0) && ((len = recv(conn_fd, buffer, PACKET_SIZE, 0)) > 0)) {
        fwrite(buffer, sizeof(char), len, received_file);
        rec_bytes += len;
        remain_data -= len;
        cout << (100*rec_bytes/file_size) << "% ";
    }
    cout << "\n";
    fclose(received_file);
    cout << "File downloaded successfully\n";
    return;
}

void client_send_file(int conn_fd, string username, string filename) {

    char buffer[MAX_LEN];
    char cwd[MAX_LEN];

    if(getcwd(cwd, sizeof(cwd)) == NULL) {
        // Send operation failure message
        string msg = "ERROR: Some error occurred during getcwd()\n";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        cout << msg << "\n";
        return;
    }

    string directory(cwd);
    string user_file = "clients_storage/" + username + "/" + filename;
    string file_path = directory + "/" + user_file;

    // cout << "File path: " << file_path << "\n";
    FILE *f;
    if((f = fopen(file_path.c_str(), "rb")) == NULL) {
        // Send operation failure message
        string msg = "ERROR: file not exists\n";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        cout << msg << "\n";
        return;
    }

    // Send operation acceptance status
    string m = "SUCCESS: file opened successfully";
    send(conn_fd, m.c_str(), MAX_LEN, 0);

    struct stat file_stat;
    if(stat(file_path.c_str(), &file_stat) == -1) {
        cout << "ERROR: some error occurred during extracting file stats\n";
        return;
    }


    /* ************************************* */
    // Handling file with same name in the target location.

    // receive message and check for any issue
    memset(buffer, 0, sizeof(buffer));
    recv(conn_fd, buffer, sizeof(buffer), 0);

    if(strncmp(buffer, "ISSUE", 5)==0) {
        cout << buffer;
        
        string msg;
        cin >> msg;
        send(conn_fd, msg.c_str(), MAX_LEN, 0);

        if(msg != "yes") {
            cout << "Aborting operation..\n";
            return;
        }
    }
    else {
        string msg = "okay";
        send(conn_fd, msg.c_str(), MAX_LEN, 0);
        // Sending message just to follow the protocol
    }

    /* ************************************* */



    string file_size = to_string(file_stat.st_size);
    cout << "File-size: " << file_size << " bytes\n";

    // send file size
    send(conn_fd, file_size.c_str(), MAX_LEN, 0);

    int fd = open(file_path.c_str(), O_RDONLY);
    int sent_bytes, offset = 0;
    int remain_data = file_stat.st_size;

    int finished_bytes = 0;
    cout << "uploading: ";

    while (((sent_bytes = sendfile(conn_fd, fd, NULL, PACKET_SIZE)) > 0) && (remain_data > 0)) {
        remain_data -= sent_bytes;
        // cout << "remaining bytes: " << remain_data << "\n";

        finished_bytes += sent_bytes;
        cout << ((100*finished_bytes)/atoi(file_size.c_str())) << "% ";
    }
    cout << "\n";

    return;
}

bool check_file_exists(string filepath) {
    if( access(filepath.c_str(), F_OK ) == 0 ) {
        return true;
    } else
        return false;
}

void get_user_files(int conn_fd) {

    char buffer[MAX_LEN];
    memset(buffer, 0, sizeof(buffer));
    recv(conn_fd, buffer, sizeof(buffer), 0);
    cout  << "Server      : " << buffer << "\n";

    return;
}

void send_delete_file(int conn_fd, string filename) {

    char buffer[MAX_LEN];
    memset(buffer, 0, sizeof(buffer));

    // Receive delete status
    recv(conn_fd, buffer, sizeof(buffer), 0);
    cout << buffer << "\n";
}