import { describe, expect, it, vi, beforeEach } from 'vitest'
import { renderHook, waitFor } from '@testing-library/react'

import { useDeviceData } from './useDeviceData'
import * as api from '@/api/device'

vi.mock('@/api/device', async () => {
  const actual = await vi.importActual<typeof api>('@/api/device')
  return {
    ...actual,
    getStatus: vi.fn(),
    getGroups: vi.fn(),
    getScenarios: vi.fn(),
  }
})

const mockedApi = vi.mocked(api)

const baseStatus: api.DeviceStatus = {
  sdMounted: true,
  status: 'IDLE',
  currentTrack: '',
  volume: 1,
  tracks: [],
  scenarioActive: false,
  activeScenario: '',
  batteryValid: false,
  batteryVoltage: 0,
  batteryPercent: 0,
}
const baseGroups: api.GroupsData = { groups: [], assignments: {} }
const baseScenarios: api.ScenariosData = { scenarios: [] }

beforeEach(() => {
  mockedApi.getStatus.mockResolvedValue(baseStatus)
  mockedApi.getGroups.mockResolvedValue(baseGroups)
  mockedApi.getScenarios.mockResolvedValue(baseScenarios)
})

describe('useDeviceData', () => {
  it('подтягивает status/groups/scenarios при монтировании', async () => {
    const { result } = renderHook(() => useDeviceData())

    await waitFor(() => expect(result.current.status).toEqual(baseStatus))
    expect(result.current.groups).toEqual(baseGroups)
    expect(result.current.scenarios).toEqual(baseScenarios)
    expect(result.current.error).toBeNull()
  })

  it('падение одного запроса не стирает уже полученные данные от других (allSettled, не all)', async () => {
    mockedApi.getGroups.mockRejectedValue(new Error('Network changed'))

    const { result } = renderHook(() => useDeviceData())

    await waitFor(() => expect(result.current.status).toEqual(baseStatus))
    expect(result.current.scenarios).toEqual(baseScenarios)
    expect(result.current.groups).toBeNull()
    expect(result.current.error).toBe('Network changed')
  })
})
