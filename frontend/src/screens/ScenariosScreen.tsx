import { useState } from 'react'
import { Plus, Square } from 'lucide-react'

import { activateScenario, deleteScenario, saveScenario, stopScenario, type Scenario } from '@/api/device'
import { Button } from '@/components/ui/button'
import { Card, CardContent } from '@/components/ui/card'
import { Skeleton } from '@/components/ui/skeleton'
import { ScenarioCard } from '@/components/ScenarioCard'
import { ScenarioEditor } from '@/components/ScenarioEditor'
import { useDeviceData } from '@/hooks/useDeviceData'

export function ScenariosScreen() {
  const { status, scenarios, error, refetch } = useDeviceData()
  const [actionError, setActionError] = useState<string | null>(null)
  // null — список, 'new' — создание, Scenario — редактирование существующего
  const [editing, setEditing] = useState<Scenario | 'new' | null>(null)

  const runAction = async (fn: () => Promise<void>) => {
    try {
      setActionError(null)
      await fn()
      await refetch()
    } catch (e) {
      setActionError(e instanceof Error ? e.message : 'Не удалось выполнить действие')
    }
  }

  if (error && !status) {
    return (
      <div className="flex min-h-svh flex-col items-center justify-center gap-3 p-4 text-center">
        <p className="text-sm text-destructive">{error}</p>
        <Button onClick={refetch}>Повторить</Button>
      </div>
    )
  }

  if (!status || !scenarios) {
    return (
      <div className="flex flex-col gap-3 p-4">
        {Array.from({ length: 3 }).map((_, i) => (
          <Skeleton key={i} className="h-32 w-full" />
        ))}
      </div>
    )
  }

  return (
    <div className="mx-auto flex max-w-2xl flex-col gap-3 p-4 pb-24">
      <header className="flex items-center justify-between">
        <h1 className="text-lg font-semibold">Сценарии</h1>
      </header>

      {(error || actionError) && (
        <p className="rounded-md bg-destructive/10 p-2 text-sm text-destructive">{actionError ?? error}</p>
      )}

      {status.scenarioActive && (
        <Card className="border-primary" data-testid="active-scenario-banner">
          <CardContent className="flex items-center justify-between gap-2 pt-4">
            <span className="text-sm">
              Активен сценарий: <strong>{status.activeScenario}</strong>
              {status.status === 'PLAYING' && status.currentTrack && ` — играет «${status.currentTrack}»`}
            </span>
            <Button size="sm" variant="secondary" onClick={() => runAction(() => stopScenario())}>
              <Square /> Остановить
            </Button>
          </CardContent>
        </Card>
      )}

      {editing !== null ? (
        <ScenarioEditor
          tracks={status.tracks}
          initial={editing === 'new' ? undefined : editing}
          onCancel={() => setEditing(null)}
          onSave={async (s) => {
            await saveScenario(s) // не через runAction — ошибка должна остаться внутри редактора, не закрывать его
            await refetch()
            setEditing(null)
          }}
        />
      ) : (
        <>
          <Button size="sm" onClick={() => setEditing('new')} disabled={status.tracks.length === 0}>
            <Plus /> Новый сценарий
          </Button>
          {status.tracks.length === 0 && (
            <p className="text-xs text-muted-foreground">Сначала загрузите хотя бы один трек на экране «Треки».</p>
          )}

          {scenarios.scenarios.length === 0 ? (
            <p className="py-8 text-center text-sm text-muted-foreground">Сценариев пока нет.</p>
          ) : (
            <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
              {scenarios.scenarios.map((s) => (
                <ScenarioCard
                  key={s.name}
                  scenario={s}
                  isActive={status.scenarioActive && status.activeScenario === s.name}
                  onActivate={() => runAction(() => activateScenario(s.name))}
                  onStop={() => runAction(() => stopScenario())}
                  onEdit={() => setEditing(s)}
                  onDelete={() => runAction(() => deleteScenario(s.name))}
                />
              ))}
            </div>
          )}
        </>
      )}
    </div>
  )
}
