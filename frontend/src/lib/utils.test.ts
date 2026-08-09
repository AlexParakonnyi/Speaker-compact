import { describe, it, expect } from 'vitest'
import { cn } from './utils'

describe('cn()', () => {
  it('возвращает пустую строку без аргументов', () => {
    expect(cn()).toBe('')
  })

  it('объединяет несколько классов через пробел', () => {
    expect(cn('foo', 'bar')).toBe('foo bar')
  })

  it('пропускает falsy-значения', () => {
    expect(cn('foo', false && 'bar', undefined, null, 'baz')).toBe('foo baz')
  })

  it('мёрджит конфликтующие Tailwind-классы (последний побеждает)', () => {
    expect(cn('p-2', 'p-4')).toBe('p-4')
  })

  it('обрабатывает объект с условными классами', () => {
    expect(cn({ active: true, hidden: false })).toBe('active')
  })
})
