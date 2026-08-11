import { useState } from 'react'
import { Pencil, Play, Repeat, Square, Trash2 } from 'lucide-react'

import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardFooter, CardHeader, CardTitle } from '@/components/ui/card'
import { ConfirmDialog } from '@/components/ConfirmDialog'
import type { Scenario } from '@/api/device'

interface ScenarioCardProps {
  scenario: Scenario
  isActive: boolean
  onActivate: () => void
  onStop: () => void
  onEdit: () => void
  onDelete: () => Promise<void> | void
}

export function ScenarioCard({ scenario, isActive, onActivate, onStop, onEdit, onDelete }: ScenarioCardProps) {
  const [confirmingDelete, setConfirmingDelete] = useState(false)
  const totalDelaySec =
    scenario.startDelaySec + scenario.steps.reduce((sum, s) => sum + s.delayAfterPrevSec, 0)

  return (
    <Card data-testid="scenario-card">
      <CardHeader className="flex-row items-center justify-between gap-2 pb-2">
        <CardTitle className="truncate text-base" title={scenario.name}>
          {scenario.name}
        </CardTitle>
        {isActive && <Badge>Активен</Badge>}
      </CardHeader>
      <CardContent className="flex flex-wrap items-center gap-2 pb-2">
        <Badge variant="outline">{scenario.steps.length} шаг(ов)</Badge>
        <Badge variant="outline">старт через {scenario.startDelaySec}с</Badge>
        {scenario.loop && (
          <Badge variant="secondary">
            <Repeat className="mr-1 size-3" /> зациклен
          </Badge>
        )}
        <span className="text-xs text-muted-foreground">~{totalDelaySec}с суммарных задержек</span>
      </CardContent>
      <CardFooter className="flex-wrap justify-between gap-2">
        {isActive ? (
          <Button variant="secondary" size="sm" onClick={onStop}>
            <Square /> Остановить
          </Button>
        ) : (
          <Button variant="secondary" size="sm" onClick={onActivate}>
            <Play /> Активировать
          </Button>
        )}
        <div className="flex gap-1">
          <Button variant="ghost" size="icon" onClick={onEdit} aria-label="Редактировать" disabled={isActive}>
            <Pencil />
          </Button>
          <Button
            variant="ghost"
            size="icon"
            onClick={() => setConfirmingDelete(true)}
            aria-label="Удалить сценарий"
            disabled={isActive}
          >
            <Trash2 />
          </Button>
        </div>
      </CardFooter>

      <ConfirmDialog
        open={confirmingDelete}
        onOpenChange={setConfirmingDelete}
        title="Удалить сценарий?"
        description={`«${scenario.name}» будет удалён с устройства безвозвратно.`}
        onConfirm={onDelete}
      />
    </Card>
  )
}
