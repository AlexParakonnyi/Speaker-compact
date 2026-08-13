import { Battery, BatteryFull, BatteryLow, BatteryMedium, BatteryWarning } from 'lucide-react'

interface BatteryIndicatorProps {
  valid: boolean
  voltage: number
  percent: number
}

// Делитель откалиброван по факту (device/src/battery.cpp, Plan/21) —
// напряжение точное. Процент — пока условный: диапазон рассчитан на 1S
// Li-ion (3.0-4.2В), а питание сейчас от USB (5В) — упирается в 100%,
// пересчитается сам, когда подключат реальную батарею.
export function BatteryIndicator({ valid, voltage, percent }: BatteryIndicatorProps) {
  if (!valid) {
    return (
      <span className="flex items-center gap-1 text-xs text-muted-foreground" title="Датчик батареи не отвечает">
        <BatteryWarning className="size-4" />
      </span>
    )
  }

  const Icon = percent >= 90 ? BatteryFull : percent >= 40 ? BatteryMedium : percent >= 15 ? BatteryLow : Battery
  const low = percent < 15

  return (
    <span
      className={`flex items-center gap-1 text-xs ${low ? 'text-destructive' : 'text-muted-foreground'}`}
      title="Напряжение откалибровано; процент условный, пока не подключена реальная батарея (см. Plan/21)"
    >
      <Icon className="size-4" />
      {percent}% · {voltage.toFixed(2)}В
    </span>
  )
}
