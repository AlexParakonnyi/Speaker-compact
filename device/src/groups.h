#ifndef GROUPS_H
#define GROUPS_H

#include <Arduino.h>

#include <vector>

// Живёт в корне SD, не в TRACKS_DIR — это метаданные, не аудио (см.
// CLAUDE.md §4.5.2: новые категории файлов держим вне TRACKS_DIR).
#define GROUPS_FILE "/groups.json"

struct TrackGroupAssignment {
  String track;
  String group;
};

// Трек без записи в trackGroups считается негруппированным ("Без группы") —
// см. CLAUDE.md §2.3/Plan/05. Кэш в памяти, перечитывается из SD только на
// старте; на запись — сразу saveGroups(), как с trackList в storage.cpp.
extern std::vector<String> groupList;
extern std::vector<TrackGroupAssignment> trackGroups;

// Читает GROUPS_FILE в groupList/trackGroups. Если файла ещё нет или SD не
// смонтирована — оставляет оба списка пустыми, не ошибка.
void loadGroups();

// Перезаписывает GROUPS_FILE текущим содержимым groupList/trackGroups.
bool saveGroups();

bool createGroup(const String& name);
// Переименовывает группу и все её assignments разом.
bool renameGroup(const String& from, const String& to);
// Удаляет группу; треки, которые были в ней, становятся негруппированными
// (запись из trackGroups удаляется, сам трек на SD не трогается).
bool deleteGroup(const String& name);

// group == "" — снять группу с трека (сделать негруппированным).
// Существование группы/трека — ответственность вызывающей стороны
// (servers.cpp), как и с trackNameSafe() для остальных трек-эндпоинтов.
void assignTrackGroup(const String& track, const String& group);

// Вызывать при удалении/переименовании файла трека (storage.cpp сам ничего
// не знает про группы — оркестрация в servers.cpp::processPendingCommands).
void unassignTrack(const String& track);
void renameTrackAssignment(const String& from, const String& to);
// Для clear_all — стирает все assignments, groupList (сами группы) остаётся.
void clearAllAssignments();

#endif
