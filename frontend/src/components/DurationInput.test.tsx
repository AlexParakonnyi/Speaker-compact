import { useState } from 'react'
import { describe, expect, it, vi } from 'vitest'
import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'

import { DurationInput } from './DurationInput'

function Controlled({ initialSec }: { initialSec: number }) {
  const [sec, setSec] = useState(initialSec)
  return <DurationInput valueSec={sec} onChangeSec={setSec} aria-label="Задержка" />
}

describe('<DurationInput />', () => {
  it('автоматически выбирает единицу по значению — 300с показывает как 5 мин', () => {
    render(<DurationInput valueSec={300} onChangeSec={vi.fn()} aria-label="Задержка" />)
    expect(screen.getByLabelText('Задержка')).toHaveValue(5)
    expect(screen.getByLabelText(/единица времени/i)).toHaveValue('m')
  })

  it('значение, не кратное минуте/часу, показывается в секундах', () => {
    render(<DurationInput valueSec={90} onChangeSec={vi.fn()} aria-label="Задержка" />)
    expect(screen.getByLabelText(/единица времени/i)).toHaveValue('s')
    expect(screen.getByLabelText('Задержка')).toHaveValue(90)
  })

  it('ввод числа пересчитывается в секунды через текущую единицу', async () => {
    // Controlled, не голый vi.fn() — компонент управляемый (value идёт из
    // пропа), без реального стейта у родителя дисплей "отскакивает" назад
    // между нажатиями клавиш, и посимвольный ввод userEvent.type ломается.
    const user = userEvent.setup()
    render(<Controlled initialSec={300} />)
    // сейчас в минутах (300с -> 5), меняем на 10 минут
    await user.clear(screen.getByLabelText('Задержка'))
    await user.type(screen.getByLabelText('Задержка'), '10')
    expect(screen.getByLabelText('Задержка')).toHaveValue(10)
  })

  it('переключение единицы не меняет сохранённое количество секунд, только отображение', async () => {
    const user = userEvent.setup()
    render(<Controlled initialSec={300} />)
    expect(screen.getByLabelText('Задержка')).toHaveValue(5) // 5 мин

    await user.selectOptions(screen.getByLabelText(/единица времени/i), 'h')
    // 300с/3600 = 0.08333... — округляется до 2 знаков для отображения (не 300с сами по себе)
    expect(screen.getByLabelText('Задержка')).toHaveValue(0.08)
  })
})
