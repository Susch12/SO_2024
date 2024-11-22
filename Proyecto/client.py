import socket
import tkinter as tk
from tkinter import messagebox
import threading

SERVER_IP = "192.168.100.142"
SERVER_PORT = 8080
MAX_QUESTIONS = 10

class Question:
    def __init__(self, question, options, correct_option):
        self.question = question
        self.options = options
        self.correct_option = correct_option

class Exam:
    def __init__(self):
        self.questions = []
        self.current_question = 0
        self.score = 0

    def add_question(self, question):
        self.questions.append(question)

class ClientApp:
    def __init__(self):
        self.client_socket = None
        self.exam = Exam()

    def connect_to_server(self):
        """Establece conexión con el servidor."""
        try:
            self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.client_socket.settimeout(30)  # Timeout de 30 segundos
            self.client_socket.connect((SERVER_IP, SERVER_PORT))
            print(f"Conexión establecida con el servidor {SERVER_IP}:{SERVER_PORT}")
        except Exception as e:
            print(f"Error conectando al servidor: {e}")
            self.client_socket = None

    def send_data(self, data):
        """Envía datos al servidor."""
        try:
            self.client_socket.sendall(data.encode())
        except Exception as e:
            print(f"Error enviando datos al servidor: {e}")

    def receive_exam(self):
        """Recibe el examen del servidor."""
        try:
            while True:
                buffer = self.client_socket.recv(1024).decode()
                if not buffer or buffer == "END_EXAM":
                    break
                print(f"Pregunta recibida: {buffer}")
                question_text = buffer.strip()
                options = [self.client_socket.recv(1024).decode().strip() for _ in range(3)]
                print(f"Opciones recibidas: {options}")
                correct_option = self.client_socket.recv(1024).decode().strip()
                print(f"Opción correcta recibida: {correct_option}")
                question = Question(question_text, options, correct_option)
                self.exam.add_question(question)
            print(f"Examen recibido con {len(self.exam.questions)} preguntas.")
        except socket.timeout:
            print("Error recibiendo examen: timed out")
        except Exception as e:
            print(f"Error recibiendo examen: {e}")

    def start_exam(self):
        """Inicia el examen."""
        threading.Thread(target=self.connect_and_receive_exam).start()

    def connect_and_receive_exam(self):
        self.connect_to_server()
        self.receive_exam()
        gui.show_question()

    def show_question(self):
        """Muestra la pregunta actual."""
        if self.exam.current_question < len(self.exam.questions):
            question = self.exam.questions[self.exam.current_question]
            gui.show_question(question)
        else:
            print(f"Examen finalizado. Tu puntuación es: {self.exam.score}")

    def submit_answer(self, answer):
        """Procesa la respuesta del usuario."""
        current_question = self.exam.questions[self.exam.current_question]
        if answer == current_question.correct_option:
            print("Respuesta correcta.")
            self.exam.score += 1
        else:
            print("Respuesta incorrecta.")
        self.exam.current_question += 1
        self.show_question()

    def close_connection(self):
        """Cierra la conexión con el servidor."""
        if self.client_socket:
            self.client_socket.close()
            print("Conexión cerrada.")

class GUI:
    def __init__(self, client_app):
        self.client_app = client_app
        self.window = tk.Tk()
        self.window.title("Cliente de Examen")
        self.create_login_window()

    def create_login_window(self):
        """Crea la ventana de inicio de sesión."""
        tk.Label(self.window, text="Matrícula").pack()
        self.matricula_entry = tk.Entry(self.window)
        self.matricula_entry.pack()
        tk.Label(self.window, text="Contraseña").pack()
        self.password_entry = tk.Entry(self.window, show="*")
        self.password_entry.pack()
        login_button = tk.Button(self.window, text="Login", command=self.on_login_clicked)
        login_button.pack()

    def create_exam_menu(self):
        """Crea el menú para seleccionar examen."""
        for widget in self.window.winfo_children():
            widget.destroy()
        tk.Label(self.window, text="Selecciona un examen").pack()
        math_button = tk.Button(self.window, text="Matemáticas", command=lambda: self.select_exam("Matemáticas"))
        math_button.pack()
        spanish_button = tk.Button(self.window, text="Español", command=lambda: self.select_exam("Español"))
        spanish_button.pack()
        english_button = tk.Button(self.window, text="Inglés", command=lambda: self.select_exam("Inglés"))
        english_button.pack()

    def on_login_clicked(self):
        """Callback para el botón de login."""
        matricula = self.matricula_entry.get()
        password = self.password_entry.get()
        credentials = f"{matricula},{password}"
        self.client_app.send_data(credentials)
        # Leer respuesta del servidor
        try:
            response = self.client_app.client_socket.recv(1024).decode()
            if "Autenticación exitosa" in response:
                messagebox.showinfo("Login", "Autenticación exitosa")
                self.create_exam_menu()
            else:
                messagebox.showerror("Login", "Autenticación fallida")
        except Exception as e:
            print(f"Error durante el login: {e}")

    def select_exam(self, exam_name):
        """Envía la selección del examen y recibe preguntas."""
        self.client_app.send_data(exam_name)
        self.client_app.receive_exam()
        self.client_app.start_exam()

    def show_question(self, question=None):
        """Muestra la pregunta y opciones en la interfaz gráfica."""
        if question is None:
            question = self.client_app.exam.questions[self.client_app.exam.current_question]
        for widget in self.window.winfo_children():
            widget.destroy()
        tk.Label(self.window, text=question.question).pack()
        for i, option in enumerate(question.options):
            tk.Button(self.window, text=option, command=lambda opt=option: self.submit_answer(opt)).pack()

    def submit_answer(self, answer):
        """Procesa la respuesta del usuario."""
        self.client_app.submit_answer(answer)
        self.window.after(100, self.show_question)

    def run(self):
        """Ejecuta la interfaz gráfica."""
        self.window.mainloop()

# Código principal
if __name__ == "__main__":
    client = ClientApp()
    client.connect_to_server()
    gui = GUI(client)
    try:
        gui.run()
    finally:
        client.close_connection()
