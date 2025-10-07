@echo off
echo === ЗАПУСК FASTREGEX В DOCKER ===
echo.

echo 1. Сборка и запуск контейнера...
docker-compose up -d fastregex

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Успешно запущено!
    echo.
    echo 2. Для входа в контейнер используйте:
    echo    docker exec -it fastregex-dev bash
    echo.
    echo 3. Или используйте Docker Dashboard
    echo.
    echo === ГОТОВО ===
) else (
    echo.
    echo Ошибка запуска!
    pause
)
