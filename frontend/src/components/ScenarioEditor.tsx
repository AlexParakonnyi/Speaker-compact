import { useState } from 'react'
import { Reorder } from 'framer-motion'
import { Pause, Plus } from 'lucide-react'

import { Button } from '@/components/ui/button'
import { Card, CardContent, CardFooter, CardHeader, CardTitle } from '@/components/ui/card'
import { Input } from '@/components/ui/input'
import { DurationInput } from '@/components/DurationInput'
import { ScenarioStepRow } from '@/components/ScenarioStepRow'
import type { Scenario, ScenarioStep, Track } from '@/api/device'

interface ScenarioEditorProps {
  tracks: Track[]
  initial?: Scenario // undefined — создание нового
  onSave: (scenario: Scenario) => Promise<void> | void
  onCancel: () => void
}

export function ScenarioEditor({ tracks, initial, onSave, onCancel }: ScenarioEditorProps) {
  const isEditing = initial !== undefined
  const [name, setName] = useState(initial?.name ?? '')
  const [startDelaySec, setStartDelaySec] = useState(initial?.startDelaySec ?? 0)
  const [loop, setLoop] = useState(initial?.loop ?? false)
  const [steps, setSteps] = useState<ScenarioStep[]>(initial?.steps ?? [])
  const [addTrack, setAddTrack] = useState('')
  const [error, setError] = useState<string | null>(null)
  const [saving, setSaving] = useState(false)

  const updateStepAt = (step: ScenarioStep, patch: Partial<ScenarioStep>) => {
    setSteps((prev) => prev.map((s) => (s === step ? { ...s, ...patch } : s)))
  }

  const removeStepAt = (step: ScenarioStep) => {
    setSteps((prev) => prev.filter((s) => s !== step))
  }

  const addStep = () => {
    if (!addTrack) return
    setSteps((prev) => [...prev, { file: addTrack, delayAfterPrevSec: 0, volume: 1 }])
    setAddTrack('')
  }

  // Пустой file — "чистая пауза" (по просьбе пользователя): ничего не
  // играет, устройство просто ждёт delayAfterPrevSec и идёт к следующему
  // шагу (device/src/scenario.cpp::scenarioTick). 10с по умолчанию — на
  // глаз разумное стартовое значение, тут же можно поправить.
  const addPauseStep = () => {
    setSteps((prev) => [...prev, { file: '', delayAfterPrevSec: 10, volume: 1 }])
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
          <span className="text-xs text-muted-foreground">Задержка перед стартом</span>
          <DurationInput valueSec={startDelaySec} onChangeSec={setStartDelaySec} aria-label="Задержка перед стартом" />
        </div>

        <label className="flex items-center gap-2 text-sm">
          <input type="checkbox" checked={loop} onChange={(e) => setLoop(e.target.checked)} />
          Зацикливать сценарий
        </label>
        {loop && <p className="text-xs text-muted-foreground">После последнего трека сценарий начнётся заново.</p>}

        <div className="flex flex-col gap-2">
          <span className="text-xs text-muted-foreground">Шаги ({steps.length}) — перетащить за ⠿, чтобы переставить</span>
          {steps.length === 0 && <p className="text-sm text-muted-foreground">Шагов пока нет.</p>}
          <Reorder.Group as="div" axis="y" values={steps} onReorder={setSteps} className="flex flex-col gap-2">
            {steps.map((step, i) => (
              <ScenarioStepRow
                key={i} // индекс — у ScenarioStep нет id, а file может повторяться (два пустых pause-шага и т.п.)
                step={step}
                index={i}
                onChange={(patch) => updateStepAt(step, patch)}
                onRemove={() => removeStepAt(step)}
              />
            ))}
          </Reorder.Group>
        </div>

        <div className="flex flex-wrap items-center gap-2">
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
          <Button size="sm" variant="outline" onClick={addPauseStep}>
            <Pause /> Добавить паузу
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
