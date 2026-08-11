import { useState } from 'react'
import { Pencil, Plus, Trash2 } from 'lucide-react'

import { Button } from '@/components/ui/button'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import { Input } from '@/components/ui/input'
import { ConfirmDialog } from '@/components/ConfirmDialog'

interface GroupsManagerProps {
  groups: string[]
  onCreate: (name: string) => Promise<void> | void
  onRename: (from: string, to: string) => Promise<void> | void
  onDelete: (name: string) => Promise<void> | void
}

// Отдельная секция, не смешана с фильтр-чипами (GroupFilterChips) — CRUD
// групп нужен реже, чем фильтрация, не должен загромождать основной экран.
export function GroupsManager({ groups, onCreate, onRename, onDelete }: GroupsManagerProps) {
  const [newName, setNewName] = useState('')
  const [editing, setEditing] = useState<string | null>(null)
  const [draft, setDraft] = useState('')
  const [deleting, setDeleting] = useState<string | null>(null)

  const submitCreate = () => {
    const trimmed = newName.trim()
    if (!trimmed) return
    onCreate(trimmed)
    setNewName('')
  }

  const commitRename = (from: string) => {
    setEditing(null)
    const trimmed = draft.trim()
    if (trimmed && trimmed !== from) onRename(from, trimmed)
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle className="text-base">Группы</CardTitle>
      </CardHeader>
      <CardContent className="flex flex-col gap-2">
        {groups.length === 0 && <p className="text-sm text-muted-foreground">Групп пока нет.</p>}
        {groups.map((g) => (
          <div key={g} className="flex items-center gap-2">
            {editing === g ? (
              <Input
                autoFocus
                value={draft}
                onChange={(e) => setDraft(e.target.value)}
                onBlur={() => commitRename(g)}
                onKeyDown={(e) => e.key === 'Enter' && commitRename(g)}
                className="h-8 flex-1 text-sm"
              />
            ) : (
              <span className="flex-1 truncate text-sm">{g}</span>
            )}
            <Button
              variant="ghost"
              size="icon"
              className="h-8 w-8"
              aria-label={`Переименовать группу ${g}`}
              onClick={() => {
                setDraft(g)
                setEditing(g)
              }}
            >
              <Pencil />
            </Button>
            <Button
              variant="ghost"
              size="icon"
              className="h-8 w-8"
              aria-label={`Удалить группу ${g}`}
              onClick={() => setDeleting(g)}
            >
              <Trash2 />
            </Button>
          </div>
        ))}

        <div className="flex items-center gap-2 pt-2">
          <Input
            placeholder="Новая группа"
            value={newName}
            onChange={(e) => setNewName(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && submitCreate()}
            className="h-9 flex-1 text-sm"
          />
          <Button size="sm" onClick={submitCreate} disabled={!newName.trim()}>
            <Plus /> Добавить
          </Button>
        </div>
      </CardContent>

      <ConfirmDialog
        open={deleting !== null}
        onOpenChange={(open) => !open && setDeleting(null)}
        title="Удалить группу?"
        description={`Треки из «${deleting}» станут негруппированными, сами файлы не удаляются.`}
        onConfirm={() => {
          if (deleting) return onDelete(deleting)
        }}
      />
    </Card>
  )
}
