#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "192.168.100.142"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void receive_data(int socket, char *buffer, size_t size) {
    int n = read(socket, buffer, size - 1);
    if (n < 0) {
        error("Error leyendo del socket");
    }
    buffer[n] = '\0';
}

void send_data(int socket, const char *data) {
    if (write(socket, data, strlen(data)) < 0) {
        error("Error escribiendo en el socket");
    }
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Crear el socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        error("Error abriendo el socket");
    }

    // Configurar la dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    // Conectar al servidor
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Error conectando al servidor");
    }
    printf("Conexión establecida con el servidor %s:%d\n", SERVER_IP, SERVER_PORT);

    // Enviar credenciales
    char matricula[10], password[10];
    printf("Ingrese su matrícula: ");
    scanf("%s", matricula);
    printf("Ingrese su contraseña: ");
    scanf("%s", password);
    snprintf(buffer, sizeof(buffer), "%s,%s", matricula, password);
    send_data(client_socket, buffer);

    // Recibir respuesta de autenticación
    receive_data(client_socket, buffer, sizeof(buffer));
    printf("Respuesta del servidor: %s\n", buffer);
    if (strstr(buffer, "Autenticación exitosa") == NULL) {
        printf("Autenticación fallida\n");
        close(client_socket);
        return 0;
    }

    // Solicitar examen
    char exam_name[20];
    printf("Ingrese el nombre del examen (Matemáticas, Español, Inglés): ");
    scanf("%s", exam_name);
    send_data(client_socket, exam_name);

    // Recibir y mostrar preguntas y opciones
    for (int i = 0; i < 10; i++) {
        receive_data(client_socket, buffer, sizeof(buffer));
        printf("Pregunta %d: %s\n", i + 1, buffer);
        for (int j = 0; j < 3; j++) {
            receive_data(client_socket, buffer, sizeof(buffer));
            printf("Opción %c: %s\n", 'A' + j, buffer);
        }
        printf("Ingrese su respuesta (1, 2, 3): ");
        int answer;
        scanf("%d", &answer);
        snprintf(buffer, sizeof(buffer), "%d", answer);
        send_data(client_socket, buffer);
    }

    // Cerrar la conexión
    close(client_socket);
    printf("Conexión cerrada.\n");

    return 0;
}
