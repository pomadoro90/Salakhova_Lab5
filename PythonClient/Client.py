import threading
import time
from Message import *


def ProcessMessages():
    while True:
        m = Message()
        # Поток бесконечно ждет новых данных от сервера
        m.Receive(Message.Connection)

        if m.Header.Type == MT_DATA:
            print(f"\n[Клиент #{m.Header.From}]: {m.Data}")
            print("> ", end="", flush=True)

        elif m.Header.Type == MT_CONFIRM:
            # Сервер присылает Broadcast со списком при любом изменении.
            # Мы можем использовать первое такое сообщение, чтобы узнать свой ID,
            # так как сервер отправляет его в поле From или To.
            if Message.ClientID == 0:
                print("\n[Система] Успешное подключение к серверу сообщений.")
                print("> ", end="", flush=True)
                # Просто ставим флаг, что мы подключены
                Message.ClientID = -1

        elif m.Header.Type == MT_CLOSE:
            print("\n[Система] Соединение с сервером разорвано.")
            break


def Client():
    try:
        Message.Connect('127.0.0.1', 12345)
    except Exception as e:
        print(f"Ошибка подключения: {e}")
        return

    # Отправляем пустое MT_INFO (пинг), чтобы сервер нас зарегистрировал
    # и прислал в ответ MT_CONFIRM
    Message.SendMessage(MR_BROKER, MT_INFO, "")

    # Запускаем чтение входящих сообщений в отдельном фоновом потоке
    t = threading.Thread(target=ProcessMessages, daemon=True)
    t.start()

    print("Введи сообщение (или /quit для выхода):")

    while True:
        try:
            text = input("> ")
        except (KeyboardInterrupt, EOFError):
            break

        if text == "/quit":
            Message.SendMessage(MR_BROKER, MT_QUIT)
            time.sleep(0.5)  # Даем время на отправку перед закрытием
            break

        if text.strip() != "":
            # По умолчанию шлем широковещательное сообщение всем (MR_ALL)
            Message.SendMessage(MR_ALL, MT_DATA, text)


if __name__ == '__main__':
    Client()