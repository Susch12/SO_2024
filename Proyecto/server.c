#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define PORT 8080
#define MAX_PREGUNTAS 100
#define MAX_LONGITUD 512
#define MAX_CLIENTES 10
#define MAX_USUARIOS 20 // Definir un límite máximo de usuarios

typedef struct {
    char categoria[MAX_LONGITUD];
    char numero[MAX_LONGITUD];
    char pregunta[MAX_LONGITUD];
    char opcionA[MAX_LONGITUD];
    char opcionB[MAX_LONGITUD];
    char opcionC[MAX_LONGITUD];
    char respuesta_correcta;
} Pregunta;

typedef struct {
    char matricula[MAX_LONGITUD];
    int puntaje;
} Resultado;

Pregunta preguntas[MAX_PREGUNTAS];
int total_preguntas = 0;

typedef struct {
    char matricula[MAX_LONGITUD];
    char password[MAX_LONGITUD];
} User;

User users[MAX_USUARIOS];
int user_count = 0;
int pregunta_timeout = 0;

// Manejador de señal para el timeout de la pregunta
void manejar_timeout(int signo) {
    pregunta_timeout = 1;
}

// Función para cargar preguntas desde un archivo
int cargar_preguntas(Pregunta preguntas[], const char *archivo) {
    FILE *file = fopen(archivo, "r");
    if (!file) {
        perror("Error al abrir el archivo de preguntas");
        return 0;
    }

    int total_preguntas = 0;
    char linea[MAX_LONGITUD];
    while (fgets(linea, sizeof(linea), file) && total_preguntas < MAX_PREGUNTAS) {
        linea[strcspn(linea, "\n")] = 0; // Elimina el salto de línea

        Pregunta p;
        char *token = strtok(linea, "|");

        if (token != NULL) strncpy(p.categoria, token, MAX_LONGITUD);
        token = strtok(NULL, "|");
        if (token != NULL) strncpy(p.pregunta, token, MAX_LONGITUD);
        token = strtok(NULL, "|");
        if (token != NULL) strncpy(p.opcionA, token, MAX_LONGITUD);
        token = strtok(NULL, "|");
        if (token != NULL) strncpy(p.opcionB, token, MAX_LONGITUD);
        token = strtok(NULL, "|");
        if (token != NULL) strncpy(p.opcionC, token, MAX_LONGITUD);
        token = strtok(NULL, "|");
        if (token != NULL) p.respuesta_correcta = toupper(token[0]);

        preguntas[total_preguntas++] = p;
    }

    fclose(file);
    return total_preguntas;
}

// Función para cargar usuarios
void load_users(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error abriendo el archivo de usuarios");
        exit(1);
    }

    while (fscanf(file, "%[^,],%s\n", users[user_count].matricula, users[user_count].password) == 2) {
        user_count++;
        if (user_count >= MAX_USUARIOS) break;
    }
    fclose(file);
}

// Función para autenticar usuario
int authenticate(char *matricula, char *password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].matricula, matricula) == 0 && strcmp(users[i].password, password) == 0) {
            return 1;
        }
    }
    return 0;
}
void imprimir_menu(int nuevo_socket){
  int examen;
  send(nuevo_socket, "[+] Seleccione el examen:\n1. Matemáticas\n2. Español\n3. Inglés\n", 64, 0);

}
// Función para mezclar preguntas
void mezclar_preguntas(Pregunta preguntas[], int n) {
    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        int j = rand() % n;
        Pregunta temp = preguntas[i];
        preguntas[i] = preguntas[j];
        preguntas[j] = temp;
    }
}

// Función para manejar las conexiones de los clientes
void manejar_cliente(int nuevo_socket) {
    char buffer[1024] = {0};
    int valread;
    char usuario[MAX_LONGITUD];
    char contrasena[MAX_LONGITUD];
    char categoria[MAX_LONGITUD];
    int puntaje = 0;
    int num_examen = 0;

    send(nuevo_socket, "[!] Por favor, ingrese su matrícula: ", 32, 0);
    valread = read(nuevo_socket, usuario, MAX_LONGITUD);
    if (valread <= 0) {
        close(nuevo_socket);
        return;
    }
    usuario[valread] = '\0';
    usuario[strcspn(usuario, "\r\n")] = 0;

    send(nuevo_socket, "[!] Por favor, ingrese su contraseña: ", 34, 0);
    valread = read(nuevo_socket, contrasena, MAX_LONGITUD);
    if (valread <= 0) {
        close(nuevo_socket);
        return;
    }
    contrasena[valread] = '\0';
    contrasena[strcspn(contrasena, "\r\n")] = 0;
    if (!authenticate(usuario, contrasena)) {
        send(nuevo_socket, "[!] Autenticación fallida. Cerrando conexión...\n", 44, 0);
        close(nuevo_socket);
        return;
    }
    
    valread = read(nuevo_socket, categoria, MAX_LONGITUD);
    if (valread <= 0) {
        close(nuevo_socket);
        perror("[!] No existe una opción para ese valor...");
        return;
    }
    categoria[valread - 1] = '\0';
    categoria[strcspn(categoria, "\r\n")] = 0;

    char archivo[50];
    if (strcmp(categoria, "1") == 0) {
        strcpy(archivo, "mate.txt");
    } else if (strcmp(categoria, "2") == 0) {
        strcpy(archivo, "español.txt");
    } else if (strcmp(categoria, "3") == 0) {
        strcpy(archivo, "ingles.txt");
    } else {
        send(nuevo_socket, "[!] Examen no válido. Cerrando conexión...\n", 38, 0);
        close(nuevo_socket);
        return;
    }

    total_preguntas = cargar_preguntas(preguntas, archivo);
    if (total_preguntas == 0) {
        send(nuevo_socket, "[!] No se pudieron cargar preguntas para este examen.\n", 49, 0);
        close(nuevo_socket);
        return;
    }

    mezclar_preguntas(preguntas, total_preguntas);

    signal(SIGALRM, manejar_timeout);

    for (int i = 0; i < 10; i++) {
        pregunta_timeout = 0;
        sprintf(buffer, "[+] Pregunta: %s\n %s\n %s\n %s\n\n",
                preguntas[i].pregunta,
                preguntas[i].opcionA,
                preguntas[i].opcionB,
                preguntas[i].opcionC);
        send(nuevo_socket, buffer, strlen(buffer), 0);

        alarm(30);
        valread = read(nuevo_socket, buffer, 1024);

        if (!pregunta_timeout && valread > 0) {
            alarm(0);
            if (toupper(buffer[0]) == preguntas[i].respuesta_correcta) {
                puntaje++;
            }
        } else {
            send(nuevo_socket, "[!] Tiempo de respuesta agotado para esta pregunta.\n", 48, 0);
        }
        memset(buffer, 0, sizeof(buffer));
    }

    sprintf(buffer, "[+] Examen terminado. Puntaje final: %d/10\n", puntaje);
    send(nuevo_socket, buffer, strlen(buffer), 0);
    num_examen++;
    if (num_examen == 3){
      close(nuevo_socket);
  }
}

int main() {
    int server_fd, nuevo_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    load_users("usuarios.txt");

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("[!] Error al crear el socket");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[!] Error en el bind");
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTES) < 0) {
        perror("[!] Error al escuchar conexiones");
        return 1;
    }

    printf("[+] Esperando conexiones...\n");

    while ((nuevo_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) >= 0) {
        printf("[+] Nuevo cliente conectado\n");
        if (fork() == 0) {
            close(server_fd);
            manejar_cliente(nuevo_socket);
            exit(0);
        }
        close(nuevo_socket);
    }

    return 0;
}

