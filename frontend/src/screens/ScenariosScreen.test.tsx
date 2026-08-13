import { describe, expect, it, vi, beforeEach } from 'vitest'
import { render, screen, waitFor, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'

import { ScenariosScreen } from './ScenariosScreen'
import * as api from '@/api/device'

vi.mock('@/api/device', async () => {
  const actual = await vi.importActual<typeof api>('@/api/device')
  return {
    ...actual,
    getStatus: vi.fn(),
    getGroups: vi.fn(),
    getScenarios: vi.fn(),
    saveScenario: vi.fn().mockResolvedValue(undefined),
    deleteScenario: vi.fn().mockResolvedValue(undefined),
    activateScenario: vi.fn().mockResolvedValue(undefined),
    stopScenario: vi.fn().mockResolvedValue(undefined),
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
  batteryValid: false,
  batteryVoltage: 0,
  batteryPercent: 0,
}

const baseGroups: api.GroupsData = { groups: [], assignments: {} }

const savedScenario: api.Scenario = {
  name: 'Вечер',
  startDelaySec: 60,
  loop: false,
  steps: [{ file: 'morning.wav', delayAfterPrevSec: 0, volume: 1 }],
}

beforeEach(() => {
  vi.clearAllMocks()
  mockedApi.getStatus.mockResolvedValue(baseStatus)
  mockedApi.getGroups.mockResolvedValue(baseGroups)
  mockedApi.getScenarios.mockResolvedValue({ scenarios: [savedScenario] })
})

describe('<ScenariosScreen />', () => {
  it('отображает список сохранённых сценариев', async () => {
    render(<ScenariosScreen />)
    expect(await screen.findByText('Вечер')).toBeInTheDocument()
    expect(screen.getByText(/1 шаг/i)).toBeInTheDocument()
  })

  it('показывает баннер активного сценария со кнопкой "Остановить"', async () => {
    mockedApi.getStatus.mockResolvedValue({ ...baseStatus, scenarioActive: true, activeScenario: 'Вечер' })
    const user = userEvent.setup()
    render(<ScenariosScreen />)

    const banner = await screen.findByTestId('active-scenario-banner')
    await user.click(within(banner).getByRole('button', { name: /остановить/i }))
    await waitFor(() => expect(mockedApi.stopScenario).toHaveBeenCalled())
  })

  it('"Активировать" на карточке вызывает activateScenario с именем сценария', async () => {
    const user = userEvent.setup()
    render(<ScenariosScreen />)
    const card = (await screen.findByText('Вечер')).closest('[data-testid="scenario-card"]') as HTMLElement

    await user.click(within(card).getByRole('button', { name: /активировать/i }))

    await waitFor(() => expect(mockedApi.activateScenario).toHaveBeenCalledWith('Вечер'))
  })

  it('удаление сценария требует подтверждения перед вызовом API', async () => {
    const user = userEvent.setup()
    render(<ScenariosScreen />)
    const card = (await screen.findByText('Вечер')).closest('[data-testid="scenario-card"]') as HTMLElement

    await user.click(within(card).getByRole('button', { name: /удалить сценарий/i }))
    expect(await screen.findByText(/будет удалён с устройства/i)).toBeInTheDocument()
    expect(mockedApi.deleteScenario).not.toHaveBeenCalled()

    await user.click(screen.getByRole('button', { name: /^удалить$/i }))
    await waitFor(() => expect(mockedApi.deleteScenario).toHaveBeenCalledWith('Вечер'))
  })

  it('создание сценария: имя + шаг + сохранение вызывает saveScenario с собранными данными', async () => {
    const user = userEvent.setup()
    render(<ScenariosScreen />)
    await screen.findByText('Вечер')

    await user.click(screen.getByRole('button', { name: /новый сценарий/i }))
    await user.type(screen.getByLabelText(/имя сценария/i), 'Утро')

    await user.selectOptions(screen.getByLabelText(/добавить трек в сценарий/i), 'evening.mp3')
    await user.click(screen.getByRole('button', { name: /добавить шаг/i }))
    expect(screen.getByTestId('scenario-step')).toHaveTextContent('evening.mp3')

    await user.click(screen.getByRole('button', { name: /^сохранить$/i }))

    await waitFor(() =>
      expect(mockedApi.saveScenario).toHaveBeenCalledWith(
        expect.objectContaining({
          name: 'Утро',
          steps: [{ file: 'evening.mp3', delayAfterPrevSec: 0, volume: 1 }],
        }),
      ),
    )
  })

  it('слайдер громкости шага меняет volume клавиатурой (стрелка влево = тише)', async () => {
    const user = userEvent.setup()
    render(<ScenariosScreen />)
    await screen.findByText('Вечер')

    await user.click(screen.getByRole('button', { name: /новый сценарий/i }))
    await user.type(screen.getByLabelText(/имя сценария/i), 'Тихий')
    await user.selectOptions(screen.getByLabelText(/добавить трек в сценарий/i), 'evening.mp3')
    await user.click(screen.getByRole('button', { name: /добавить шаг/i }))

    const volumeSlider = screen.getByRole('slider', { name: /громкость шага 1/i })
    volumeSlider.focus()
    await user.keyboard('{ArrowLeft}{ArrowLeft}') // step=5 — 100% -> 90%

    expect(await screen.findByText('90%')).toBeInTheDocument()

    await user.click(screen.getByRole('button', { name: /^сохранить$/i }))

    await waitFor(() =>
      expect(mockedApi.saveScenario).toHaveBeenCalledWith(
        expect.objectContaining({ steps: [{ file: 'evening.mp3', delayAfterPrevSec: 0, volume: 0.9 }] }),
      ),
    )
  })

  it('"Добавить паузу" создаёт шаг с пустым file — устройство считает это чистой паузой', async () => {
    const user = userEvent.setup()
    render(<ScenariosScreen />)
    await screen.findByText('Вечер')

    await user.click(screen.getByRole('button', { name: /новый сценарий/i }))
    await user.type(screen.getByLabelText(/имя сценария/i), 'С паузой')
    await user.click(screen.getByRole('button', { name: /добавить паузу/i }))

    const step = screen.getByTestId('scenario-step')
    expect(step).toHaveTextContent('Пауза')
    expect(screen.getByLabelText(/длительность паузы/i)).toBeInTheDocument()

    await user.click(screen.getByRole('button', { name: /^сохранить$/i }))

    await waitFor(() =>
      expect(mockedApi.saveScenario).toHaveBeenCalledWith(
        expect.objectContaining({ steps: [{ file: '', delayAfterPrevSec: 10, volume: 1 }] }),
      ),
    )
  })

  it('не даёт сохранить сценарий без шагов', async () => {
    const user = userEvent.setup()
    render(<ScenariosScreen />)
    await screen.findByText('Вечер')

    await user.click(screen.getByRole('button', { name: /новый сценарий/i }))
    await user.type(screen.getByLabelText(/имя сценария/i), 'Пустой')
    await user.click(screen.getByRole('button', { name: /^сохранить$/i }))

    expect(await screen.findByText(/добавьте хотя бы один шаг/i)).toBeInTheDocument()
    expect(mockedApi.saveScenario).not.toHaveBeenCalled()
  })
})
