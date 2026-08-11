import { afterEach, describe, expect, it, vi } from 'vitest'

import {
  activateScenario,
  deleteTrack,
  getScenarios,
  getStatus,
  playTrack,
  renameGroup,
  saveScenario,
  trackImageUrl,
  uploadTrackImage,
  type Scenario,
} from './device'

function mockFetch(response: { ok: boolean; status?: number; json?: unknown; text?: string }) {
  return vi.fn().mockResolvedValue({
    ok: response.ok,
    status: response.status ?? (response.ok ? 200 : 400),
    statusText: response.ok ? 'OK' : 'Bad Request',
    json: async () => response.json,
    text: async () => response.text ?? '',
  })
}

describe('device API client', () => {
  afterEach(() => {
    vi.unstubAllGlobals()
  })

  it('getStatus запрашивает /api/status и возвращает распарсенный JSON', async () => {
    const fetchMock = mockFetch({ ok: true, json: { sdMounted: true, status: 'IDLE', currentTrack: '', volume: 1, tracks: [] } })
    vi.stubGlobal('fetch', fetchMock)

    const status = await getStatus()

    expect(fetchMock).toHaveBeenCalledWith('/api/status')
    expect(status.sdMounted).toBe(true)
  })

  it('playTrack шлёт POST с form-encoded телом на /api/play', async () => {
    const fetchMock = mockFetch({ ok: true })
    vi.stubGlobal('fetch', fetchMock)

    await playTrack('a.wav')

    const [url, init] = fetchMock.mock.calls[0]
    expect(url).toBe('/api/play')
    expect(init.method).toBe('POST')
    expect((init.body as URLSearchParams).get('file')).toBe('a.wav')
  })

  it('renameGroup передаёт from/to в теле запроса', async () => {
    const fetchMock = mockFetch({ ok: true })
    vi.stubGlobal('fetch', fetchMock)

    await renameGroup('Утро', 'Вечер')

    const [, init] = fetchMock.mock.calls[0]
    const body = init.body as URLSearchParams
    expect(body.get('from')).toBe('Утро')
    expect(body.get('to')).toBe('Вечер')
  })

  it('бросает ошибку с текстом ответа устройства при не-2xx', async () => {
    const fetchMock = mockFetch({ ok: false, status: 404, text: 'Unknown track' })
    vi.stubGlobal('fetch', fetchMock)

    await expect(deleteTrack('missing.wav')).rejects.toThrow('Unknown track')
  })

  it('uploadTrackImage шлёт raw body на /api/tracks/image?file=...', async () => {
    const fetchMock = mockFetch({ ok: true })
    vi.stubGlobal('fetch', fetchMock)
    const blob = new Blob(['jpeg'])

    await uploadTrackImage('a.wav', blob)

    const [url, init] = fetchMock.mock.calls[0]
    expect(url).toBe('/api/tracks/image?file=a.wav')
    expect(init.method).toBe('POST')
    expect(init.body).toBe(blob)
  })

  it('trackImageUrl строит путь по имени трека, с опциональным cache-bust', () => {
    expect(trackImageUrl('a.wav')).toBe('/images/a.wav.jpg')
    expect(trackImageUrl('a.wav', 123)).toBe('/images/a.wav.jpg?v=123')
  })

  it('getScenarios запрашивает /api/scenarios', async () => {
    const fetchMock = mockFetch({ ok: true, json: { scenarios: [] } })
    vi.stubGlobal('fetch', fetchMock)

    await getScenarios()

    expect(fetchMock).toHaveBeenCalledWith('/api/scenarios')
  })

  it('saveScenario шлёт JSON-тело с Content-Type: application/json (не form)', async () => {
    const fetchMock = mockFetch({ ok: true })
    vi.stubGlobal('fetch', fetchMock)
    const scenario: Scenario = {
      name: 'Вечер',
      startDelaySec: 60,
      loop: false,
      steps: [{ file: 'a.wav', delayAfterPrevSec: 0, volume: 1 }],
    }

    await saveScenario(scenario)

    const [url, init] = fetchMock.mock.calls[0]
    expect(url).toBe('/api/scenarios/save')
    expect(init.method).toBe('POST')
    expect(init.headers).toEqual({ 'Content-Type': 'application/json' })
    expect(JSON.parse(init.body as string)).toEqual(scenario)
  })

  it('activateScenario шлёт name form-encoded на /api/scenarios/activate', async () => {
    const fetchMock = mockFetch({ ok: true })
    vi.stubGlobal('fetch', fetchMock)

    await activateScenario('Вечер')

    const [url, init] = fetchMock.mock.calls[0]
    expect(url).toBe('/api/scenarios/activate')
    expect((init.body as URLSearchParams).get('name')).toBe('Вечер')
  })
})
