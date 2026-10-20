# ZanniGate Linux Node Systemd Service

Данная директория содержит systemd-сервис для запуска и управления `zanni-node-linux`.

## Особенности интеграции

- **Type=notify**: Сервис уведомляет systemd (`READY=1`), когда сервер успешно открыл и слушает TCP-порт.
- **Watchdog (`WatchdogSec=30s`)**: Поддержка аппаратного/программного сторожевого таймера systemd. Фоновый поток `node-linux` отправляет сигналы `WATCHDOG=1` каждые `WatchdogSec / 2` (15 сек). При зависании процесса systemd перезапустит узел автоматически.
- **Информативный статус (`STATUS=...`)**: `systemctl status zanni-node-linux` отображает текущее состояние, порт и последнее обработанное сообщение.
- **Корректная остановка (`STOPPING=1`)**: При получении сигналов `SIGTERM` или `SIGINT` узел отправляет уведомление `STOPPING=1`, закрывает соединения, останавливает watchdog-поток и завершает работу с кодом `0`.
- **Логирование**: Логи направляются в `journald` и файл `/var/log/zanni-gate-node/zanni-gate-node.log`. Директива `LogsDirectory=zanni-gate-node` обеспечивает автоматическое создание каталога с нужными правами доступа.

## Установка

### Быстрая установка через Makefile:
```bash
make build TARGET=linux
sudo make install-service
```

### Ручная установка:
1. Соберите бинарный файл:
   ```bash
   make build TARGET=linux
   ```
2. Скопируйте исполняемый файл:
   ```bash
   sudo install -D -m 755 bin/ZanniGateLinux /usr/local/bin/zanni-node-linux
   ```
3. Создайте каталог конфигурации и скопируйте конфигурационный файл:
   ```bash
   sudo mkdir -p /etc/zannigate
   sudo cp core/config/config.example.json /etc/zanni-node/config.json
   ```
4. Установите unit-файл сервиса:
   ```bash
   sudo install -D -m 644 node-linux/services/zanni-node-linux.service /etc/systemd/system/zanni-node-linux.service
   ```
5. Перечитайте конфигурацию systemd и активируйте сервис:
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable --now zanni-node-linux
   ```

## Управление сервисом

- **Проверка статуса:**
  ```bash
  systemctl status zanni-node-linux
  ```
- **Просмотр логов в реальном времени:**
  ```bash
  journalctl -u zanni-node-linux -f
  ```
- **Перезапуск сервиса:**
  ```bash
  sudo systemctl restart zanni-node-linux
  ```
- **Остановка сервиса:**
  ```bash
  sudo systemctl stop zanni-node-linux
  ```

## Настройка конфигурационного пути

По умолчанию сервис ищет конфиг в `/etc/zanni-node/config.json`.
Если вы хотите изменить путь без редактирования `.service` файла, создайте `/etc/default/zanni-node-linux`:
```bash
ZANNI_CONFIG=/custom/path/to/config.json
```
и перезапустите сервис:
```bash
sudo systemctl restart zanni-node-linux
```
