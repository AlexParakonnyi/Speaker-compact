import { useState } from 'react'
import { ArrowDown, ArrowUp, Plus, Trash2 } from 'lucide-react'

import { Button } from '@/components/ui/button'
import { Card, CardContent, CardFooter, CardHeader, CardTitle } from '@/components/ui/card'
import { Input } from '@/components/ui/input'
import type { Scenario, ScenarioStep, Track } from '@/api/device'

interface ScenarioEditorProps {
  tracks: Track[]
  initial?: Scenario // undefined — создание нового
  onSave: (scenario: Scenario) => Promise<void> | void
  onCancel: () => void
}

// Упрощённая версия плана 14: без Framer Motion drag-and-drop (кнопки
// вверх/вниз вместо Reorder.Group) и без единиц времени "5м"/"1ч" (только
// секунды) — тот же принцип "функционал сейчас, полировка отдельным
// проходом по плану 12", что и с TrackImagePicker/ConfirmDialog.
export function ScenarioEditor({ tracks, initial, onSave, onCancel }: ScenarioEditorProps) {
  const isEditing = initial !== undefined
  const [name, setName] = useState(initial?.name ?? '')
  const [startDelaySec, setStartDelaySec] = useState(initial?.startDelaySec ?? 0)
  const [loop, setLoop] = useState(initial?.loop ?? false)
  const [steps, setSteps] = useState<ScenarioStep[]>(initial?.steps ?? [])
  const [addTrack, setAddTrack] = useState('')
  const [error, setError] = useState<string | null>(null)
  const [saving, setSaving] = useState(false)

  const updateStep = (index: number, patch: Partial<ScenarioStep>) => {
    setSteps((prev) => prev.map((s, i) => (i === index ? { ...s, ...patch } : s)))
  }

  const moveStep = (index: number, dir: -1 | 1) => {
    setSteps((prev) => {
      const target = index + dir
      if (target < 0 || target >= prev.length) return prev
      const next = [...prev]
      ;[next[index], next[target]] = [next[target], next[index]]
      return next
    })
  }

  const removeStep = (index: number) => {
    setSteps((prev) => prev.filter((_, i) => i !== index))
  }

  const addStep = () => {
    if (!addTrack) return
    setSteps((prev) => [...prev, { file: addTrack, delayAfterPrevSec: 0, volume: 1 }])
    setAddTrack('')
  }

  const handleSave = async () => {
    const trimmed = name.trim()
    if (!trimmed) {
      setError('Укажите имя сценария')
      return
    }
    if (steps.length === 0) {
      setError('Добавьте хотя бы один шаг')
      return
    }
    setError(null)
    setSaving(true)
    try {
      await onSave({ name: trimmed, startDelaySec, loop, steps })
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Не удалось сохранить сценарий')
    } finally {
      setSaving(false)
    }
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle className="text-base">{isEditing ? `Сценарий «${initial.name}»` : 'Новый сценарий'}</CardTitle>
      </CardHeader>
      <CardContent className="flex flex-col gap-3">
        <div className="flex flex-col gap-1">
          <label htmlFor="scenario-name" className="text-xs text-muted-foreground">
            Имя сценария
          </label>
          <Input
            id="scenario-name"
            value={name}
            onChange={(e) => setName(e.target.value)}
            disabled={isEditing} // переименования у устройства нет — только create/overwrite по имени
            placeholder="Вечерний"
          />
        </div>

        <div className="flex flex-col gap-1">
          <label htmlFor="scenario-start-delay" className="text-xs text-muted-foreground">
            Задержка перед стартом, сек
          </label>
          <Input
            id="scenario-start-delay"
            type="number"
            min={0}
            value={startDelaySec}
            onChange={(e) => setStartDelaySec(Math.max(0, Number(e.target.value) || 0))}
          />
        </div>

        <label className="flex items-center gap-2 text-sm">
          <input type="checkbox" checked={loop} onChange={(e) => setLoop(e.target.checked)} />
          Зацикливать сценарий
        </label>
        {loop && <p className="text-xs text-muted-foreground">После последнего трека сценарий начнётся заново.</p>}

        <div className="flex flex-col gap-2">
          <span className="text-xs text-muted-foreground">Шаги ({steps.length})</span>
          {steps.length === 0 && <p className="text-sm text-muted-foreground">Шагов пока нет.</p>}
          {steps.map((step, i) => (
            <div key={i} className="flex flex-wrap items-center gap-2 rounded-md border p-2" data-testid="scenario-step">
              <span className="min-w-0 flex-1 truncate text-sm" title={step.file}>
                {i + 1}. {step.file}
              </span>
              <label className="flex items-center gap-1 text-xs text-muted-foreground">
                Задержка, с
                <Input
                  type="number"
                  min={0}
                  value={step.delayAfterPrevSec}
                  onChange={(e) => updateStep(i, { delayAfterPrevSec: Math.max(0, Number(e.target.value) || 0) })}
                  className="h-8 w-20"
                />
              </label>
              <label className="flex items-center gap-1 text-xs text-muted-foreground">
                Громкость
                <Input
                  type="number"
                  min={0}
                  max={100}
                  value={Math.round(step.volume * 100)}
                  onChange={(e) =>
                    updateStep(i, { volume: constrain01(Number(e.target.value) / 100) })
                  }
                  className="h-8 w-16"
                />
                %
              </label>
              <Button variant="ghost" size="icon" className="h-8 w-8" onClick={() => moveStep(i, -1)} disabled={i === 0} aria-label="Выше">
                <ArrowUp />
              </Button>
              <Button
                variant="ghost"
                size="icon"
                className="h-8 w-8"
                onClick={() => moveStep(i, 1)}
                disabled={i === steps.length - 1}
                aria-label="Ниже"
              >
                <ArrowDown />
              </Button>
              <Button variant="ghost" size="icon" className="h-8 w-8" onClick={() => removeStep(i)} aria-label="Удалить шаг">
                <Trash2 />
              </Button>
            </div>
          ))}
        </div>

        <div className="flex items-center gap-2">
          <select
            className="h-9 flex-1 rounded-md border border-input bg-background px-2 text-sm"
            value={addTrack}
            onChange={(e) => setAddTrack(e.target.value)}
            aria-label="Добавить трек в сценарий"
          >
            <option value="">Выберите трек…</option>
            {tracks.map((t) => (
              <option key={t.name} value={t.name}>
                {t.name}
              </option>
            ))}
          </select>
          <Button size="sm" onClick={addStep} disabled={!addTrack}>
            <Plus /> Добавить шаг
          </Button>
        </div>

        {error && <p className="text-sm text-destructive">{error}</p>}
      </CardContent>
      <CardFooter className="justify-end gap-2">
        <Button variant="outline" onClick={onCancel} disabled={saving}>
          Отмена
        </Button>
        <Button onClick={handleSave} disabled={saving}>
          {saving ? 'Сохранение…' : 'Сохранить'}
        </Button>
      </CardFooter>
    </Card>
  )
}

function constrain01(v: number): number {
  if (Number.isNaN(v)) return 0
  return Math.min(1, Math.max(0, v))
}
