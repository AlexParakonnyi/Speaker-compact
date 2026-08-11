import { Button } from '@/components/ui/button'

interface GroupFilterChipsProps {
  groups: string[]
  counts: Record<string, number> // '' — счётчик негруппированных
  selected: string | null // null — "Все"
  onSelect: (group: string | null) => void
}

// Горизонтальный скролл чипов (Plan/13) — фильтр по группе над сеткой треков.
export function GroupFilterChips({ groups, counts, selected, onSelect }: GroupFilterChipsProps) {
  const chip = (label: string, value: string | null, count: number) => (
    <Button
      key={value ?? '__all__'}
      variant={selected === value ? 'default' : 'outline'}
      size="sm"
      className="shrink-0 rounded-full"
      onClick={() => onSelect(value)}
    >
      {label} ({count})
    </Button>
  )

  const total = Object.values(counts).reduce((a, b) => a + b, 0)

  return (
    <div className="flex gap-2 overflow-x-auto pb-1" role="tablist" aria-label="Фильтр по группе">
      {chip('Все', null, total)}
      {chip('Без группы', '', counts[''] ?? 0)}
      {groups.map((g) => chip(g, g, counts[g] ?? 0))}
    </div>
  )
}
