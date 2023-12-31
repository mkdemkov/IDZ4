# ИДЗ-4. Демков Михаил Кириллович БПИ212
Учел ваши предыдущие замечания
## Вариант 22
## Условие задачи
Задача об инвентаризации по рядам. После нового года в библиотеке университета обнаружилась пропажа каталога. После поиска и наказания виноватых, ректор дал указание восстановить каталог силами студентов. Фонд библиотека представляет собой прямоугольное помещение, в котором находится M рядов по N шкафов по K книг в каждом шкафу. Требуется создать приложение, составляющее каталог. В качестве отдельного процесса задается составление подкаталога одним студентом–процессом для одного ряда. После этого студент передает информацию процессу–библиотекарю через tcp сокет, который сортирует ее учитывая подкаталоги, переданные другими студентами.

## Схема решения
* Процесс библиотекаря запускается первым, создает все необходимые ресурсы для дальнейшей работы (сокеты, семафоры, массив книг).
* Процесс библиотекаря запускает отдельный поток для отдачи логов.
* Процесс логгера запускается вторым, подключается к сокету библиотекаря и выводит в консоль все полученные логи по мере выполнения программы.
* Процесс студента запускается последним, подключается к сокету библиотекаря и отправляет ему информацию о книгах в своем ряду.
* Каждый процесс/поток удаляет свои ресурсы.

## Запуск
Программа библиотекаря принимает на вход 5 аргументов: количество рядов, количество шкафов, количество книг в шкафу, порт для студентов и порт для логгера:

```markdown
./lib.out 2 2 4 3490 3491
```

Программа логгер принимает на вход 2 аргумента: адрес библиотекаря (сервера) и порт:

```markdown
./log.out localhost 3491
```

Программа студент принимает на вход 2 аргумента: адрес библиотекаря (сервера), количество рядов, количество шкафов, количество книг в шкафу, порт для студентов:

```markdown
./stu.out localhost 2 2 4 3490
```

Также значение NMK должно быть меньше 100, так как в программе используется фисксированный размер пакета в 100 байт.

## Пример работы программы

### Пример работы библиотекаря

![Скрин lib.jpg](https://github.com/mkdemkov/IDZ4/blob/main/lib.JPG)


### Пример работы логгера

![Скрин log.jpg](https://github.com/mkdemkov/IDZ4/blob/main/log.JPG)

### Пример работы студента

![Скрин stu.jpg](https://github.com/mkdemkov/IDZ4/blob/main/stu.JPG)

## Логика работы
Вся логика работы программы не меняется в решениях на разные оценки. Различия в реализации только в том, какие средства синхронизации используются.

## Работа на 4-5
В коде на 4-5 сервер слушает один порт, к которому подключаются клиенты.
## Работа на 6-7
В коде на 6-7 сервер слушает второй порт для логгера в отдельном потоке. Связть мужду потоками организована через именованный канал и семафоры Posix.