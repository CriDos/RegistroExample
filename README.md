# RegistroExample

<p align="center">
  <img src="https://img.shields.io/badge/Windows-x64_%28MSVC%29-0078d6?logo=windows&logoColor=white" alt="Windows x64 (MSVC)" />
  <img src="https://img.shields.io/badge/Qt-6.8.3-41cd52?logo=qt&logoColor=white" alt="Qt 6.8.3" />
  <img src="https://img.shields.io/badge/C%2B%2B-MSVC_2022-00599c?logo=cplusplus&logoColor=white" alt="C++ / MSVC 2022" />
  <img src="https://img.shields.io/badge/CMake-%E2%89%A5%203.25-064F8C?logo=cmake&logoColor=white" alt="CMake ≥ 3.25" />
  <img src="https://img.shields.io/badge/License-MIT-yellow" alt="MIT" />
</p>

<!-- GitHub badges (activate after the first push; replace <owner>/<repo>):
  <img src="https://img.shields.io/github/actions/workflow/status/<owner>/<repo>/ci.yml?label=CI&logo=github" alt="CI" />
  <img src="https://img.shields.io/github/v/release/<owner>/<repo>?label=Release" alt="Release" />
-->

Пример клиент-серверного справочника клиентов на Qt 6, в котором QML-клиент обменивается с REST-сервером (QHttpServer) по общему JSON-контракту, сервер хранит данные в SQLite, зависимости между слоями строго направлены, а из внешних библиотек используется только сам Qt.

```
┌──────────────┐   REST / JSON   ┌──────────────┐   SQL   ┌────────────┐
│  QML-клиент  │ ──────────────► │ REST-сервер  │ ──────► │   SQLite   │
│  Qt Quick    │                 │ QHttpServer  │         │   (WAL)    │
└──────────────┘                 └──────────────┘         └────────────┘
```

## Скриншоты

| Форма входа | Справочник клиентов | Карточка клиента |
|:---:|:---:|:---:|
| ![Форма входа](assets/screenshots/login.png) | ![Справочник клиентов](assets/screenshots/main.png) | ![Карточка клиента](assets/screenshots/card.png) |

## Возможности

- **Демо-режим по умолчанию** — запуск без флагов создаёт `registro-demo.db` с учётной записью
  `demo`/`demo` и 10 000 случайных клиентов; клиент заполняет форму входа учётными данными
  `demo`/`demo` из своего конфигурационного файла (ключ `demo` в `config.ini`, по умолчанию
  `true` — сервер при этом не опрашивается). `--no-demo` запускает сервер без демо-режима
  (`registro.db`, без сида).
- **Каталог клиентов** — CRUD, поиск, фильтр по статусу, пагинация по 50 записей;
  лимиты полей общие для сервера и клиента.
- **Заметки в карточке** — часть карточки клиента, сохраняется одним запросом вместе с ней.
- **Журнал аудита** — история изменений, доступна администратору.
- **Мягкое удаление** — запись помечается `deleted` и пропадает из API, но остаётся в базе.
- **Логирование** — компактный однострочный формат, `--log-level/--log-file`.

## Архитектура

Три модуля со строгим направлением зависимостей: `common` ничего не знает о клиенте и сервере,
весь SQL остаётся внутри `Database`, QML общается с C++ через синглтоны модуля.

| Модуль | Содержимое |
|---|---|
| `src/common` | DTO и JSON-маппинг, лимиты полей, инфраструктура логирования |
| `src/server` | `Database` (SQL, миграции), `ApiServer` (маршруты, авторизация, аудит), конфигурация; приложение `RegistroServer` |
| `src/client` | `ApiClient`, модель списка клиентов и модель настроек; QML-приложение `RegistroClient` (синглтоны `Api`, `ClientModel`, `Config`, `Limits`, `Theme`) |

## API

Базовый URL: `http://<host>:9080`. Ответы — JSON; ошибки — `{"error": "message"}` с подходящим HTTP-кодом. `auth` — требуется заголовок `Authorization: Bearer <token>`; `admin` — только для роли `admin`. Пока пользователей нет, аутентификация отключена — первый созданный через `POST /api/users` принудительно получает роль `admin`. Альтернатива: `--admin-user`/`--admin-password` при первом запуске без демо-режима (или ключи `admin_user`/`admin_password` в `registro-server.ini`); в демо-режиме админ `demo`/`demo` создаётся автоматически.

| Метод и путь | Доступ | Описание |
|---|---|---|
| `GET /api/health` | публичный | `{"status": "ok"}`; в демо-режиме добавляется `"demo": true` |
| `POST /api/auth/login` | публичный | `{username, password}` → `{token, user}`; неудачные попытки ограничиваются (10 попыток/мин → 429) |
| `GET /api/auth/me` | auth | текущий пользователь `{id, username, role}` |
| `GET /api/users` | admin | список пользователей `{items: [...]}` |
| `POST /api/users` | admin | создание пользователя `{username, password (≥8), role: user\|admin}` → 201 |
| `GET /api/clients?search=&status=&limit=&offset=` | auth | список `{total, items}` (поиск не зависит от регистра, включая кириллицу; телефон ищется по цифрам; пагинация по 50) |
| `POST /api/clients` | auth | создание клиента: `full_name` — обязательное поле, остальные (`org`, `phone`, `email`, `notes`, `status`) необязательны → 201 |
| `GET /api/client?id=N` | auth | один клиент; 404, если запись не найдена |
| `PUT /api/client?id=N` | auth | обновление (тело как у POST) |
| `DELETE /api/client?id=N` | admin | мягкое удаление (запись больше не попадает в список) |
| `GET /api/audit?limit=&offset=` | admin | журнал изменений `{total, items}` |

## Сборка и запуск

Требуется: Qt 6.8.3 `msvc2022_64`, VS Build Tools (C++), CMake ≥ 3.25, Ninja.

```powershell
$env:CMAKE_PREFIX_PATH = "<путь к Qt>"                   # например: F:\Dev\Qt\sdk\version\6.8.3\msvc2022_64
& .\scripts\run-vs.cmd cmake --workflow --preset ci      # конфигурация + сборка + тесты
& .\build\RegistroServer.exe --port 9080                 # запуск по умолчанию = демо-режим
& .\build\RegistroClient.exe                             # клиент заполняет форму входа (demo/demo)
& .\build\RegistroServer.exe --port 9080 --no-demo       # сервер без демо-режима (registro.db)
```

Библиотеки Qt разворачиваются рядом с каждым исполняемым файлом, поэтому приложения из `build/`
работают автономно — без настройки PATH и переменных окружения. Упаковка релиза (zip + windeployqt):
`.\scripts\release.ps1`.

## Конфигурация

**Сервер** — `registro-server.ini` рядом с исполняемым файлом (обнаруживается автоматически) или
`--config <путь>`; `--write-config <путь>` генерирует шаблон с комментариями. Приоритет:
встроенные значения по умолчанию < INI < флаги командной строки (`--port`, `--log-level`,
`--log-file`, `--db`, `--admin-user/--admin-password`, `--demo/--no-demo`). Значение
по умолчанию — демо-режим.

**Клиент** — настройки в файле `config.ini` в папке данных приложения (создаётся
автоматически): адрес сервера (меняется на форме входа), последний пользователь (логин),
«запомнить меня». Токен сохраняется только при согласии пользователя; пароль не хранится
никогда. Ключ `demo` (по умолчанию `true`) включает предзаполнение формы входа учётными
данными `demo`/`demo` и суффикс «(демо-режим)» в заголовке окна.

## Проверка

```powershell
& .\scripts\run-vs.cmd ctest --preset debug      # все тестовые наборы
```

| Критерий | Состояние |
|---|---|
| Наборы QtTest (юнит + интеграция с реальным HTTP) | зелёные |
| qmllint (строгий режим) | 0 предупреждений |
| MSVC `/W4`, ASAN, `/analyze` | 0 предупреждений; ASAN и `/analyze` зелёные |

## Структура

```
assets/        иконки (SVG/ICO), скриншоты
src/common/    общий REST-контракт и логирование (Qt::Core)
src/server/    хранилище, маршруты, конфигурация, авторизация; RegistroServer
src/client/    HTTP-контроллер, модели, настройки; QML-представления
tests/         8 наборов QtTest + проверка qmllint
scripts/       сборка, форматирование, бенчмарк, релизная упаковка
docs/          архитектурная документация
```

## Лицензия

[MIT](LICENSE)
