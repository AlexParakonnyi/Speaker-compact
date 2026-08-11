import { describe, expect, it } from 'vitest'

import { formatDuration, formatSize } from './format'

describe('formatDuration', () => {
  it('форматирует секунды в m:ss', () => {
    expect(formatDuration(65)).toBe('1:05')
    expect(formatDuration(5)).toBe('0:05')
    expect(formatDuration(600)).toBe('10:00')
  })

  it('показывает прочерк для несчитанной длительности (-1, сжатые форматы)', () => {
    expect(formatDuration(-1)).toBe('—')
  })
})

describe('formatSize', () => {
  it('форматирует байты в B/KB/MB в зависимости от величины', () => {
    expect(formatSize(500)).toBe('500 B')
    expect(formatSize(2048)).toBe('2.0 KB')
    expect(formatSize(5 * 1024 * 1024)).toBe('5.0 MB')
  })
})
