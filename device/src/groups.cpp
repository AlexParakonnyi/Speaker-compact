#include "groups.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <esp_log.h>

#include "global.h"

static const char* TAG = "groups";

std::vector<String> groupList;
std::vector<TrackGroupAssignment> trackGroups;

void loadGroups() {
  groupList.clear();
  trackGroups.clear();
  if (!sdMounted || !SD.exists(GROUPS_FILE)) return;

  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File f = SD.open(GROUPS_FILE, FILE_READ);
  if (!f) {
    xSemaphoreGive(sdMutex);
    ESP_LOGE(TAG, "SD.open failed: %s", GROUPS_FILE);
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  xSemaphoreGive(sdMutex);

  if (err) {
    ESP_LOGE(TAG, "%s parse error: %s", GROUPS_FILE, err.c_str());
    return;
  }
  for (JsonVariant g : doc["groups"].as<JsonArray>()) {
    groupList.push_back(g.as<String>());
  }
  for (JsonPair kv : doc["assignments"].as<JsonObject>()) {
    trackGroups.push_back({String(kv.key().c_str()), kv.value().as<String>()});
  }
  ESP_LOGI(TAG, "loaded: %u groups, %u assignments", (unsigned)groupList.size(),
           (unsigned)trackGroups.size());
}

bool saveGroups() {
  if (!sdMounted) return false;

  JsonDocument doc;
  JsonArray groups = doc["groups"].to<JsonArray>();
  for (const String& g : groupList) groups.add(g);
  JsonObject assignments = doc["assignments"].to<JsonObject>();
  for (const TrackGroupAssignment& a : trackGroups) assignments[a.track] = a.group;

  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File f = SD.open(GROUPS_FILE, FILE_WRITE);
  bool ok = f && serializeJson(doc, f) > 0;
  if (f) f.close();
  xSemaphoreGive(sdMutex);

  if (!ok) ESP_LOGE(TAG, "Failed to write %s", GROUPS_FILE);
  return ok;
}

bool createGroup(const String& name) {
  for (const String& g : groupList) {
    if (g == name) return false;  // уже существует
  }
  groupList.push_back(name);
  return saveGroups();
}

bool renameGroup(const String& from, const String& to) {
  int idx = -1;
  for (size_t i = 0; i < groupList.size(); i++) {
    if (groupList[i] == from) idx = (int)i;
    if (groupList[i] == to) return false;  // целевое имя уже занято
  }
  if (idx < 0) return false;
  groupList[idx] = to;
  for (TrackGroupAssignment& a : trackGroups) {
    if (a.group == from) a.group = to;
  }
  return saveGroups();
}

bool deleteGroup(const String& name) {
  int idx = -1;
  for (size_t i = 0; i < groupList.size(); i++) {
    if (groupList[i] == name) {
      idx = (int)i;
      break;
    }
  }
  if (idx < 0) return false;
  groupList.erase(groupList.begin() + idx);
  for (size_t i = 0; i < trackGroups.size();) {
    if (trackGroups[i].group == name) {
      trackGroups.erase(trackGroups.begin() + i);
    } else {
      i++;
    }
  }
  return saveGroups();
}

void assignTrackGroup(const String& track, const String& group) {
  if (group.length() == 0) {
    unassignTrack(track);
    return;
  }
  for (TrackGroupAssignment& a : trackGroups) {
    if (a.track == track) {
      a.group = group;
      saveGroups();
      return;
    }
  }
  trackGroups.push_back({track, group});
  saveGroups();
}

void unassignTrack(const String& track) {
  for (size_t i = 0; i < trackGroups.size(); i++) {
    if (trackGroups[i].track == track) {
      trackGroups.erase(trackGroups.begin() + i);
      saveGroups();
      return;
    }
  }
}

void renameTrackAssignment(const String& from, const String& to) {
  for (TrackGroupAssignment& a : trackGroups) {
    if (a.track == from) {
      a.track = to;
      saveGroups();
      return;
    }
  }
}

void clearAllAssignments() {
  if (trackGroups.empty()) return;
  trackGroups.clear();
  saveGroups();
}
