import socket
import threading
import time
import pwn 

def connect_to_server(server_ip, server_port):
    # Crear un objeto socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    # Conectar al servidor
    client_socket.connect((server_ip, server_port))
    
    return client_socket

def authenticate(client_socket, matricula, password):
    # Enviar matrícula y contraseña al servidor
    credentials = f"{matricula},{password}"
    client_socket.sendall(credentials.encode())
    
    # Recibir respuesta de autenticación del servidor
    response = client_socket.recv(1024).decode()
    print(response)
    
    return "exitosa" in response

def request_exam(client_socket, exam_name):
    # Enviar solicitud de examen al servidor
    client_socket.sendall(exam_name.encode())
    
    # Recibir y mostrar preguntas y opciones del examen
    questions = []
    for _ in range(10):  # Asumiendo que cada examen tiene 10 preguntas
        question = client_socket.recv(1024).decode()
        print(f"Pregunta: {question}")
        
        options = []
        for _ in range(3):  # Asumiendo que cada pregunta tiene 3 opciones
            option = client_socket.recv(1024).decode()
            print(f"Opción: {option}")
            options.append(option)
        
        correct_option = int(client_socket.recv(1024).decode())
        print(f"Opción Correcta: {correct_option}")
        
        questions.append((question, options, correct_option))
    
    return questions

def send_answers(client_socket, answers):
    for answer in answers:
        client_socket.sendall(str(answer).encode())
        response = client_socket.recv(1024).decode()
        print(response)

def take_exam(client_socket, exam_name):
    questions = request_exam(client_socket, exam_name)
    
    answers = []
    for i, (question, options, correct_option) in enumerate(questions):
        start_time = time.time()
        answer = input(f"Tu respuesta para la pregunta {i+1} (tienes 10 minutos): ")
        
        # Verificar si el tiempo de respuesta excede los 10 minutos
        if time.time() - start_time > 600:
            print("Tiempo excedido para esta pregunta. Se tomará como incorrecta.")
            answer = -1
        
        answers.append(answer)
    
    send_answers(client_socket, answers)

def main():
    server_ip = '192.168.100.142'  # Reemplazar con la IP del servidor
    server_port = 8080
    
    matricula = input("Introduce tu matrícula: ")
    password = input("Introduce tu contraseña: ")
    
    client_socket = connect_to_server(server_ip, server_port)
    
    if authenticate(client_socket, matricula, password):
       print("Hola") 
    client_socket.close()

if __name__ == "__main__":
    main()
