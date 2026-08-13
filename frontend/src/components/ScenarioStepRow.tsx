import { GripVertical, Pause, Trash2 } from 'lucide-react'
import { Reorder, useDragControls } from 'framer-motion'

import { Button } from '@/components/ui/button'
import { Slider } from '@/components/ui/slider'
import { DurationInput } from '@/components/DurationInput'
import type { ScenarioStep } from '@/api/device'

interface ScenarioStepRowProps {
  step: ScenarioStep
  index: number
  onChange: (patch: Partial<ScenarioStep>) => void
  onRemove: () => void
}

// Отдельный компонент (не инлайн в .map()) — нужен свой useDragControls()
// на каждый шаг (правило хуков, на верхнем уровне компонента).
export function ScenarioStepRow({ step, index, onChange, onRemove }: ScenarioStepRowProps) {
  const isPause = step.file.length === 0
  const dragControls = useDragControls()

  return (
    <Reorder.Item
      value={step}
      as="div"
      dragListener={false}
      dragControls={dragControls}
      className="flex flex-wrap items-center gap-2 rounded-md border bg-background p-2"
      data-testid="scenario-step"
    >
      {/* dragListener=false на всём Item — иначе перетаскивание начиналось бы
          с клика по инпутам/слайдеру внутри строки. Тащить можно только за ручку. */}
      <button
        type="button"
        className="cursor-grab touch-none text-muted-foreground"
        onPointerDown={(e) => dragControls.start(e)}
        aria-label={`Перетащить шаг ${index + 1}`}
      >
        <GripVertical className="size-4" />
      </button>

      <span className="flex min-w-0 flex-1 items-center gap-1 truncate text-sm" title={isPause ? 'Пауза' : step.file}>
        {isPause && <Pause className="size-3.5 shrink-0" />}
        {index + 1}. {isPause ? 'Пауза' : step.file}
      </span>

      <DurationInput
        valueSec={step.delayAfterPrevSec}
        onChangeSec={(sec) => onChange({ delayAfterPrevSec: sec })}
        aria-label={isPause ? `Длительность паузы ${index + 1}` : `Задержка шага ${index + 1}`}
      />

      {!isPause && (
        <div className="flex min-w-[150px] items-center gap-2 text-xs text-muted-foreground">
          <span className="shrink-0">Громкость</span>
          <Slider
            value={[Math.round(step.volume * 100)]}
            onValueChange={([v]) => onChange({ volume: constrain01(v / 100) })}
            min={0}
            max={100}
            step={5}
            className="w-24"
            aria-label={`Громкость шага ${index + 1}`}
          />
          <span className="w-9 shrink-0 text-right">{Math.round(step.volume * 100)}%</span>
        </div>
      )}

      <Button variant="ghost" size="icon" className="h-8 w-8" onClick={onRemove} aria-label="Удалить шаг">
        <Trash2 />
      </Button>
    </Reorder.Item>
  )
}

function constrain01(v: number): number {
  if (Number.isNaN(v)) return 0
  return Math.min(1, Math.max(0, v))
}
