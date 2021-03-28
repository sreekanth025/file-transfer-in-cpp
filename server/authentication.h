#include <bits/stdc++.h>
#include <fstream>

using namespace std;

// defining secret key for encryption and decryption
#define SECRET_KEY "4gRG9lIiwiaWFoxNTE2MjM5MD"
#define MAX_LEN 4096


// map<string, string> passwords = {
//     {"user_1", "pass_1"},
//     {"user_2", "pass_2"}
// };

string encrypt(string msg) {

    string key(SECRET_KEY);
    for(int i=0; i<msg.size(); i++) {
        msg[i] ^= key[i%key.size()];
    }
    return msg;
}


string decrypt(string msg) {
    return encrypt(msg);
}


map<string, string> get_passwords() {

    map<string, string> passwords;
    std::ifstream auth_file("auth.txt");

    string uname, pass;
    while(auth_file >> uname >> pass) {
        passwords[decrypt(uname)] = decrypt(pass);
    }
    
    return passwords;
}


int add_user(string uname, string pass) {

    char cwd[MAX_LEN];
    if(getcwd(cwd, sizeof(cwd)) == NULL) {
        string msg = "ERROR: Some error occurred during getcwd()\n";
        cout << msg << "\n";
        return -1;
    }

    string p_directory(cwd);

    string dirname = p_directory + "/clients_storage/" + uname + "/";
    if(mkdir(dirname.c_str(), 0777)==0) {
        cout << "Directory created\n";
    } else {
        cout << "Some error occured, folder not created\n";
        return -1;
    }

    // string command = "mkdir -p /clients_storage/" + uname;
    // system(command.c_str());

    string enc_uname = encrypt(uname);
    string enc_pass = encrypt(pass);

    std::ofstream auth_file("auth.txt", std::ios_base::app);
    auth_file << enc_uname << " " << enc_pass << "\n";

    return 1;
}