#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

//magic numbers and constants
#define DEFAULT_PORT_NO 54321
#define ARRAY_SIZE 10
#define STRING_LENGTH 10
#define BACKLOG 10     /* how many pending connections queue will hold */
#define RETURNED_ERROR -1
#define ARGUMENTS_EXPECTED 2
#define usernameMaxLength 10
#define passwordMaxLength 10
#define wordMaxLength 15

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

//define a structure-linked list to hold word pairs
typedef struct list {
    char firstWord[wordMaxLength];
    char secondWord[wordMaxLength];
    unsigned int wordPairNumber;
    struct list * next;
}
wordPair;

//define a structure-linked list to hold users
typedef struct node {
    char username[usernameMaxLength];
    char password[passwordMaxLength];
    int gamesWon;
    int gamesPlayed;
    struct node * next;
}
userNode;

//define a structure containing thread arguments
typedef struct threadArgs{
  userNode *users;  //pointer to head of user linked lsit
  wordPair *words;  //pointer to head of words linked list
  pthread_t tid;  //threadID
  int new_fd; //int for socket number
  int idleThread;
}
ThreadArgs;

//create array of ThreadArgs
ThreadArgs threadArgsArray[BACKLOG];

typedef void( * callbackUser)(userNode * data);
typedef void( * callbackWord)(wordPair * data);


//function declarations
char * ReceiveData(int socket_identifier, int size);
int TestCharacter(char receivedChar);


//catch and handle SIGINT ctrl+c
void SignalHandler(void) {
  //close open sockets
  for(int i = 0; i <BACKLOG; i++) {
    if(threadArgsArray[i].new_fd != 0){
    close(threadArgsArray[i].new_fd);
    }
  }
  exit(0);
}


//create array of thread attribute structures
void GenerateThreadArgs(userNode *users, wordPair *words) {
  for(int i = 0; i < BACKLOG; i++) {
    threadArgsArray[i].users = users;
    threadArgsArray[i].words = words;
    threadArgsArray[i].tid;
    threadArgsArray[i].new_fd = 0;
    threadArgsArray[i].idleThread = 1;
  }
}


// creates a userNode node
userNode * AddUserNode(userNode * next, char username[], char password[]) {
    // create new userNode to add to list
    userNode * newNode = (userNode * ) malloc(sizeof(userNode));
    if (newNode == NULL) {
        return (0);
    }
    // insert new node
    strcpy(newNode-> username, username);
    strcpy(newNode-> password, password);
    newNode-> gamesWon = 0;
    newNode-> gamesPlayed = 0;
    newNode-> next = next;
    return newNode;
}


// puts user at the top of the list
// must be used before appendUser
userNode * prependUser(userNode * head, char usrn[], char pass[]) {
    userNode * new_node = AddUserNode(head, usrn, pass);
    head = new_node;
    return head;
}


// puts user after the last user in the list
userNode * appendUser(userNode * head, char usrn[], char pass[]) {
    if (head == NULL)
        return NULL;
    /* go to the last node */
    userNode * cursor = head;
    while (cursor-> next != NULL)
        cursor = cursor-> next;
    /* create a new node */
    userNode * new_node = AddUserNode(NULL, usrn, pass);
    cursor-> next = new_node;
    return head;
}


// finds and returns user form list
userNode * searchUser(userNode * head, char usrn[]) {
    userNode * cursor = head;
    while (cursor != NULL) {
        if (strcmp(cursor-> username, usrn) == 0) {
            return cursor;
        } else {
            cursor = cursor-> next;
        }
    }
    return NULL;
}


//create a wordPair node
wordPair * AddWordPair(wordPair * next, char firstWord[], char secondWord[], \
  unsigned int wordPairCounter) {
    // create new wordPair to add to list
    wordPair * newPair = (wordPair * ) malloc(sizeof(wordPair));
    if (newPair == NULL) {
        return (0);
    }
    // insert new node
    strcpy(newPair-> firstWord, firstWord);
    strcpy(newPair-> secondWord, secondWord);
    newPair-> wordPairNumber = wordPairCounter;
    newPair-> next = next;
    return newPair;
}


// puts word at the start of the list
// must be used before appendWord
wordPair * prependWord(wordPair * head, char word1[], char word2[]) {
    wordPair * new_node = AddWordPair(head, word1, word2, 0);
    head = new_node;
    return head;
}


// puts word after the last word in the list
wordPair * appendWord(wordPair * head, char word1[], char word2[], unsigned int counter) {
    if (head == NULL)
        return NULL;
    /* go to the last node */
    wordPair * cursor = head;
    while (cursor-> next != NULL)
        cursor = cursor-> next;

    /* create a new node */
    wordPair * new_node = AddWordPair(NULL, word1, word2, counter);
    cursor-> next = new_node;

    return head;
}


// finds and returns a word pair
wordPair * searchWord(wordPair * head, int number) {
    wordPair * cursor = head;
    while (cursor != NULL) {
        if (cursor-> wordPairNumber == number) {
            return cursor;
        } else {
            cursor = cursor-> next;
        }
    }
    return NULL;
}


// counts number of word pairs in the list
int wordCount(wordPair * head) {
    wordPair * cursor = head;
    int c = 0;
    while (cursor != NULL) {
        c++;
        cursor = cursor-> next;
    }
    return c;
}


//write function to take contents of Authentication.txt to linked list
userNode * AuthenticationImport() {
    userNode * head = NULL;
    FILE * authenticationText;
    //authenticationArray holds fget string before parsing
    char authenticationArray[passwordMaxLength + usernameMaxLength];
    //parsed username from authenticationArray
    char usernameArray[usernameMaxLength];
    //parsed password from authenticationArray
    char passwordArray[passwordMaxLength];
    //initialise arrays with zeros
    for (int i = 0; i < usernameMaxLength; i++) {
        usernameArray[i] = 0;
        passwordArray[i] = 0;
        authenticationArray[i] = 0;
        authenticationArray[i + 10] = 0;
    }
    //open Authentication.txt in read mode
    authenticationText = fopen("Authentication.txt", "r");
    if (authenticationText == NULL) {
        perror("Error opening Authentication file:");
        return (-1);
    }
    //copy values in Authentication.txt to usernameArray and passwordArray, remove spaces somehow
    fgets(authenticationArray, (usernameMaxLength + passwordMaxLength), authenticationText);
    //head = prependUser(head, usernameArray, passwordArray);
    int cnt = 0;
    while (fgets(authenticationArray, (usernameMaxLength + passwordMaxLength), \
    authenticationText)) {
        //populate usernameArray using first 10 values (or until a space or indent) of authenticationArray
        for (int i = 0; i < usernameMaxLength; i++) {
            //read the username one char at a time until hitting space or indent
            if ((authenticationArray[i] != ' ') && (authenticationArray[i] != '\t')) {
                usernameArray[i] = authenticationArray[i];
            }
            //terminate string
            else {
                usernameArray[i] = '\0';
                break;
            }
            //turn read chars on line into spaces
            authenticationArray[i] = ' ';
        }
        //populate passwordArray using final values (not spaces or indents) of authenticationArray
        int passwordCounter = 0;
        for (int i = 0; i < (usernameMaxLength + passwordMaxLength); i++) {
            if ((authenticationArray[i] != ' ') && (authenticationArray[i] != '\t') \
              && (authenticationArray[i] != '\r') && (authenticationArray[i] != '\n')) {
                passwordArray[passwordCounter] = authenticationArray[i];
                authenticationArray[i] = ' ';
                passwordCounter++;
            }
        }
        // Prepend the first
        if (!cnt) {
            head = prependUser(head, usernameArray, passwordArray);
            cnt++;
        } // append the rest
        else {
            head = appendUser(head, usernameArray, passwordArray);
        }
    }
    fclose(authenticationText);
    return head;
}


wordPair * WordListImport() {
    wordPair * head = NULL;
    FILE * hangmanText;
    //awordArray holds fget string before parsing
    char wordArray[2 * wordMaxLength];
    //parsed first word from wordArray
    char firstWordArray[wordMaxLength];
    ////parsed password from authenticationArray
    char secondWordArray[wordMaxLength];
    //initialise arrays with zeros
    for (int i = 0; i < wordMaxLength; i++) {
        firstWordArray[i] = 0;
        secondWordArray[i] = 0;
        wordArray[i] = 0;
        wordArray[i + wordMaxLength] = 0;
    }
    //open Authentication.txt in read mode
    hangmanText = fopen("hangman_text.txt", "r");
    if (hangmanText == NULL) {
        perror("Error opening hangman_text file");
        return (-1);
    }
    //copy values in Authentication.txt to usernameArray and passwordArray, remove spaces somehow
    unsigned int wordPairCounter = 0;
    int cnt = 0;
    while (fgets(wordArray, sizeof(wordArray), hangmanText)) {
        //populate firstWordArray using char values (until a comma) of hangmanText
        for (int i = 0; i < wordMaxLength; i++) {
            //read the username one char at a time until hitting comma
            if (wordArray[i] != ',') {
                firstWordArray[i] = wordArray[i];
            }
            //when reaching the comma, change it to a space, terminate string, and break
            else {
                firstWordArray[i] = '\0';
                wordArray[i] = ' ';
                break;
            }
            //change each read char in wordArray to a space
            wordArray[i] = ' ';
        }
        //populate secondWordArray using final values (not spaces) of wordArray
        int secondWordCounter = 0;
        for (int i = 0; i < 2 * wordMaxLength; i++) {
            if (wordArray[i] != '\r' && wordArray[i] != '\n' && wordArray[i] != ' ') {
                secondWordArray[secondWordCounter] = wordArray[i];
                wordArray[i] = ' ';
                secondWordCounter++;
            }
        }
        wordPairCounter++;
        // prepend the first
        if (!cnt) {
            head = prependWord(head, firstWordArray, secondWordArray);
            cnt++;
        } // append the rest
        else {
            head = appendWord(head, firstWordArray, secondWordArray, wordPairCounter);
        }
    }
    fclose(hangmanText);
    return head;
}


//goes through users list executing a function of choice f
void traverseUsers(userNode * head, callbackUser f) {
    userNode * cursor = head;
    while (cursor != NULL) {
        f(cursor);
        cursor = cursor-> next;
    }
}


//goes through word list executing function of choice f
void traverseWords(wordPair * head, callbackWord f) {
    wordPair * cursor = head;
    while (cursor != NULL) {
        f(cursor);
        cursor = cursor-> next;
    }
}


//displays users from list
void displayUsers(userNode * n) {
    if (n != NULL) {
        printf("%s ", n-> username);
        printf("%s\n", n-> password);
    }
}


//sends userdata for leaderboard displaying
//only users with at least 1 game played
void sendUser(userNode * n, int new_fd) {
    if (n != NULL && n-> gamesPlayed > 0) {
        char name[usernameMaxLength];
        strcpy(name, n-> username);
        int played = n-> gamesPlayed;
        int won = n-> gamesWon;
        SendInt(new_fd, 0);
        ReceiveInt(new_fd);
        send(new_fd, name, 10, 0); // sends user name
        ReceiveInt(new_fd);
        SendInt(new_fd, won);
        ReceiveInt(new_fd);
        SendInt(new_fd, played);
        ReceiveInt(new_fd);
    }
}


//sends leaderboard data for each user
void sendLeaderboard(userNode * head, int new_fd) {
    userNode * users = head;
    while (users != NULL) {
        sendUser(users, new_fd);
        users = users-> next;
    }
    SendInt(new_fd, 1); // flag for completed send
}


// displays word pairs in list
void displayWords(wordPair * n) {
    if (n != NULL) {
        printf("%s ", n-> firstWord);
        printf("%s\n", n-> secondWord);
    }
}


//sends a string but doesnt work...
void SendString(int socket_id, char * string, int stringLength) {
    //pad out string with zeros as required
    if (stringLength < ARRAY_SIZE) {
        for (int i = stringLength; i < ARRAY_SIZE; i++) {
            string[i] = 0;
        }
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
        unsigned short networkValue = htons(string[i]);
        send(socket_id, & networkValue, sizeof(unsigned short), 0);
        // printf("String[%d] = %c\n", i, string[i]);
    }
    fflush(stdout);
}


//receives string data
char * ReceiveData(int socket_identifier, int size) {
    int numberOfBytes = 0;
    unsigned short networkValue;
    char * receivedData = malloc(sizeof(char) * size);
    for (int i = 0; i < size; i++) {
        if ((numberOfBytes = recv(socket_identifier, & networkValue, sizeof(unsigned short), 0)) == RETURNED_ERROR) {
            perror("recv");
            exit(EXIT_FAILURE);
        }
        receivedData[i] = ntohs(networkValue);
    }
    return receivedData;
}


//sends integer
void SendInt(int new_fd, int integer) {
    unsigned short netval = htons(integer);
    send(new_fd, & netval, sizeof(unsigned short), 0);
}


//receives and returns integer
int ReceiveInt(int socket_id) {
    int integer;
    unsigned short netval;
    recv(socket_id, & netval, sizeof(unsigned short), 0);
    integer = ntohs(netval);
    return integer;
}


int TestCharacter(char receivedChar) {
    if (receivedChar >= 'a' && receivedChar <= 'z') {
        return 1;
    } else {
        return 0;
    }
}


/*This is the container function for Hangman Game, Leaderboard, and quit functions.
Once a client connection is accepted, they are passed to this function.*/
void* GameLoop(void* argStruct) {
  // ThreadArgs *args = (ThreadArgs*) argStruct;
  // ThreadArgs arguments = *args;
  ThreadArgs *args;
  args = (ThreadArgs *) argStruct;

  userNode *users = args->users;  //pointer to head of user linked lsit
  wordPair *words = args->words;  //pointer to head of words linked list
  int new_fd = args->new_fd;
  int threadIdle = args->idleThread;
  printf("New_fd: %d\n", new_fd);
  char *testString = "Testing new thread:";

  callbackUser dispUsers = displayUsers;
  callbackWord dispWords = displayWords;
  // traverseUsers(users, dispUsers);
  // traverseWords(words, dispWords);

  //set thread as being in-use
  threadIdle = 0;
  int quitFlag = 0;
  //wait for string to be received
  int useracc = 0;
  int passacc = 0;
  userNode * user = NULL;
  while (!useracc) { // keep looping until valid user is entered
      char * receivedUsername = ReceiveData(new_fd, STRING_LENGTH);
      //test the received username, for now use simple string comparison
      user = searchUser(users, receivedUsername);
      //if the received and stored usernames match
      if (user != NULL) {
          //send a message prompting for a password
          printf("Password requested.\n");
          useracc = 1;
          SendInt(new_fd, useracc);
          //if the received and stored usernames DON'T match
      } else {
          SendInt(new_fd, useracc);
      }
  }
  while (!passacc) { // keep looping until password is correct
      //receive a password
      char * receivedPassword = ReceiveData(new_fd, STRING_LENGTH);
      //if the received and stored passwords match
      if (strcmp(receivedPassword, user-> password) == 0) {
          printf("Login accepted.\n");
          passacc = 1;
          SendInt(new_fd, passacc);
      } else {
          SendInt(new_fd, 0);
      }
  }
  //receive integer, 1-3 choosing action
  int end = 0;
  int wins;
  char choice;
  char * raw;
  while (!end) {
    raw = ReceiveData(new_fd, STRING_LENGTH);
    choice = raw[0];
    switch (choice) {
      case '1':
      printf("option was 1"); // testing to see if this option was chosen
      time_t t;
      srand((unsigned) time( & t));
      wordPair * word = searchWord(words, rand() % wordCount(words));
        const int sizes[] = {
          strlen(word-> firstWord),
          strlen(word-> secondWord)
        };
          printf("First word: %s, Second word: %s", word->firstWord, word->secondWord);
          wins = PlayHangman(word-> firstWord, sizes[0], word-> secondWord, \
            sizes[1], user, new_fd);
          user-> gamesPlayed++;
          user-> gamesWon += wins;
          break;
      case '2':
          printf("option was 2");
          sendLeaderboard(users, new_fd);
          break;
      case '3':
          printf("option was 3");
          threadIdle = 1;
          end = 1;
          break;
      default:
          printf("Invalid option");
      }
  } //end game while(!end) loop
}//end GameLoop


/*This function contains the game logic for client-server game interaction.*/
int PlayHangman(char * object[], int obj_size, char * obj_type[], int type_size, \
  userNode * user, int new_fd) {
    int size1 = obj_size;
    int size2 = type_size;
    char name[10], word1[size1], word2[size2];
    if (user == NULL) {
        printf("ERROR");
    }
    userNode * acc = user;
    strcpy(name, acc-> username);
    printf("user: %s", name); // testing
    strcpy(word1, object);
    strcpy(word2, obj_type);
    printf("words: %s %s", word1, word2); // testing
    int obj[size1]; // boolean arrays testing whether a letter was guessed
    int type[size2];
    memset(obj, 0, size1 * sizeof(int)); // initialising bool array to all false
    memset(type, 0, size2 * sizeof(int));

    char progress[50] = {
        NULL
    }; // concatenated string of underscores and guessed letters

    //for terminating chars with null terminators
    char str_spc[2], str_letter[2];
    str_spc[1] = '\0'; // space after letter
    str_letter[1] = '\0'; // letter itself
    char spc = ' ';
    str_spc[0] = spc;

    char c;
    char * raw; // raw received letter
    char undr[] = "_ "; // underscore
    char space[] = "  "; // space between underscored words
    int endgame = 0;
    int num_guesses = MIN((size1 + size2 + 10), 26);
    while (!endgame) {
        SendInt(new_fd, num_guesses); // sends number of guesses
        memset(progress, 0, sizeof progress); // clears progress initially
        // concatenates underscores and letters depending on which was guessed correctly
        for (int i = 0; i < size1; i++) {
            if (!obj[i]) {
                strcat(progress, undr);
            } else {
                str_letter[0] = word1[i];
                strcat(progress, str_letter);
                strcat(progress, str_spc);
            }
        }
        strcat(progress, space);
        for (int i = 0; i < size2; i++) {
            if (!type[i]) {
                strcat(progress, undr);
            } else {
                str_letter[0] = word2[i];
                strcat(progress, str_letter);
                strcat(progress, str_spc);
            }
        } // end concatenation
        ReceiveInt(new_fd);
        send(new_fd, progress, 60, 0);
        endgame = 1; // assume game has ended
        for (int i = 0; i < size1; i++) { // checks boolean array and changes endgame status
            if (!obj[i]) {
                endgame = 0;
            }
        }
        for (int i = 0; i < size2; i++) {
            if (!type[i]) {
                endgame = 0;
            }
        }
        if (!num_guesses) {
            endgame = 1;
        }
        ReceiveInt(new_fd);
        SendInt(new_fd, endgame); // sends flag if game has ended or not
        if (num_guesses > 0 && !endgame) { // if game has not ended take letters
            raw = ReceiveData(new_fd, STRING_LENGTH);
            c = raw[0];
        }
        num_guesses -= 1;
        for (int i = 0; i < size1; i++) {
            if (c == word1[i]) {
                // setting boolean array depending if letter was guessed
                obj[i] = 1;
            }
        }
        for (int i = 0; i < size2; i++) {
            if (c == word2[i]) {
                // setting boolean array depending if letter was guessed
                type[i] = 1;
            }
        }
    }
    // shouldn't be here but for testing purposes
    if (num_guesses > 0) {
        printf("Well done %s You won this round of Hangman!\n", name);
        return 1;
    } else {
        printf("Bad luck %s You have run out of guesses. The Hangman got you!\n", name);
        return 0;
    }
}


int main(int argc, char * argv[]) {
    userNode * users = NULL;
    wordPair * words = NULL;
    callbackUser dispUsers = displayUsers;
    callbackWord dispWords = displayWords;

    int portNumber;
    int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd */
    struct sockaddr_in my_addr; /* my address information */
    struct sockaddr_in their_addr; /* connector's address information */
    socklen_t sin_size;
    //thread IDs
    pthread_t tids[BACKLOG];
    int userCounter = 0;
    ThreadArgs * args = (ThreadArgs * )malloc(sizeof(ThreadArgs));


    //If no port number argument is recieved, show message, use default port
    if (argc != ARGUMENTS_EXPECTED) {
        fprintf(stderr, "Defaulting to port 54321.\n");
        portNumber = DEFAULT_PORT_NO;
    } else {
        portNumber = atoi(argv[1]);
    }

    /* generate the socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == RETURNED_ERROR) {
        perror("socket");
        exit(1);
    }

    /* generate the end point */
    my_addr.sin_family = AF_INET; /* host byte order */
    my_addr.sin_port = htons(portNumber); /* short, network byte order */
    my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */

    /* bind the socket to the end point */
    if (bind(sockfd, (struct sockaddr * ) & my_addr, sizeof(struct sockaddr)) \
    == RETURNED_ERROR) {
        perror("bind");
        exit(1);
    }

    /* start listnening */
    if (listen(sockfd, BACKLOG) == RETURNED_ERROR) {
        perror("listen");
        exit(1);
    }

    //this message may not be necessary
    printf("server starts listnening on port %d...\n", portNumber);

    //import word list and authentication details
    words = WordListImport();
    //traverseWords(words, dispWords);
    printf("Words imported!\n");
    users = AuthenticationImport();
    //traverseUsers(users, dispUsers);
    printf("Authentication details imported!\n");

    GenerateThreadArgs(users, words);

    /* repeat: accept, send, close the connection */
    /* for every accepted connection, use a separate process or thread to serve it */
    while (1) { /* main accept() loop */

        //SIGINT handler
        signal(SIGINT, SignalHandler);
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr * ) & their_addr, & sin_size))\
         == RETURNED_ERROR) {
            perror("accept");
            continue;
        }

        printf("Got connection from %s\n", inet_ntoa(their_addr.sin_addr));
        printf("Assigned socket: %d\n", new_fd);

      //if an idle thread is available, create thread

        if(threadArgsArray[0].idleThread == 1){
        threadArgsArray[0].new_fd = new_fd;
        //pass thread ID and attr to GameLoop, along with STRUCTURE of arguments
        pthread_create(&threadArgsArray[0].tid, NULL, GameLoop, &threadArgsArray[0]);

        //upon exit thread, return nothing
        //pthread_join(&threadArgsArray[i].tid, NULL);
        }


} //end accept while loop
pthread_exit(NULL);
return 0;
}//end main
