import { useState } from 'react'

import { Input } from '@/components/ui/input'
import { cn } from '@/lib/utils'

type Unit = 's' | 'm' | 'h'

const UNIT_SECONDS: Record<Unit, number> = { s: 1, m: 60, h: 3600 }

function defaultUnitFor(sec: number): Unit {
  if (sec !== 0 && sec % 3600 === 0) return 'h'
  if (sec !== 0 && sec % 60 === 0) return 'm'
  return 's'
}

interface DurationInputProps {
  id?: string
  valueSec: number
  onChangeSec: (sec: number) => void
  'aria-label'?: string
  className?: string
  inputClassName?: string
}

// Plan/14: задержки/паузы вводятся в секундах, минутах или часах — храним
// везде в секундах (формат сценария на устройстве, план 07), единица —
// чисто локальный UI-стейт для удобства ввода, не меняет сохранённое значение
// сама по себе (переключение "сек"->"мин" не пересчитывает уже введённое
// число секунд, просто показывает его в других единицах).
export function DurationInput({
  id,
  valueSec,
  onChangeSec,
  'aria-label': ariaLabel,
  className,
  inputClassName,
}: DurationInputProps) {
  const [unit, setUnit] = useState<Unit>(() => defaultUnitFor(valueSec))
  const displayValue = round2(valueSec / UNIT_SECONDS[unit])

  return (
    <div className={cn('flex items-center gap-1', className)}>
      <Input
        id={id}
        type="number"
        min={0}
        step="any"
        value={displayValue}
        aria-label={ariaLabel}
        onChange={(e) => {
          const n = Math.max(0, Number(e.target.value) || 0)
          onChangeSec(Math.round(n * UNIT_SECONDS[unit]))
        }}
        className={inputClassName ?? 'h-8 w-20'}
      />
      <select
        value={unit}
        onChange={(e) => setUnit(e.target.value as Unit)}
        aria-label="Единица времени"
        className="h-8 rounded-md border border-input bg-background px-1 text-xs"
      >
        <option value="s">сек</option>
        <option value="m">мин</option>
        <option value="h">ч</option>
      </select>
    </div>
  )
}

function round2(n: number): number {
  return Math.round(n * 100) / 100
}
