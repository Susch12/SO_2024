#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define MAX_USERS 20
#define MAX_EXAMS 3
#define MAX_QUESTIONS 10
#define TOTAL_QUESTIONS 30
#define PORT 8080

typedef struct {
    char matricula[10];
    char password[10];
} User;

typedef struct {
    char question[256];
    char options[3][256];
    int correct_option;
} Question;

typedef struct {
    Question questions[MAX_QUESTIONS];
    int question_count;
} Exam;

typedef struct {
    char matricula[10];
    int scores[MAX_EXAMS];
} Result;

User users[MAX_USERS];
Exam exams[MAX_EXAMS];
Result results[3];
int result_count = 0;
Question question_bank[TOTAL_QUESTIONS];

void load_users();
int authenticate(char *matricula, char *password);
void load_exam(const char *filename, Exam *exam);
void load_exams();
void generate_random_exam(Exam *exam);
void add_result(const char *matricula, int scores[]);
void show_results();
void handle_client(int client_socket);

void load_users() {
    FILE *file = fopen("usuarios.txt", "r");
    if (file == NULL) {
        perror("[!] Error al abrir el archivo de usuarios");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int i = 0;
    while (fgets(line, sizeof(line), file) && i < MAX_USERS) {
        char *token = strtok(line, ",");
        if (token != NULL) {
            strncpy(users[i].matricula, token, sizeof(users[i].matricula) - 1);
            token = strtok(NULL, "\n");
            if (token != NULL) {
                strncpy(users[i].password, token, sizeof(users[i].password) - 1);
            }
        }
        i++;
    }

    fclose(file);
}

int authenticate(char *matricula, char *password) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (strcmp(users[i].matricula, matricula) == 0 && strcmp(users[i].password, password) == 0) {
            return 1;
        }
    }
    return 0;
}

void load_exam(const char *filename, Exam *exam) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("[!] Error al abrir el archivo de examen");
        exit(EXIT_FAILURE);
    }

    char line[512];
    int i = 0;
    while (fgets(line, sizeof(line), file) && i < MAX_QUESTIONS) {
        char *token = strtok(line, "|");
        if (token != NULL) {
            strncpy(exam->questions[i].question, token, sizeof(exam->questions[i].question) - 1);
            token = strtok(NULL, "|");
            strncpy(exam->questions[i].question, token, sizeof(exam->questions[i].question) - 1);
            for (int j = 0; j < 3; j++) {
                token = strtok(NULL, "|");
                strncpy(exam->questions[i].options[j], token, sizeof(exam->questions[i].options[j]) - 1);
            }
            token = strtok(NULL, "\n");
            if (token != NULL) {
                exam->questions[i].correct_option = token[0];
            }
        }
        i++;
    }
    exam->question_count = i;

    fclose(file);
}

void load_exams() {
    load_exam("mate.txt", &exams[0]);
    load_exam("español.txt", &exams[1]);
    load_exam("ingles.txt", &exams[2]);
}

void generate_random_exam(Exam *exam) {
    int indices[TOTAL_QUESTIONS];
    for (int i = 0; i < TOTAL_QUESTIONS; i++) {
        indices[i] = i;
    }

    // Mezclar los índices
    for (int i = 0; i < TOTAL_QUESTIONS; i++) {
        int j = rand() % TOTAL_QUESTIONS;
        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }

    // Seleccionar las primeras 10 preguntas aleatorias
    for (int i = 0; i < MAX_QUESTIONS; i++) {
        exam->questions[i] = question_bank[indices[i]];
    }
    exam->question_count = MAX_QUESTIONS;
}

void add_result(const char *matricula, int scores[]) {
    if (result_count < 3) {
        strncpy(results[result_count].matricula, matricula, sizeof(results[result_count].matricula) - 1);
        for (int i = 0; i < MAX_EXAMS; i++) {
            results[result_count].scores[i] = scores[i];
        }
        result_count++;
    }

    if (result_count == 3) {
        show_results();
    }
}

void show_results() {
    printf("[!] Resultados de los 3 usuarios:\n");
    for (int i = 0; i < 3; i++) {
        printf("[+] Usuario %s:\n", results[i].matricula);
        for (int j = 0; j < MAX_EXAMS; j++) {
            printf("[+] Examen %d: %d\n", j + 1, results[i].scores[j]);
        }
    }
}

void handle_client(int client_socket) {
    char buffer[1024];
    int n;

    // Recibir matrícula y contraseña
    n = read(client_socket, buffer, sizeof(buffer) - 1);
    if (n < 0) {
        perror("[!] Error leyendo del socket");
        close(client_socket);
        return;
    }
    buffer[n] = '\0';

    char *matricula = strtok(buffer, ",");
    char *password = strtok(NULL, ",");

    if (authenticate(matricula, password)) {
        write(client_socket, "[+] Autenticación exitosa\n", 22);
        // Aquí puedes agregar la lógica para manejar el menú de exámenes y la realización de exámenes
    } else {
        write(client_socket, "[!] Autenticación fallida\n", 22);
    }

    close(client_socket);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    srand(time(NULL)); // Inicializar la semilla para la generación aleatoria
    load_users();
    load_exams();

    // Crear el socket del servidor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("[!] Error abriendo el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Enlazar el socket a la dirección del servidor
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[!] Error en el bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones entrantes
    listen(server_socket, 5);
    printf("[+] Servidor escuchando en el puerto %d\n", PORT);

    while (1) {
        client_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("[!] Error en el accept");
            continue;
        }

        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}
