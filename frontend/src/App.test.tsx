import { describe, expect, it, vi, beforeEach } from 'vitest'
import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'

import App from './App'
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
}

beforeEach(() => {
  mockedApi.getStatus.mockResolvedValue(baseStatus)
  mockedApi.getGroups.mockResolvedValue({ groups: [], assignments: {} })
  mockedApi.getScenarios.mockResolvedValue({ scenarios: [] })
})

describe('<App />', () => {
  it('по умолчанию показывает экран треков, переключается на сценарии по табу', async () => {
    const user = userEvent.setup()
    render(<App />)

    expect(await screen.findByRole('heading', { name: /треки/i })).toBeInTheDocument()

    await user.click(screen.getByRole('button', { name: /^сценарии$/i }))

    expect(await screen.findByRole('heading', { name: /сценарии/i })).toBeInTheDocument()
  })
})
