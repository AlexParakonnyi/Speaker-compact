import { useState } from 'react'
import { Camera, Music, Pause, Pencil, Play, Trash2 } from 'lucide-react'

import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardFooter, CardHeader } from '@/components/ui/card'
import { Input } from '@/components/ui/input'
import { ConfirmDialog } from '@/components/ConfirmDialog'
import { TrackImagePicker } from '@/components/TrackImagePicker'
import { trackImageUrl, type Track } from '@/api/device'
import { formatDuration, formatSize } from '@/lib/format'
import { hashColor } from '@/lib/hashColor'

interface TrackCardProps {
  track: Track
  group: string // '' — без группы
  groups: string[]
  isPlaying: boolean
  isBusy: boolean
  onPlay: () => void
  onStop: () => void
  onDelete: () => Promise<void> | void
  onRename: (newName: string) => Promise<void> | void
  onAssignGroup: (group: string) => void
  onImageSave: (image: Blob) => Promise<void> | void
}

export function TrackCard({
  track,
  group,
  groups,
  isPlaying,
  isBusy,
  onPlay,
  onStop,
  onDelete,
  onRename,
  onAssignGroup,
  onImageSave,
}: TrackCardProps) {
  const [editing, setEditing] = useState(false)
  const [draftName, setDraftName] = useState(track.name)
  const [confirmingDelete, setConfirmingDelete] = useState(false)
  const [pickerOpen, setPickerOpen] = useState(false)
  const [imageBroken, setImageBroken] = useState(false)
  const [imageVersion, setImageVersion] = useState(0)

  const commitRename = () => {
    setEditing(false)
    const trimmed = draftName.trim()
    if (trimmed && trimmed !== track.name) onRename(trimmed)
    else setDraftName(track.name)
  }

  const handleImageSave = async (image: Blob) => {
    await onImageSave(image)
    setImageBroken(false)
    setImageVersion((v) => v + 1) // cache-bust: тот же путь, новый файл на устройстве
  }

  return (
    <Card data-testid="track-card">
      {/* Plan/13 п.2: заглушка без фото — не пустой квадрат, акцентный цвет
          по хэшу имени + иконка ноты, чтобы плитки не выглядели одинаково. */}
      <div
        className="relative flex aspect-video items-center justify-center overflow-hidden rounded-t-lg"
        style={imageBroken ? { backgroundColor: hashColor(track.name) } : undefined}
      >
        {!imageBroken && (
          <img
            src={trackImageUrl(track.name, imageVersion || undefined)}
            alt=""
            className="h-full w-full object-cover"
            onError={() => setImageBroken(true)}
          />
        )}
        {imageBroken && <Music className="size-8 text-white/80" />}
        <Button
          variant="secondary"
          size="icon"
          className="absolute right-2 top-2 h-8 w-8"
          onClick={() => setPickerOpen(true)}
          aria-label="Изменить фото"
        >
          <Camera />
        </Button>
      </div>

      <CardHeader className="flex-row items-center justify-between gap-2 pb-2">
        {editing ? (
          <Input
            autoFocus
            value={draftName}
            onChange={(e) => setDraftName(e.target.value)}
            onBlur={commitRename}
            onKeyDown={(e) => {
              if (e.key === 'Enter') commitRename()
              if (e.key === 'Escape') {
                setDraftName(track.name)
                setEditing(false)
              }
            }}
            className="h-8 text-sm"
          />
        ) : (
          <span className="truncate text-sm font-medium" title={track.name}>
            {track.name}
          </span>
        )}
        <Button variant="ghost" size="icon" className="h-8 w-8 shrink-0" onClick={() => setEditing(true)} aria-label="Переименовать">
          <Pencil />
        </Button>
      </CardHeader>

      <CardContent className="flex items-center gap-2 pb-2">
        <Button
          variant="secondary"
          size="icon"
          onClick={isPlaying ? onStop : onPlay}
          disabled={isBusy}
          aria-label={isPlaying ? 'Остановить' : 'Слушать'}
        >
          {isPlaying ? <Pause /> : <Play />}
        </Button>
        <Badge variant="outline">{formatDuration(track.duration)}</Badge>
        <Badge variant="outline">{formatSize(track.size)}</Badge>
      </CardContent>

      <CardFooter className="justify-between gap-2">
        <select
          className="h-9 flex-1 rounded-md border border-input bg-background px-2 text-sm"
          value={group}
          onChange={(e) => onAssignGroup(e.target.value)}
          aria-label="Группа"
        >
          <option value="">Без группы</option>
          {groups.map((g) => (
            <option key={g} value={g}>
              {g}
            </option>
          ))}
        </select>
        <Button variant="ghost" size="icon" onClick={() => setConfirmingDelete(true)} aria-label="Удалить">
          <Trash2 />
        </Button>
      </CardFooter>

      <ConfirmDialog
        open={confirmingDelete}
        onOpenChange={setConfirmingDelete}
        title="Удалить трек?"
        description={`«${track.name}» будет удалён с устройства безвозвратно.`}
        onConfirm={onDelete}
      />

      <TrackImagePicker
        open={pickerOpen}
        onOpenChange={setPickerOpen}
        trackName={track.name}
        onSave={handleImageSave}
      />
    </Card>
  )
}
