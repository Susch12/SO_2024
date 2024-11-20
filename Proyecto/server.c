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
#define MAX_EXAMENES 3

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

typedef struct {
    char matricula[MAX_LONGITUD];
    int calificaciones[MAX_EXAMENES];
} EstadoUsuario;

EstadoUsuario estados[MAX_USUARIOS];
int estados_count = 0;

Pregunta preguntas[MAX_PREGUNTAS];
int total_preguntas = 0;

typedef struct {
    char matricula[MAX_LONGITUD];
    char password[MAX_LONGITUD];
} User;

User users[MAX_USUARIOS];
int user_count = 0;
int pregunta_timeout = 0;

typedef struct {
    int id;             // Identificador del examen
    char nombre[50];    // Nombre del examen
    int preguntas;      // Número de preguntas
} Examen;

Examen examenes[MAX_EXAMENES];

// Manejador de señal para el timeout de la pregunta
void manejar_timeout(int signo) {
    pregunta_timeout = 1;
}

// Inicializa los estados de los exámenes para un usuario nuevo
void inicializar_estado_usuario(const char *matricula) {
    for (int i = 0; i < estados_count; i++) {
        if (strcmp(estados[i].matricula, matricula) == 0) {
            return; // El usuario ya tiene un estado registrado
        }
    }

    strcpy(estados[estados_count].matricula, matricula);
    for (int i = 0; i < MAX_EXAMENES; i++) {
        estados[estados_count].calificaciones[i] = -1;
    }
    estados_count++;
}

//Obtiene el indice de estado de un usuario

int obtener_indice_usuario(const char *matricula) {
    for (int i = 0; i < estados_count; i++) {
        if (strcmp(estados[i].matricula, matricula) == 0) {
            return i;
        }
    }
    return -1; // Usuario no encontrado
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

void inicializar_examenes() {
    // Cargar exámenes de diferentes categorías
    strcpy(examenes[0].nombre, "Matemáticas");
    examenes[0].preguntas = cargar_preguntas(examenes[0].preguntas, "mate.txt");

    strcpy(examenes[1].nombre, "Español");
    examenes[1].preguntas = cargar_preguntas(examenes[1].preguntas, "español.txt");

    strcpy(examenes[2].nombre, "Inglés");
    examenes[2].preguntas = cargar_preguntas(examenes[2].preguntas, "ingles.txt");
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
// Función para imprimir menú
void imprimir_menu(int nuevo_socket, int indice_usuario){
    char buffer[512];
    sprintf(buffer, "[+] Seleccione el examen:\n");
    for (int i = 0; i < MAX_EXAMENES; i++) {
        if (estados[indice_usuario].calificaciones[i] == -1) {
            sprintf(buffer + strlen(buffer), "%d. Examen %d\n", i + 1, i + 1);
        } else {
            sprintf(buffer + strlen(buffer), "%d. Examen %d (Completado, Calificación: %d/10)\n",
                    i + 1, i + 1, estados[indice_usuario].calificaciones[i]);
        }
    }
    send(nuevo_socket, buffer, strlen(buffer), 0);

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

// Función que ejecuta el examen

int ejecutar_examen(int nuevo_socket, Examen examen) {
    char categoria[MAX_LONGITUD];
    char buffer[MAX_LONGITUD];
    char archivo[50];
    Pregunta preguntas[10];
    int total_preguntas, puntaje = 0;
    static int num_examen = 0; // Para contar el número de exámenes por conexión
    int valread;

    // Leer la categoría enviada por el cliente
    valread = read(nuevo_socket, categoria, MAX_LONGITUD);
    if (valread <= 0) {
        close(nuevo_socket);
        perror("[!] No existe una opción para ese valor...");
        return -1; // Indica error en la conexión
    }
    categoria[valread - 1] = '\0'; // Elimina salto de línea
    categoria[strcspn(categoria, "\r\n")] = 0; // Asegura limpieza de entrada

    // Seleccionar archivo basado en la categoría
    if (strcmp(categoria, "1") == 0) {
        strcpy(archivo, "mate.txt");
    } else if (strcmp(categoria, "2") == 0) {
        strcpy(archivo, "español.txt");
    } else if (strcmp(categoria, "3") == 0) {
        strcpy(archivo, "ingles.txt");
    } else {
        send(nuevo_socket, "[!] Examen no válido. Cerrando conexión...\n", 38, 0);
        close(nuevo_socket);
        return -1; // Indica examen inválido
    }

    // Cargar preguntas desde el archivo
    total_preguntas = cargar_preguntas(preguntas, archivo);
    if (total_preguntas == 0) {
        send(nuevo_socket, "[!] No se pudieron cargar preguntas para este examen.\n", 49, 0);
        close(nuevo_socket);
        return -1; // Indica error al cargar preguntas
    }

    // Mezclar las preguntas
    mezclar_preguntas(preguntas, total_preguntas);

    // Configurar el manejador de señales para el temporizador
    signal(SIGALRM, manejar_timeout);

    // Realizar el examen
    for (int i = 0; i < 10; i++) {
        int pregunta_timeout = 0;

        // Mostrar la pregunta al cliente
        snprintf(buffer, sizeof(buffer), "[+] Pregunta %d: %s\n A) %s\n B) %s\n C) %s\n\n",
                 i + 1, preguntas[i].pregunta, preguntas[i].opcionA, preguntas[i].opcionB, preguntas[i].opcionC);
        send(nuevo_socket, buffer, strlen(buffer), 0);

        // Iniciar temporizador
        alarm(30);
        valread = read(nuevo_socket, buffer, sizeof(buffer));
        alarm(0); // Cancelar temporizador si hay respuesta

        // Evaluar respuesta
        if (valread > 0 && !pregunta_timeout) {
            buffer[valread - 1] = '\0'; // Eliminar salto de línea
            if (toupper(buffer[0]) == preguntas[i].respuesta_correcta) {
                puntaje++;
            }
        } else {
            send(nuevo_socket, "[!] Tiempo de respuesta agotado para esta pregunta.\n", 48, 0);
        }

        memset(buffer, 0, sizeof(buffer)); // Limpiar buffer
    }

    // Mostrar resultado final al cliente
    snprintf(buffer, sizeof(buffer), "[+] Examen terminado. Puntaje final: %d/10\n", puntaje);
    send(nuevo_socket, buffer, strlen(buffer), 0);

    // Incrementar el número de exámenes realizados
    num_examen++;
    if (num_examen == 3) {
        close(nuevo_socket); // Cerrar conexión después de 3 exámenes
    }

    return puntaje; // Devuelve el puntaje final
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
    inicializar_estado_usuario(usuario);
    int indice_usuario = obtener_indice_usuario(usuario);

    while (1) {
      imprimir_menu(nuevo_socket, indice_usuario);
      valread = read(nuevo_socket, buffer, sizeof(buffer));
      if (valread <= 0) {
        close(nuevo_socket);
        return;
      }
      int examen_idx = atoi(buffer)-1;
      if (examen_idx < 0 || examen_idx >= MAX_EXAMENES) {
        send(nuevo_socket, "[!] Opción inválida.\n", 20, 0);
        continue;
      }
      if (estados[indice_usuario].calificaciones[examen_idx] != -1) {
        sprintf(buffer, "[!] Ya completaste este examen. Calificación: %d/10\n",
        estados[indice_usuario].calificaciones[examen]);
        send(nuevo_socket, buffer, strlen(buffer), 0);
        continue;
      }
      // Iniciar examen 
      sprintf(buffer, "[+] Iniciando examen %d...\n", examen_idx + 1);
      send(nuevo_socket, buffer, strlen(buffer), 0);
      Examen examen_seleccionado = examenes[examen_idx];
      puntaje = ejecutar_examen(nuevo_socket, examen_seleccionado); // Implementa esta función según tu lógica actual
        estados[indice_usuario].calificaciones[examen_idx] = puntaje;

        sprintf(buffer, "[+] Examen terminado. Calificación: %d/10\n", puntaje);
        send(nuevo_socket, buffer, strlen(buffer), 0);
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

