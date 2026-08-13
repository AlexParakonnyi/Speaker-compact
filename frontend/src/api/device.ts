// Тонкий клиент к device API (device/src/servers.cpp). Валидация здесь —
// только UX (быстрая обратная связь), финальная — на устройстве, см.
// CLAUDE.md §4.5.4.

export interface Track {
  name: string
  size: number
  duration: number // -1, если не посчитана (сжатые форматы)
}

export interface DeviceStatus {
  sdMounted: boolean
  status: 'PLAYING' | 'IDLE'
  currentTrack: string
  volume: number
  tracks: Track[]
  // План 08 (интерим без deep sleep) — виден статус активного сценария.
  scenarioActive: boolean
  activeScenario: string
  // Новый пункт плана: индикатор батареи через ADS1115. batteryValid=false,
  // если модуль не отвечает на I2C. ⚠️ Делитель напряжения на устройстве
  // ещё не откалиброван (см. device/src/battery.cpp) — значения ориентировочные.
  batteryValid: boolean
  batteryVoltage: number
  batteryPercent: number
}

export interface GroupsData {
  groups: string[]
  assignments: Record<string, string>
}

export interface ScenarioStep {
  file: string
  delayAfterPrevSec: number
  volume: number
}

export interface Scenario {
  name: string
  startDelaySec: number
  loop: boolean
  steps: ScenarioStep[]
}

export interface ScenariosData {
  scenarios: Scenario[]
}

export const AUDIO_EXTENSIONS = ['.wav', '.mp3', '.aac', '.m4a', '.pcm']
export const MAX_UPLOAD_BYTES = 20 * 1024 * 1024

async function requestJson<T>(path: string): Promise<T> {
  const res = await fetch(path)
  if (!res.ok) throw new Error(await deviceErrorMessage(res))
  return res.json() as Promise<T>
}

async function postForm(path: string, params: Record<string, string>): Promise<void> {
  const res = await fetch(path, { method: 'POST', body: new URLSearchParams(params) })
  if (!res.ok) throw new Error(await deviceErrorMessage(res))
}

async function deviceErrorMessage(res: Response): Promise<string> {
  const text = await res.text().catch(() => '')
  return text || `${res.status} ${res.statusText}`
}

export const getStatus = () => requestJson<DeviceStatus>('/api/status')
export const getGroups = () => requestJson<GroupsData>('/api/groups')

export const playTrack = (file: string) => postForm('/api/play', { file })
export const stopPlayback = () => postForm('/api/stop', {})
export const deleteTrack = (file: string) => postForm('/api/delete', { file })
export const renameTrack = (from: string, to: string) => postForm('/api/rename', { from, to })
export const clearAllTracks = () => postForm('/api/clear_all', {})

export const createGroup = (name: string) => postForm('/api/groups/create', { name })
export const renameGroup = (from: string, to: string) => postForm('/api/groups/rename', { from, to })
export const deleteGroup = (name: string) => postForm('/api/groups/delete', { name })
// group === '' — снять группу с трека (см. device/src/groups.cpp).
export const assignGroup = (file: string, group: string) =>
  postForm('/api/tracks/assign_group', { file, group })

// XHR, не fetch — нужен onprogress для индикации загрузки крупных файлов
// по Wi-Fi (fetch с ReadableStream-прогрессом не работает одинаково везде).
export function uploadTrack(file: File, onProgress?: (pct: number) => void): Promise<void> {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest()
    xhr.open('POST', `/api/upload-audio?name=${encodeURIComponent(file.name)}`)
    xhr.upload.onprogress = (e) => {
      if (onProgress && e.lengthComputable) onProgress(Math.round((e.loaded / e.total) * 100))
    }
    xhr.onload = () => {
      if (xhr.status >= 200 && xhr.status < 300) resolve()
      else reject(new Error(xhr.responseText || `${xhr.status} ${xhr.statusText}`))
    }
    xhr.onerror = () => reject(new Error('Сетевая ошибка при загрузке'))
    xhr.send(file)
  })
}

// Plan/06: картинка уже ресайзнута на клиенте (src/lib/image.ts) — обычно
// десятки КБ, raw fetch без прогресса достаточно (в отличие от uploadTrack).
export async function uploadTrackImage(trackName: string, image: Blob): Promise<void> {
  const res = await fetch(`/api/tracks/image?file=${encodeURIComponent(trackName)}`, {
    method: 'POST',
    body: image,
  })
  if (!res.ok) throw new Error(await deviceErrorMessage(res))
}

export const getScenarios = () => requestJson<ScenariosData>('/api/scenarios')

// Единственный эндпоинт устройства с JSON-телом (не form-urlencoded) — steps
// вложенный массив, в form-параметры не укладывается (см. device/src/servers.cpp).
export async function saveScenario(scenario: Scenario): Promise<void> {
  const res = await fetch('/api/scenarios/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(scenario),
  })
  if (!res.ok) throw new Error(await deviceErrorMessage(res))
}

export const deleteScenario = (name: string) => postForm('/api/scenarios/delete', { name })

// "Активация" тут — временный минимум для проверки движка (план 08), не
// полноценный план 10 (там же погасится AP и т.д.).
export const activateScenario = (name: string) => postForm('/api/scenarios/activate', { name })
export const stopScenario = () => postForm('/api/scenarios/stop', {})

// Раздаётся статикой напрямую с устройства (device/src/servers.cpp,
// server.serveStatic("/images/", ...)) — имя файла картинки детерминировано
// из имени трека (images.cpp::imagePath), отдельного API-вызова не нужно.
// version — простой cache-bust после переливки картинки (тот же трек может
// сменить фото, а имя URL не меняется).
export function trackImageUrl(trackName: string, version?: number): string {
  const base = `/images/${encodeURIComponent(trackName)}.jpg`
  return version ? `${base}?v=${version}` : base
}
