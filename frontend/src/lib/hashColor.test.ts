import { describe, expect, it } from 'vitest'

import { hashColor } from './hashColor'

describe('hashColor', () => {
  it('детерминирован — одно и то же имя даёт один и тот же цвет', () => {
    expect(hashColor('morning.wav')).toBe(hashColor('morning.wav'))
  })

  it('разные имена обычно дают разные цвета', () => {
    expect(hashColor('morning.wav')).not.toBe(hashColor('evening.mp3'))
  })

  it('возвращает валидную hsl()-строку', () => {
    expect(hashColor('track.wav')).toMatch(/^hsl\(\d+, 55%, 45%\)$/)
  })
})
