import { describe, expect, it, vi, beforeEach } from 'vitest'
import { render, screen, waitFor, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'

import { TracksScreen } from './TracksScreen'
import * as api from '@/api/device'

vi.mock('@/api/device', async () => {
  const actual = await vi.importActual<typeof api>('@/api/device')
  return {
    ...actual,
    getStatus: vi.fn(),
    getGroups: vi.fn(),
    getScenarios: vi.fn(),
    playTrack: vi.fn().mockResolvedValue(undefined),
    stopPlayback: vi.fn().mockResolvedValue(undefined),
    deleteTrack: vi.fn().mockResolvedValue(undefined),
    renameTrack: vi.fn().mockResolvedValue(undefined),
    clearAllTracks: vi.fn().mockResolvedValue(undefined),
    assignGroup: vi.fn().mockResolvedValue(undefined),
    uploadTrackImage: vi.fn().mockResolvedValue(undefined),
  }
})

const mockedApi = vi.mocked(api)

const baseStatus: api.DeviceStatus = {
  sdMounted: true,
  status: 'IDLE',
  currentTrack: '',
  volume: 1,
  tracks: [
    { name: 'morning.wav', size: 1024, duration: 12 },
    { name: 'evening.mp3', size: 2048, duration: 30 },
  ],
  scenarioActive: false,
  activeScenario: '',
}

const baseGroups: api.GroupsData = {
  groups: ['Утро'],
  assignments: { 'morning.wav': 'Утро' },
}

const baseScenarios: api.ScenariosData = { scenarios: [] }

beforeEach(() => {
  mockedApi.getStatus.mockResolvedValue(baseStatus)
  mockedApi.getGroups.mockResolvedValue(baseGroups)
  mockedApi.getScenarios.mockResolvedValue(baseScenarios)
})

describe('<TracksScreen />', () => {
  it('отображает список треков после загрузки', async () => {
    render(<TracksScreen />)
    expect(await screen.findByText('morning.wav')).toBeInTheDocument()
    expect(screen.getByText('evening.mp3')).toBeInTheDocument()
  })

  it('нажатие ▶ на плитке вызывает playTrack с именем файла', async () => {
    const user = userEvent.setup()
    render(<TracksScreen />)
    const card = (await screen.findByText('morning.wav')).closest('[data-testid="track-card"]') as HTMLElement
    await user.click(within(card).getByRole('button', { name: /слушать/i }))
    await waitFor(() => expect(mockedApi.playTrack).toHaveBeenCalledWith('morning.wav'))
  })

  it('удаление трека требует подтверждения в диалоге перед вызовом API', async () => {
    const user = userEvent.setup()
    render(<TracksScreen />)
    const card = (await screen.findByText('morning.wav')).closest('[data-testid="track-card"]') as HTMLElement
    await user.click(within(card).getByRole('button', { name: /удалить/i }))

    // Диалог открыт, API ещё не вызван.
    expect(await screen.findByText(/будет удалён с устройства/i)).toBeInTheDocument()
    expect(mockedApi.deleteTrack).not.toHaveBeenCalled()

    await user.click(screen.getByRole('button', { name: /^удалить$/i }))
    await waitFor(() => expect(mockedApi.deleteTrack).toHaveBeenCalledWith('morning.wav'))
  })

  it('фильтр по группе показывает только треки этой группы', async () => {
    const user = userEvent.setup()
    render(<TracksScreen />)
    await screen.findByText('morning.wav')

    await user.click(screen.getByRole('button', { name: /^утро \(1\)$/i }))

    expect(screen.getByText('morning.wav')).toBeInTheDocument()
    expect(screen.queryByText('evening.mp3')).not.toBeInTheDocument()
  })

  it('показывает ошибку устройства без падения экрана', async () => {
    mockedApi.getStatus.mockRejectedValue(new Error('Device unreachable'))
    render(<TracksScreen />)
    expect(await screen.findByText(/device unreachable/i)).toBeInTheDocument()
  })
})
