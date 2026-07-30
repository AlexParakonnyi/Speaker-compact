#ifndef SERVERS_H
#define SERVERS_H

#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;

// Регистрирует все HTTP-роуты (API + раздача фронтенда из SD /www/), поднимает сервер.
void initServers();

// Выполняет команду, поставленную HTTP-хендлером (см. global.h::pendingCmd) —
// единственное место (вместе с loop()), где трогаем SD_MMC/File для
// play/stop/delete/rename, чтобы не гоняться с AsyncTCP-задачей.
void processPendingCommands();

#endif
