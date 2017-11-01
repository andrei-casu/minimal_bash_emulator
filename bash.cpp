#include <sys/types.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <fstream>

#define SOCK "socketpair"
#define FIFO "fifo"
#define DMAX 10240
#define log(x) std::cout <<x<<'\n'

char msg[DMAX];

class Bash {
  bool logged;
  char users[10][DMAX];
  int noUsers;
  char path[DMAX];
  void finishPath();
  char *runCD(const char* command);
public:
  Bash();
  char *run(const char * command);
};

class Message {
  const char *type;
  int *object;
  int w, r;
  int participant;
public:
  Message(const char *type);
  void connect(int participant);
  void send(const char *);
  char *get();
};

Message::Message(const char *type) {
  this->type = type;
  if (strcmp(type, SOCK) == 0) {
    object = new int[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, object) < 0) { 
      perror("Err... socketpair"); 
      _exit(1); 
    }
  }
  else {
    mknod("fifo1", S_IFIFO | 0666, 0);
    mknod("fifo0", S_IFIFO | 0666, 0);
  }
}

Bash::Bash() {
  strcpy(path, "~");
  logged = false;

  std::ifstream fin("./users");
  noUsers = 0;
  while (fin >>users[noUsers++]);
  fin.close();
}

void Bash::finishPath() {
  if (path[strlen(path)-1] != '/')
    strcat(path, "/");
}

char *Bash::runCD(const char* command) {
  const char *afterCD = command + 3;
  if (afterCD[0] == '/' || afterCD[0] == '~') {
    strcpy(path, afterCD);
    return "";
  }
  else if (strcmp(afterCD, "..") == 0 || strcmp(command, "cd .") == 0) {
    finishPath();
    strcat(path, afterCD);
    return "";
  }
  else if (afterCD[0] == '.') {
    finishPath();
    strcat(path, afterCD);
    return "";
  }
  else {
    // char *output = run("ls -d */ | cut -f1 -d'/'");
    finishPath();
    strcat(path, afterCD);
    return "";
  }
}

char *Bash::run(const char *command) {
  if (logged == false) {
    if (strstr(command, "login") != NULL) {
      const char *name = command + 6;
      int i;
      for (i = 0; i < noUsers; ++i) {
        if (strcmp(users[i], name) == 0) {
          logged = true;
          return "Success";
        }
      }
      return "User does not exist";
    }
    else if (strcmp(command, "quit") == 0) {
      exit(0);
    }
    return "You need to login before running any commands:\nRun login <username>";
  }
  if (strstr(command, "cd ") != NULL) {
    return runCD(command);
  }
  else if (strstr(command, "quit")) {
    logged = false;
    return "logout";
  }
  char finalComm[DMAX] = "cd ";
  strcat(finalComm, path); strcat(finalComm, " && "); strcat(finalComm, command);

  int link[2];
  pipe(link);
  
  pid_t pid= fork();
  if (pid == 0){
    dup2(link[1], 1);

    close(link[0]); close(link[1]);
    char *args[DMAX] = {
      "/bin/bash",
      "-c",
      finalComm,
      NULL
    };
    execvp("/bin/bash", args);
  }
  else {
    char *output = new char[DMAX];
    close(link[1]);
    read(link[0], output, DMAX);
    close(link[0]);
    return output;
  }
}

void Message::connect(int participant) {
  if (strcmp(type, FIFO) == 0) {
    char from[100], to[100];
    strcpy(from, "fifo"); from[4] = participant + '0'; from[5] = 0;
    strcpy(to, "fifo"); to[4] = (1 - participant) + '0'; to[5] = 0;

    r = open(from, O_RDONLY | O_NONBLOCK);
    w = open(to, O_WRONLY);

    int flags = fcntl(r, F_GETFL);
    flags = flags ^ O_NONBLOCK;
    fcntl(r, F_SETFL, flags);
  }
  else if (strcmp(type, SOCK) == 0) {
    this->participant = participant;
    if (this->participant == 0) {
      close(object[0]);
      r = w = object[1];
    }
    else if (this->participant == 1) {
      close(object[1]);
      r = w = object[0];
    }
  }
}

void Message::send(const char *msg) {
  write(w, msg, strlen(msg)+1);
}

char *Message::get() {
  read(r, msg, DMAX);
  return msg;
}

int main(int argc, char *argv[]) { 
  int child; 
  Message m(argv[1]);

  if ((child = fork()) == -1) perror("fork error"); 
  else if (child) {  
    m.connect(1);

    char command[DMAX];
    char *output;
    while (2) {
      gets(command);
      command[strlen(command)] = 0;
      m.send(command);
      output = m.get();
      log(output);
    }
  } 
  else { 
    Bash bash;
    m.connect(0);
    while (2) {
      char *command;
      command = m.get();
      m.send(bash.run(command));
    }
  } 
  return 0;
} 